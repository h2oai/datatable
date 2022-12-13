import re, collections, enum

from bashlex import flags, shutils, utils, errors, heredoc, state

sh_syntaxtab = collections.defaultdict(set)

def _addsyntax(chars, symbol):
    for c in chars:
        sh_syntaxtab[c].add(symbol)

_addsyntax('\\`$"\n', 'dquote')
_addsyntax('()<>;&|', 'meta')
_addsyntax('"`\'', 'quote')
_addsyntax('$<>', 'exp')
_addsyntax("()<>;&| \t\n", 'break')

def _shellblank(c):
    return c in ' \t'

def _shellmeta(c):
    return 'meta' in sh_syntaxtab[c]

def _shellquote(c):
    return 'quote' in sh_syntaxtab[c]

def _shellexp(c):
    return 'exp' in sh_syntaxtab[c]

def _shellbreak(c):
    return 'break' in sh_syntaxtab[c]

class tokentype(enum.Enum):
    IF = 1
    THEN = 2
    ELSE = 3
    ELIF = 4
    FI = 5
    CASE = 6
    ESAC = 7
    FOR = 8
    SELECT = 9
    WHILE = 10
    UNTIL = 11
    DO = 12
    DONE = 13
    FUNCTION = 14
    COPROC = 15
    COND_START = 16
    COND_END = 17
    # https://github.com/idank/bashlex/issues/20
    # COND_ERROR = 18
    IN = 19
    BANG = '!'
    TIME = 21
    TIMEOPT = 22
    TIMEIGN = 23
    WORD = 24
    ASSIGNMENT_WORD = 25
    REDIR_WORD = 26
    NUMBER = 27
    ARITH_CMD = 28
    ARITH_FOR_EXPRS = 29
    COND_CMD = 30
    AND_AND = '&&'
    OR_OR = '||'
    GREATER_GREATER = '>>'
    LESS_LESS = '<<'
    LESS_AND = '<&'
    LESS_LESS_LESS = '<<<'
    GREATER_AND = '>&'
    SEMI_SEMI = ';;'
    SEMI_AND = ';&'
    SEMI_SEMI_AND = ';;&'
    LESS_LESS_MINUS = '<<-'
    AND_GREATER = '&>'
    AND_GREATER_GREATER = '&>>'
    LESS_GREATER = '<>'
    GREATER_BAR = '>|'
    BAR_AND = '|&'
    LEFT_CURLY = 47
    RIGHT_CURLY = 48
    EOF = '$end'
    LEFT_PAREN = '('
    RIGHT_PAREN = ')'
    BAR = '|'
    SEMICOLON = ';'
    DASH = '-'
    NEWLINE = '\n'
    LESS = '<'
    GREATER = '>'
    AMPERSAND = '&'

_reserved = set([
    tokentype.AND_AND, tokentype.BANG, tokentype.BAR_AND, tokentype.DO,
    tokentype.DONE, tokentype.ELIF, tokentype.ELSE, tokentype.ESAC,
    tokentype.FI, tokentype.IF, tokentype.OR_OR, tokentype.SEMI_SEMI,
    tokentype.SEMI_AND, tokentype.SEMI_SEMI_AND, tokentype.THEN,
    tokentype.TIME, tokentype.TIMEOPT, tokentype.TIMEIGN, tokentype.COPROC,
    tokentype.UNTIL, tokentype.WHILE])

for c in '\n;()|&{}':
    _reserved.add(c)

# word_token_alist
valid_reserved_first_command = {
    "if" : tokentype.IF,
    "then" : tokentype.THEN,
    "else" : tokentype.ELSE,
    "elif" : tokentype.ELIF,
    "fi" : tokentype.FI,
    "case" : tokentype.CASE,
    "esac" : tokentype.ESAC,
    "for" : tokentype.FOR,
    "select" : tokentype.SELECT,
    "while" : tokentype.WHILE,
    "until" : tokentype.UNTIL,
    "do" : tokentype.DO,
    "done" : tokentype.DONE,
    "in" : tokentype.IN,
    "function" : tokentype.FUNCTION,
    "time" : tokentype.TIME,
    "{" : tokentype.LEFT_CURLY,
    "}" : tokentype.RIGHT_CURLY,
    "!" : tokentype.BANG,
    "[[" : tokentype.COND_START,
    "]]" : tokentype.COND_END,
    "coproc" : tokentype.COPROC
}

class MatchedPairError(errors.ParsingError):
    def __init__(self, startline, message, tokenizer):
        # TODO use startline?
        super(MatchedPairError, self).__init__(message,
                                               tokenizer.source,
                                               tokenizer._shell_input_line_index - 1)

wordflags = flags.word
parserflags = flags.parser

class token(object):
    def __init__(self, type_, value, pos=None, flags=None):
        if type_ is not None:
            assert isinstance(type_, tokentype)

        if flags is None:
            flags = set()

        self.ttype = type_

        self.value = value
        if pos is not None:
            self.lexpos = pos[0]
            self.endlexpos = pos[1]
            assert self.lexpos < self.endlexpos, (self.lexpos, self.endlexpos)
        else:
            self.lexpos = self.endlexpos = None

        self.flags = flags

    @property
    def type(self):
        if self.ttype:
            # make yacc see our EOF token as its own special one $end
            if self.ttype == tokentype.EOF:
                return '$end'
            else:
                return self.ttype.name

    def __nonzero__(self):
        return not (self.ttype is None and self.value is None)

    __bool__ = __nonzero__

    def __eq__(self, other):
        return isinstance(other, token) and (self.type == other.type and
                                             self.value == other.value and
                                             self.lexpos == other.lexpos and
                                             self.endlexpos == other.endlexpos and
                                             self.flags == other.flags)

    def __repr__(self):
        s = ['<', self.type]
        if self.lexpos is not None and self.endlexpos is not None:
            s.append('@%d:%d' % (self.lexpos, self.endlexpos))
        if self.value:
            s.append(' ')
            s.append(repr(self.value))

        if self.flags:
            prettyflags = ' '.join([e.name for e in self.flags])
            s.append(' (%s)' % prettyflags)
        s.append('>')
        return ''.join(s)

    def nopos(self):
        return self.__class__(self.ttype, self.value, flags=self.flags)

eoftoken = token(tokentype.EOF, None)

class tokenizer(object):
    def __init__(self, s, parserstate, strictmode=True, eoftoken=None,
                 lastreadtoken=None, tokenbeforethat=None, twotokensago=None):
        self._shell_eof_token = eoftoken
        self._shell_input_line = s
        self._added_newline = False
        if self._shell_input_line and self._shell_input_line[-1] != '\n':
            self._shell_input_line += '\n' # bash/parse.y L2431
            self._added_newline = True
        self._shell_input_line_index = 0
        # self._shell_input_line_terminator = None
        self._two_tokens_ago = twotokensago or token(None, None)
        self._token_before_that = tokenbeforethat or token(None, None)
        self._last_read_token = lastreadtoken or token(None, None)
        self._current_token = token(None, None)

        # This implements one-character lookahead/lookbehind across physical
        # input lines, to avoid something being lost because it's pushed back
        # with shell_ungetc when we're at the start of a line.
        self._eol_ungetc_lookahead = None

        # token waiting to be read
        self._token_to_read = None

        self._parserstate = parserstate
        self._line_number = 0
        self._open_brace_count = 0
        self._esacs_needed_count = 0

        self._dstack = []

        # a stack of positions to record the start and end of a token
        self._positions = []

        self._strictmode = strictmode

        # hack: the tokenizer needs access to the stack of redirection
        # nodes when it reads heredocs. this instance is shared between
        # the tokenizer and the parser, which also needs it
        self.redirstack = []

    @property
    def source(self):
        if self._added_newline:
            return self._shell_input_line[:-1]
        return self._shell_input_line

    def __iter__(self):
        while True:
            t = self.token()
            # we're finished when we see the eoftoken OR when we added a newline
            # to the input and we're there now
            if t is eoftoken or (self._added_newline and
                                 t.lexpos + 1 == len(self._shell_input_line)):
                break
            yield t

    def _createtoken(self, type_, value, flags=None):
        '''create a token with position information'''
        pos = None
        assert len(self._positions) >= 2, (type_, value)
        p2 = self._positions.pop()
        p1 = self._positions.pop()
        pos = [p1, p2]
        return token(type_, value, pos, flags)

    def token(self):
        self._two_tokens_ago, self._token_before_that, self._last_read_token = \
            self._token_before_that, self._last_read_token, self._current_token

        self._current_token = self._readtoken()
        if isinstance(self._current_token, tokentype):
            self._recordpos()
            self._current_token = self._createtoken(self._current_token,
                                                    self._current_token.value)

        if (self._parserstate & parserflags.EOFTOKEN and
            self._current_token.ttype == self._shell_eof_token):
            self._current_token = eoftoken
            # bash/parse.y L2626
        self._parserstate.discard(parserflags.EOFTOKEN)

        return self._current_token

    def _readtoken(self):
        character = None
        peek_char = None

        if self._token_to_read is not None:
            t = self._token_to_read
            self._token_to_read = None
            return t

        # bashlex/parse.y L2989 COND_COMMAND
        character = self._getc(True)
        while character is not None and _shellblank(character):
            character = self._getc(True)

        if character is None:
            return eoftoken

        if character == '#':
            self._discard_until('\n')
            self._getc(False)
            character = '\n'

        self._recordpos(1)

        if character == '\n':
            # bashlex/parse.y L3034 ALIAS
            heredoc.gatherheredocuments(self)

            self._parserstate.discard(parserflags.ASSIGNOK)
            return tokentype(character)

        if self._parserstate & parserflags.REGEXP:
            return self._readtokenword(character)

        if _shellmeta(character) and not (self._parserstate & parserflags.DBLPAREN):
            self._parserstate.discard(parserflags.ASSIGNOK)
            peek_char = self._getc(True)

            both = character
            if peek_char:
                both += peek_char
            if character == peek_char:
                if character == '<':
                    peek_char = self._getc()
                    if peek_char == '-':
                        return tokentype.LESS_LESS_MINUS
                    elif peek_char == '<':
                        return tokentype.LESS_LESS_LESS
                    else:
                        self._ungetc(peek_char)
                        return tokentype.LESS_LESS
                elif character == '>':
                    return tokentype.GREATER_GREATER
                elif character == ';':
                    self._parserstate |= parserflags.CASEPAT
                    # bashlex/parse.y L3085 ALIAS
                    peek_char = self._getc()
                    if peek_char == '&':
                        return tokentype.SEMI_SEMI_AND
                    else:
                        self._ungetc(peek_char)
                        return tokentype.SEMI_SEMI
                elif character == '&':
                    return tokentype.AND_AND
                elif character == '|':
                    return tokentype.OR_OR
                # bashlex/parse.y L3105
            elif both == '<&':
                return tokentype.LESS_AND
            elif both == '>&':
                return tokentype.GREATER_AND
            elif both == '<>':
                return tokentype.LESS_GREATER
            elif both == '>|':
                return tokentype.GREATER_BAR
            elif both == '&>':
                peek_char = self._getc()
                if peek_char == '>':
                    return tokentype.AND_GREATER_GREATER
                else:
                    self._ungetc(peek_char)
                    return tokentype.AND_GREATER
            elif both == '|&':
                return tokentype.BAR_AND
            elif both == ';&':
                return tokentype.SEMI_AND

            self._ungetc(peek_char)
            if character == ')' and self._last_read_token.value == '(' and self._token_before_that.ttype == tokentype.WORD:
                self._parserstate.add(parserflags.ALLOWOPNBRC)
                # bashlex/parse.y L3155

            if character == '(' and not self._parserstate & parserflags.CASEPAT:
                self._parserstate.add(parserflags.SUBSHELL)
            elif self._parserstate & parserflags.CASEPAT and character == ')':
                self._parserstate.discard(parserflags.CASEPAT)
            elif self._parserstate & parserflags.SUBSHELL and character == ')':
                self._parserstate.discard(parserflags.SUBSHELL)

            if character not in '<>' or peek_char != '(':
                return tokentype(character)

        if character == '-' and (self._last_read_token.ttype == tokentype.LESS_AND or self._last_read_token.ttype == tokentype.GREATER_AND):
            return tokentype(character)

        return self._readtokenword(character)

    def _readtokenword(self, c):
        d = {}
        d['all_digit_token'] = c.isdigit()
        d['dollar_present'] = d['quoted'] = d['pass_next_character'] = d['compound_assignment'] = False

        tokenword = []

        def handleshellquote():
            self._push_delimiter(c)
            try:
                ttok = self._parse_matched_pair(c, c, c, parsingcommand=(c == '`'))
            finally:
                self._pop_delimiter()

            tokenword.append(c)
            tokenword.extend(ttok)
            d['all_digit_token'] = False
            d['quoted'] = True
            if not d['dollar_present']:
                d['dollar_present'] = c == '"' and '$' in ttok

        def handleshellexp():
            peek_char = self._getc()
            if peek_char == '(' or (c == '$' and peek_char in '{['):
                # try:
                if peek_char == '{':
                    ttok = self._parse_matched_pair(cd, '{', '}', firstclose=True, dolbrace=True)
                elif peek_char == '(':
                    self._push_delimiter(peek_char)
                    ttok = self._parse_comsub(cd, '(', ')', parsingcommand=True)
                    self._pop_delimiter()
                else:
                    ttok = self._parse_matched_pair(cd, '[', ']')
                # except MatchedPairError:
                #   return -1

                tokenword.append(c)
                tokenword.append(peek_char)
                tokenword.extend(ttok)
                d['dollar_present'] = True
                d['all_digit_token'] = False

                # goto next_character
            elif c == '$' and peek_char in '\'"':
                self._push_delimiter(peek_char)
                try:
                    ttok = self._parse_matched_pair(peek_char, peek_char, peek_char,
                                                    allowesc=(peek_char == "'"))
                # except MatchedPairError:
                #    return -1
                finally:
                    self._pop_delimiter()

                #if peek_char == "'":
                #    # XXX ansiexpand
                #    ttok = shutils.single_quote(ttok)
                #else:
                #    ttok = shutils.double_quote(ttok)

                tokenword.append(c)
                tokenword.append(peek_char)
                tokenword.extend(ttok)
                d['quoted'] = True
                d['all_digit_token'] = False

                # goto next_character
            elif c == '$' and peek_char == '$':
                tokenword.append('$')
                tokenword.append('$')
                d['dollar_present'] = True
                d['all_digit_token'] = False

                # goto next_character
            else:
                self._ungetc(peek_char)
                return True

            # bashlex/parse.y L4699 ARRAY_VARS

        def handleescapedchar():
            tokenword.append(c)
            d['all_digit_token'] &= c.isdigit()
            if not d['dollar_present']:
                d['dollar_present'] = c == '$'

        while True:
            if c is None:
                break

            if d['pass_next_character']:
                d['pass_next_character'] = False
                handleescapedchar()
                # goto escaped_character
            else:
                cd = self._current_delimiter()
                gotonext = False
                if c == '\\':
                    peek_char = self._getc(False)

                    if peek_char == '\n':
                        c = '\n'
                        gotonext = True
                        # goto next_character
                    else:
                        self._ungetc(peek_char)

                        if (cd is None or cd == '`' or
                            (cd == '"' and peek_char is not None and
                             'dquote' in sh_syntaxtab[peek_char])):
                            d['pass_next_character'] = True
                            d['quoted'] = True

                            handleescapedchar()
                            gotonext = True
                            # goto got_character
                elif _shellquote(c):
                    handleshellquote()
                    gotonext = True
                    # goto next_character
                # bashlex/parse.y L4542
                # bashlex/parse.y L4567
                elif _shellexp(c):
                    gotonext = not handleshellexp()
                    # bashlex/parse.y L4699
                if not gotonext:
                    if _shellbreak(c):
                        self._ungetc(c)
                        break
                    else:
                        handleescapedchar()

            # got_character
            # got_escaped_character

            # tokenword.append(c)
            # all_digit_token &= c.isdigit()
            # if not dollar_present:
            #     dollar_present = c == '$'

            # next_character
            cd = self._current_delimiter()
            c = self._getc(cd != "'" and not d['pass_next_character'])

        # got_token
        self._recordpos()

        tokenword = ''.join(tokenword)

        if d['all_digit_token'] and (c in '<>' or self._last_read_token.ttype in (tokentype.LESS_AND, tokentype.GREATER_AND)) and shutils.legal_number(tokenword):
            return self._createtoken(tokentype.NUMBER, int(tokenword))

        # bashlex/parse.y L4811
        specialtokentype = self._specialcasetokens(tokenword)
        if specialtokentype:
            return self._createtoken(specialtokentype, tokenword)

        if not d['dollar_present'] and not d['quoted'] and self._reserved_word_acceptable(self._last_read_token):
            if tokenword in valid_reserved_first_command:
                ttype = valid_reserved_first_command[tokenword]
                ps = self._parserstate
                if ps & parserflags.CASEPAT and ttype != tokentype.ESAC:
                    pass
                elif ttype == tokentype.TIME and not self._time_command_acceptable():
                    pass
                elif ttype == tokentype.ESAC:
                    ps.discard(parserflags.CASEPAT)
                    ps.discard(parserflags.CASESTMT)
                elif ttype == tokentype.CASE:
                    ps.add(parserflags.CASESTMT)
                elif ttype == tokentype.COND_END:
                    ps.discard(parserflags.CONDCMD)
                    ps.discard(parserflags.CONDEXPR)
                elif ttype == tokentype.COND_START:
                    ps.add(parserflags.CONDCMD)
                elif ttype == tokentype.LEFT_CURLY:
                    self._open_brace_count += 1
                elif ttype == tokentype.RIGHT_CURLY and self._open_brace_count:
                    self._open_brace_count -= 1
                return self._createtoken(ttype, tokenword)

        tokenword = self._createtoken(tokentype.WORD, tokenword, utils.typedset(wordflags))
        if d['dollar_present']:
            tokenword.flags.add(wordflags.HASDOLLAR)
        if d['quoted']:
            tokenword.flags.add(wordflags.QUOTED)
        if d['compound_assignment'] and tokenword[-1] == ')':
            tokenword.flags.add(wordflags.COMPASSIGN)
        if self._is_assignment(tokenword.value, bool(self._parserstate & parserflags.COMPASSIGN)):
            tokenword.flags.add(wordflags.ASSIGNMENT)
            if self._assignment_acceptable(self._last_read_token):
                tokenword.flags.add(wordflags.NOSPLIT)
                if self._parserstate & parserflags.COMPASSIGN:
                    tokenword.flags.add(wordflags.NOGLOB)

        # bashlex/parse.y L4865
        if self._command_token_position(self._last_read_token):
            pass

        if tokenword.value[0] == '{' and tokenword.value[-1] == '}' and c in '<>':
            if shutils.legal_identifier(tokenword.value[1:]):
                # XXX is this needed?
                tokenword.value = tokenword.value[1:]
                tokenword.ttype = tokentype.REDIR_WORD

            return tokenword

        if len(tokenword.flags & set([wordflags.ASSIGNMENT, wordflags.NOSPLIT])) == 2:
            tokenword.ttype = tokentype.ASSIGNMENT_WORD

        if self._last_read_token.ttype == tokentype.FUNCTION:
            self._parserstate.add(parserflags.ALLOWOPNBRC)
            self._function_dstart = self._line_number
        elif self._last_read_token.ttype in (tokentype.CASE, tokentype.SELECT, tokentype.FOR):
            pass # bashlex/parse.y L4907

        return tokenword

    def _parse_comsub(self, doublequotes, open, close, parsingcommand=False,
                      dquote=False, firstclose=False):
        peekc = self._getc(False)
        self._ungetc(peekc)

        if peekc == '(':
            return self._parse_matched_pair(doublequotes, open, close)

        count = 1
        dollarok = True

        checkcase = bool(parsingcommand and (doublequotes is None or doublequotes not in "'\"") and not dquote)
        checkcomment = checkcase

        startlineno = self._line_number
        heredelim = ''
        stripdoc = insideheredoc = insidecomment = insideword = insidecase = False
        readingheredocdelim = False
        wasdollar = passnextchar = False
        reservedwordok = True
        lexfirstind = -1
        lexrwlen = 0

        ret = ''

        while count:
            c = self._getc(doublequotes != "'" and not insidecomment and not passnextchar)

            if c is None:
                raise MatchedPairError(startlineno, 'unexpected EOF while looking for matching %r' % close, self)

            # bashlex/parse.y L3571
            if c == '\n':
                if readingheredocdelim and heredelim:
                    readingheredocdelim = False
                    insideheredoc = True
                    lexfirstind = len(ret) + 1
                elif insideheredoc:
                    tind = lexfirstind
                    while stripdoc and ret[tind] == '\t':
                        tind += 1
                    if ret[tind:] == heredelim:
                        stripdoc = insideheredoc = False
                        heredelim = ''
                        lexfirstind = -1
                    else:
                        lexfirstind = len(ret) + 1
            # bashlex/parse.y L3599
            if insideheredoc and c == close and count == 1:
                tind = lexfirstind
                while stripdoc and ret[tind] == '\t':
                    tind += 1
                if ret[tind:] == heredelim:
                    stripdoc = insideheredoc = False
                    heredelim = ''
                    lexfirstind = -1

            if insidecomment or insideheredoc:
                ret += c

                if insidecomment and c == '\n':
                    insidecomment = False

                continue

            if passnextchar:
                passnextchar = False
                # XXX is this needed?
                # if doublequotes != "'" and c == '\n':
                #     if ret:
                #         ret = ret[:-1]
                # else:
                #     ret += c
                ret += c
                continue

            if _shellbreak(c):
                insideword = False
            else:
                if insideword:
                    lexwlen += 1
                else:
                    insideword = True
                    lexwlen = 0

            if _shellblank(c) and not readingheredocdelim and not lexrwlen:
                ret += c
                continue

            # bashlex/parse.y L3686
            if readingheredocdelim:
                if lexfirstind == -1 and not _shellbreak(c):
                    lexfirstind = len(ret)
                elif lexfirstind >= 0 and not passnextchar and _shellbreak(c):
                    if not heredelim:
                        nestret = ret[lexfirstind:]
                        heredelim = shutils.removequotes(nestret)
                    if c == '\n':
                        insideheredoc = True
                        readingheredocdelim = False
                        lexfirstind = len(ret) + 1
                    else:
                        lexfirstind = -1

            if not reservedwordok and checkcase and not insidecomment and (_shellmeta(c) or c == '\n'):
                ret += c
                peekc = self._getc(True)
                if c == peekc and c in '&|;':
                    ret += peekc
                    reservedwordok = True
                    lexrwlen = 0
                    continue
                elif c == '\n' or c in '&|;':
                    self._ungetc(peekc)
                    reservedwordok = True
                    lexrwlen = 0
                    continue
                elif c is None:
                    raise MatchedPairError(startlineno, 'unexpected EOF while looking for matching %r' % close, self) # pragma: no coverage
                else:
                    ret = ret[:-1]
                    self._ungetc(peekc)

            # bashlex/parse.y L3761
            if reservedwordok:
                if c.islower():
                    ret += c
                    lexrwlen += 1
                    continue
                elif lexrwlen == 4 and _shellbreak(c):
                    if ret[-4:] == 'case':
                        insidecase = True
                    elif ret[-4:] == 'esac':
                        insidecase = False
                    reservedwordok = False
                elif (checkcomment and c == '#' and (lexrwlen == 0 or
                        (insideword and lexwlen == 0))):
                    pass
                elif (not insidecase and (_shellblank(c) or c == '\n') and
                    lexrwlen == 2 and ret[-2:] == 'do'):
                    lexrwlen = 0
                elif insidecase and c != '\n':
                    reservedwordok = False
                elif not _shellbreak(c):
                    reservedwordok = False

            if not insidecomment and checkcase and c == '<':
                ret += c
                peekc = self._getc(True)
                if peekc is None:
                    raise MatchedPairError(startlineno, 'unexpected EOF while looking for matching %r' % close, self)
                if peekc == c:
                    ret += peekc
                    peekc = self._getc(True)
                    if peekc is None:
                        raise MatchedPairError(startlineno, 'unexpected EOF while looking for matching %r' % close, self)
                    elif peekc == '-':
                        ret += peekc
                        stripdoc = True
                    else:
                        self._ungetc(peekc)

                    if peekc != '<':
                        readingheredocdelim = True
                        lexfirstind = -1

                    continue
                else:
                    c = peekc
            elif checkcomment and not insidecomment and c == '#' and ((reservedwordok
                    and lexrwlen == 0) or insideword or lexwlen == 0):
                insidecomment = True

            if c == close and not insidecase:
                count -= 1
            elif not firstclose and not insidecase and c == open:
                count += 1

            ret += c

            if count == 0:
                break

            if c == '\\':
                passnextchar = True

            # bashlex/parse.y L3897
            if _shellquote(c):
                self._push_delimiter(c)
                try:
                    if wasdollar and c == "'":
                        nestret = self._parse_matched_pair(c, c, c,
                                                           allowesc=True,
                                                           dquote=True)
                    else:
                        nestret = self._parse_matched_pair(c, c, c,
                                                           dquote=True)
                finally:
                    self._pop_delimiter()

                # XXX is this necessary?
                # if wasdollar and c == "'" and not rdquote:
                #     if not rdquote:
                #         nestret = shutils.single_quote(nestret)
                #     ret = ret[:-2]
                # elif wasdollar and c == '"' and not rdquote:
                #     nestret = shutils.double_quote(nestret)
                #     ret = ret[:-2]

                ret += nestret
            # check for $(), $[], or ${} inside command substitution
            elif wasdollar and c in '({[':
                if not insidecase and open == c:
                    count -= 1
                if c == '(':
                    nestret = self._parse_comsub(None, '(', ')',
                                                 parsingcommand=True,
                                                 dquote=False)
                elif c == '{':
                    nestret = self._parse_matched_pair(None, '{', '}',
                                                       firstclose=True,
                                                       dolbrace=True,
                                                       dquote=True)
                elif c == '[':
                    nestret = self._parse_matched_pair(None, '[', ']',
                                                       dquote=True)

                ret += nestret

            wasdollar = c == '$'

        return ret

    def _parse_matched_pair(self, doublequotes, open, close, parsingcommand=False, allowesc=False, dquote=False, firstclose=False, dolbrace=False, arraysub=False):
        count = 1
        dolbracestate = ''
        if dolbrace:
            dolbracestate = 'param'

        insidecomment = False
        lookforcomments = False
        sawdollar = False

        if parsingcommand and doublequotes not in "`'\"" and dquote:
            lookforcomments = True

        rdquote = True if doublequotes == '"' else dquote
        passnextchar = False
        startlineno = self._line_number

        ret = ''

        def handledollarword():
            if open == c:
                count -= 1

            # bashlex/parse.y L3486
            if c == '(':
                return self._parse_comsub(None, '(', ')',
                                          parsingcommand=True,
                                          dquote=False)
            elif c == '{':
                return self._parse_matched_pair(None, '{', '}',
                                                firstclose=True,
                                                dquote=rdquote,
                                                dolbrace=True)
            elif c == '[':
                return self._parse_matched_pair(None, '[', ']', dquote=rdquote)
            else:
                assert False # pragma: no cover

        while count:
            c = self._getc(doublequotes != "'" and not passnextchar)
            if c is None:
                raise MatchedPairError(startlineno, 'unexpected EOF while looking for matching %r' % close, self)

            # bashlex/parse.y L3285
            # if c == '\n':
            #    continue

            if insidecomment:
                ret += c
                if c == '\n':
                    insidecomment = False
                continue
            elif lookforcomments and not insidecomment and c == '#' and (not ret
                    or ret[-1] == '\n' or _shellblank(ret[-1])):
                insidecomment = True

            # last char was backslash
            if passnextchar:
                passnextchar = False
                #if doublequotes != "'" and c == '\n':
                #    if ret:
                #        ret = ret[:-1]
                #    continue
                ret += c
                continue
            elif c == close:
                count -= 1
            elif open != close and sawdollar and open == '{' and c == open:
                count += 1
            elif not firstclose and c == open:
                count += 1

            ret += c
            if count == 0:
                break

            if open == "'":
                if allowesc and c == "\\":
                    passnextchar = True
                continue
            if c == "\\":
                passnextchar = True
            if dolbrace:
                if dolbracestate == 'param':
                    if len(ret) > 1:
                        dd = {'%' : 'quote', '#' : 'quote', '/' : 'quote2', '^' : 'quote',
                                ',' : 'quote'}
                        if c in dd:
                            dolbracestate = dd[c]
                    elif c in '#%^,~:-=?+/':
                        dolbracestate = 'op'
                if dolbracestate == 'op' and c in '#%^,~:-=?+/':
                    dolbracestate = 'word'

            if dolbracestate not in 'quote2' and dquote and dolbrace and c == "'":
                continue

            if open != close:
                if _shellquote(c):
                    self._push_delimiter(c)
                    try:
                        if sawdollar and "'":
                            nestret = self._parse_matched_pair(c, c, c, parsingcommand=parsingcommand, allowesc=True, dquote=dquote, firstclose=firstclose, dolbrace=dolbrace)
                        else:
                            nestret = self._parse_matched_pair(c, c, c, parsingcommand=parsingcommand, allowesc=allowesc, dquote=dquote, firstclose=firstclose, dolbrace=dolbrace)
                    finally:
                        self._pop_delimiter()

                    # bashlex/parse.y L3419
                    if sawdollar and c == "'":
                        pass
                    elif sawdollar and c == '"':
                        ret = ret[:-2] # back up before the $"

                    ret += nestret
                elif arraysub and sawdollar and c in '({[':
                    # goto parse_dollar_word
                    ret += handledollarword()
            elif open == '"' and c == '`':
                ret += self._parse_matched_pair(None, '`', '`', parsingcommand=parsingcommand, allowesc=allowesc, dquote=dquote, firstclose=firstclose, dolbrace=dolbrace)
            elif open != '`' and sawdollar and c in '({[':
                ret += handledollarword()

            sawdollar = c == '$'

        return ret


    def _is_assignment(self, value, iscompassign):
        c = value[0]

        def legalvariablechar(x):
            return x.isalpha() or x == '_'

        if not legalvariablechar(c):
            return

        for i, c in enumerate(value):
            if c == '=':
                return i

            # bash/general.c L289
            if c == '+' and i + 1 < len(value) and value[i+1] == '=':
                return i+1

            if not legalvariablechar(c):
                return False

    def _command_token_position(self, token):
        return (token.ttype == tokentype.ASSIGNMENT_WORD or
                self._parserstate & parserflags.REDIRLIST or
                (token.ttype not in (tokentype.SEMI_SEMI, tokentype.SEMI_AND, tokentype.SEMI_SEMI_AND) and self._reserved_word_acceptable(token)))

    def _assignment_acceptable(self, token):
        return self._command_token_position(token) and not self._parserstate & parserflags.CASEPAT

    def _time_command_acceptable(self):
        pass

    def _reserved_word_acceptable(self, tok):
        if not tok or (tok.ttype in _reserved or tok.value in _reserved):
            return True
        # bash/parse.y L4955 cOPROCESS_SUPPORT

        if (self._last_read_token.ttype == tokentype.WORD and
            self._token_before_that.ttype == tokentype.FUNCTION):
            return True

        return False

    def _pop_delimiter(self):
        self._dstack.pop()

    def _push_delimiter(self, c):
        self._dstack.append(c)

    def _current_delimiter(self):
        if self._dstack:
            return self._dstack[-1]

    def _ungetc(self, c):
        if (self._shell_input_line and self._shell_input_line_index
            and self._shell_input_line_index <= len(self._shell_input_line)):
            self._shell_input_line_index -= 1
        else:
            self._eol_ungetc_lookahead = c

    def _getc(self, remove_quoted_newline=True):
        if self._eol_ungetc_lookahead is not None:
            c = self._eol_ungetc_lookahead
            self._eol_ungetc_lookahead = None
            return c

        # bash/parse.y L2220

        while True:
            if self._shell_input_line_index < len(self._shell_input_line):
                c = self._shell_input_line[self._shell_input_line_index]
                self._shell_input_line_index += 1
            else:
                c = None

            if c == '\\' and remove_quoted_newline and self._shell_input_line[self._shell_input_line_index] == '\n':
                self._line_number += 1
                continue
            else:
                return c

            #if c is None and self._shell_input_line_terminator is None:
            #    if self._shell_input_line_index != 0:
            #        return '\n'
            #    else:
            #        return None

            #return c

    def _discard_until(self, character):
        c = self._getc(False)
        while c is not None and c != character:
            c = self._getc(False)
        if c is not None:
            self._ungetc(c)

    def _recordpos(self, relativeoffset=0):
        '''record the current index of the tokenizer into the positions stack
        while adding relativeoffset from it'''
        self._positions.append(self._shell_input_line_index - relativeoffset)

    def readline(self, removequotenewline):
        linebuffer = []
        passnext = indx = 0
        while True:
            c = self._getc()
            if c is None:
                if indx == 0:
                    return None
                c = '\n'

            if passnext:
                linebuffer.append(c)
                indx += 1
                passnext = False
            elif c == '\\' and removequotenewline:
                peekc = self._getc()
                if peekc == '\n':
                    self._line_number += 1
                    continue
                else:
                    self._ungetc(peekc)
                    passnext = True
                    linebuffer.append(c)
                    indx += 1
            else:
                linebuffer.append(c)
                indx += 1

            if c == '\n':
                return ''.join(linebuffer)

    def _peekc(self, *args):
        peek_char = self._getc(*args)
        # only unget if we actually read something
        if peek_char is not None:
            self._ungetc(peek_char)
        return peek_char

    def _specialcasetokens(self, tokstr):
        if (self._last_read_token.ttype == tokentype.WORD and
            self._token_before_that.ttype in (tokentype.FOR,
                                              tokentype.CASE,
                                              tokentype.SELECT) and
            tokstr == 'in'):
                if self._token_before_that.ttype == tokentype.CASE:
                    self._parserstate.add(parserflags.CASEPAT)
                    self._esacs_needed_count += 1
                return tokentype.IN

        if (self._last_read_token.ttype == tokentype.WORD and
            self._token_before_that.ttype in (tokentype.FOR, tokentype.SELECT) and
            tokstr == 'do'):
            return tokentype.DO

        if self._esacs_needed_count:
            self._esacs_needed_count -= 1
            if tokstr == 'esac':
                self._parserstate.discard(parserflags.CASEPAT)
                return tokentype.ESAC

        if self._parserstate & parserflags.ALLOWOPNBRC:
            self._parserstate.discard(parserflags.ALLOWOPNBRC)
            if tokstr == '{':
                self._open_brace_count += 1
                # bash/parse.y L2887
                return tokentype.LEFT_CURLY

        if (self._last_read_token.ttype == tokentype.ARITH_FOR_EXPRS and
            tokstr == 'do'):
            return tokentype.DO

        if (self._last_read_token.ttype == tokentype.ARITH_FOR_EXPRS and
            tokstr == '{'):
            self._open_brace_count += 1
            return tokentype.LEFT_CURLY

        if (self._open_brace_count and
            self._reserved_word_acceptable(self._last_read_token) and
            tokstr == '}'):
            self._open_brace_count -= 1
            return tokentype.RIGHT_CURLY

        if self._last_read_token.ttype == tokentype.TIME and tokstr == '-p':
            return tokentype.TIMEOPT

        if self._last_read_token.ttype == tokentype.TIMEOPT and tokstr == '--':
            return tokentype.TIMEIGN

        if self._parserstate & parserflags.CONDEXPR and tokstr == ']]':
            return tokentype.COND_END
