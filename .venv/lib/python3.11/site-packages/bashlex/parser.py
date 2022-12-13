import os, copy

from bashlex import yacc, tokenizer, state, ast, subst, flags, errors, heredoc

def _partsspan(parts):
    return parts[0].pos[0], parts[-1].pos[1]

tokens = [e.name for e in tokenizer.tokentype]
precedence = (
    ('left', 'AMPERSAND', 'SEMICOLON', 'NEWLINE', 'EOF'),
    ('left', 'AND_AND', 'OR_OR'),
    ('right', 'BAR', 'BAR_AND')
)

def p_inputunit(p):
    '''inputunit : simple_list simple_list_terminator
                 | NEWLINE
                 | error NEWLINE
                 | EOF'''
    # XXX
    if p.lexer._parserstate & flags.parser.CMDSUBST:
        p.lexer._parserstate.add(flags.parser.EOFTOKEN)

    if isinstance(p[1], ast.node):
        p[0] = p[1]
        # accept right here in case the input contains more lines that are
        # not part of the current command
        p.accept()

def p_word_list(p):
    '''word_list : WORD
                 | word_list WORD'''
    parserobj = p.context
    if len(p) == 2:
        p[0] = [_expandword(parserobj, p.slice[1])]
    else:
        p[0] = p[1]
        p[0].append(_expandword(parserobj, p.slice[2]))

def p_redirection_heredoc(p):
    '''redirection : LESS_LESS WORD
                   | NUMBER LESS_LESS WORD
                   | REDIR_WORD LESS_LESS WORD
                   | LESS_LESS_MINUS WORD
                   | NUMBER LESS_LESS_MINUS WORD
                   | REDIR_WORD LESS_LESS_MINUS WORD'''
    parserobj = p.context
    assert isinstance(parserobj, _parser)

    output = ast.node(kind='word', word=p[len(p)-1], parts=[],
                      pos=p.lexspan(len(p)-1))
    if len(p) == 3:
        p[0] = ast.node(kind='redirect', input=None, type=p[1], heredoc=None,
                        output=output, pos=(p.lexpos(1), p.endlexpos(2)))
    else:
        p[0] = ast.node(kind='redirect', input=p[1], type=p[2], heredoc=None,
                        output=output, pos=(p.lexpos(1), p.endlexpos(3)))

    if p.slice[len(p)-2].ttype == tokenizer.tokentype.LESS_LESS:
        parserobj.redirstack.append((p[0], False))
    else:
        parserobj.redirstack.append((p[0], True))

def p_redirection(p):
    '''redirection : GREATER WORD
                   | LESS WORD
                   | NUMBER GREATER WORD
                   | NUMBER LESS WORD
                   | REDIR_WORD GREATER WORD
                   | REDIR_WORD LESS WORD
                   | GREATER_GREATER WORD
                   | NUMBER GREATER_GREATER WORD
                   | REDIR_WORD GREATER_GREATER WORD
                   | GREATER_BAR WORD
                   | NUMBER GREATER_BAR WORD
                   | REDIR_WORD GREATER_BAR WORD
                   | LESS_GREATER WORD
                   | NUMBER LESS_GREATER WORD
                   | REDIR_WORD LESS_GREATER WORD
                   | LESS_LESS_LESS WORD
                   | NUMBER LESS_LESS_LESS WORD
                   | REDIR_WORD LESS_LESS_LESS WORD
                   | LESS_AND NUMBER
                   | NUMBER LESS_AND NUMBER
                   | REDIR_WORD LESS_AND NUMBER
                   | GREATER_AND NUMBER
                   | NUMBER GREATER_AND NUMBER
                   | REDIR_WORD GREATER_AND NUMBER
                   | LESS_AND WORD
                   | NUMBER LESS_AND WORD
                   | REDIR_WORD LESS_AND WORD
                   | GREATER_AND WORD
                   | NUMBER GREATER_AND WORD
                   | REDIR_WORD GREATER_AND WORD
                   | GREATER_AND DASH
                   | NUMBER GREATER_AND DASH
                   | REDIR_WORD GREATER_AND DASH
                   | LESS_AND DASH
                   | NUMBER LESS_AND DASH
                   | REDIR_WORD LESS_AND DASH
                   | AND_GREATER WORD
                   | AND_GREATER_GREATER WORD'''
    parserobj = p.context
    if len(p) == 3:
        output = p[2]
        if p.slice[2].ttype == tokenizer.tokentype.WORD:
            output = _expandword(parserobj, p.slice[2])
        p[0] = ast.node(kind='redirect', input=None, type=p[1], heredoc=None,
                        output=output, pos=(p.lexpos(1), p.endlexpos(2)))
    else:
        output = p[3]
        if p.slice[3].ttype == tokenizer.tokentype.WORD:
            output = _expandword(parserobj, p.slice[3])
        p[0] = ast.node(kind='redirect', input=p[1], type=p[2], heredoc=None,
                        output=output, pos=(p.lexpos(1), p.endlexpos(3)))

def _expandword(parser, tokenword):
    if parser._expansionlimit == -1:
        # we enter this branch in the following conditions:
        # - currently parsing a substitution as a result of an expansion
        # - the previous expansion had limit == 0
        #
        # this means that this node is a descendant of a substitution in an
        # unexpanded word and will be filtered in the limit == 0 condition below
        #
        # (the reason we even expand when limit == 0 is to get quote removal)
        node = ast.node(kind='word', word=tokenword,
                        pos=(tokenword.lexpos, tokenword.endlexpos), parts=[])
        return node
    else:
        quoted = bool(tokenword.flags & flags.word.QUOTED)
        doublequoted = quoted and tokenword.value[0] == '"'

        # TODO set qheredocument
        parts, expandedword = subst._expandwordinternal(parser,
                                                        tokenword, 0,
                                                        doublequoted, 0, 0)

        # limit reached, don't include substitutions (still expanded to get
        # quote removal though)
        if parser._expansionlimit == 0:
            parts = [node for node in parts if 'substitution' not in node.kind]

        node = ast.node(kind='word', word=expandedword,
                        pos=(tokenword.lexpos, tokenword.endlexpos), parts=parts)
        return node

def p_simple_command_element(p):
    '''simple_command_element : WORD
                              | ASSIGNMENT_WORD
                              | redirection'''
    if isinstance(p[1], ast.node):
        p[0] = [p[1]]
        return

    parserobj = p.context
    p[0] = [_expandword(parserobj, p.slice[1])]

    # change the word node to an assignment if necessary
    if p.slice[1].ttype == tokenizer.tokentype.ASSIGNMENT_WORD:
        p[0][0].kind = 'assignment'

def p_redirection_list(p):
    '''redirection_list : redirection
                        | redirection_list redirection'''
    if len(p) == 2:
        p[0] = [p[1]]
    else:
        p[0] = p[1]
        p[0].append(p[2])

def p_simple_command(p):
    '''simple_command : simple_command_element
                      | simple_command simple_command_element'''

    p[0] = p[1]
    if len(p) == 3:
        p[0].extend(p[2])

def p_command(p):
    '''command : simple_command
               | shell_command
               | shell_command redirection_list
               | function_def
               | coproc'''
    if isinstance(p[1], ast.node):
        p[0] = p[1]
        if len(p) == 3:
            assert p[0].kind == 'compound'
            p[0].redirects.extend(p[2])
            assert p[0].pos[0] < p[0].redirects[-1].pos[1]
            p[0].pos = (p[0].pos[0], p[0].redirects[-1].pos[1])
    else:
        p[0] = ast.node(kind='command', parts=p[1], pos=_partsspan(p[1]))

def p_shell_command(p):
    '''shell_command : for_command
                     | case_command
                     | WHILE compound_list DO compound_list DONE
                     | UNTIL compound_list DO compound_list DONE
                     | select_command
                     | if_command
                     | subshell
                     | group_command
                     | arith_command
                     | cond_command
                     | arith_for_command'''
    if len(p) == 2:
        p[0] = p[1]
    else:
        # while or until
        assert p[2].kind == 'list'

        parts = _makeparts(p)
        kind = parts[0].word
        assert kind in ('while', 'until')
        p[0] = ast.node(kind='compound',
                        redirects=[],
                        list=[ast.node(kind=kind, parts=parts, pos=_partsspan(parts))],
                        pos=_partsspan(parts))

    assert p[0].kind == 'compound'

def _makeparts(p):
    parts = []
    for i in range(1, len(p)):
        if isinstance(p[i], ast.node):
            parts.append(p[i])
        elif isinstance(p[i], list):
            parts.extend(p[i])
        elif isinstance(p.slice[i], tokenizer.token):
            if p.slice[i].ttype == tokenizer.tokentype.WORD:
                parserobj = p.context
                parts.append(_expandword(parserobj, p.slice[i]))
            else:
                parts.append(ast.node(kind='reservedword', word=p[i],
                                      pos=p.lexspan(i)))
        else:
            pass

    return parts

def p_for_command(p):
    '''for_command : FOR WORD newline_list DO compound_list DONE
                   | FOR WORD newline_list LEFT_CURLY compound_list RIGHT_CURLY
                   | FOR WORD SEMICOLON newline_list DO compound_list DONE
                   | FOR WORD SEMICOLON newline_list LEFT_CURLY compound_list RIGHT_CURLY
                   | FOR WORD newline_list IN word_list list_terminator newline_list DO compound_list DONE
                   | FOR WORD newline_list IN word_list list_terminator newline_list LEFT_CURLY compound_list RIGHT_CURLY
                   | FOR WORD newline_list IN list_terminator newline_list DO compound_list DONE
                   | FOR WORD newline_list IN list_terminator newline_list LEFT_CURLY compound_list RIGHT_CURLY'''
    parts = _makeparts(p)
    # find the operatornode that we might have there due to
    # list_terminator/newline_list and convert it to a reservedword so its
    # considered as part of the for loop
    for i, part in enumerate(parts):
        if part.kind == 'operator' and part.op == ';':
            parts[i] = ast.node(kind='reservedword', word=';', pos=part.pos)
            break # there could be only one in there...

    p[0] = ast.node(kind='compound',
                    redirects=[],
                    list=[ast.node(kind='for', parts=parts, pos=_partsspan(parts))],
                    pos=_partsspan(parts))

def p_arith_for_command(p):
    '''arith_for_command : FOR ARITH_FOR_EXPRS list_terminator newline_list DO compound_list DONE
                         | FOR ARITH_FOR_EXPRS list_terminator newline_list LEFT_CURLY compound_list RIGHT_CURLY
                         | FOR ARITH_FOR_EXPRS DO compound_list DONE
                         | FOR ARITH_FOR_EXPRS LEFT_CURLY compound_list RIGHT_CURLY'''
    raise NotImplementedError('arithmetic for')

def p_select_command(p):
    '''select_command : SELECT WORD newline_list DO list DONE
                      | SELECT WORD newline_list LEFT_CURLY list RIGHT_CURLY
                      | SELECT WORD SEMICOLON newline_list DO list DONE
                      | SELECT WORD SEMICOLON newline_list LEFT_CURLY list RIGHT_CURLY
                      | SELECT WORD newline_list IN word_list list_terminator newline_list DO list DONE
                      | SELECT WORD newline_list IN word_list list_terminator newline_list LEFT_CURLY list RIGHT_CURLY'''
    raise NotImplementedError('select command')

def p_case_command(p):
    '''case_command : CASE WORD newline_list IN newline_list ESAC
                    | CASE WORD newline_list IN case_clause_sequence newline_list ESAC
                    | CASE WORD newline_list IN case_clause ESAC'''
    raise NotImplementedError ('case command')

def p_function_def(p):
    '''function_def : WORD LEFT_PAREN RIGHT_PAREN newline_list function_body
                    | FUNCTION WORD LEFT_PAREN RIGHT_PAREN newline_list function_body
                    | FUNCTION WORD newline_list function_body'''
    parts = _makeparts(p)
    body = parts[-1]
    name = parts[ast.findfirstkind(parts, 'word')]

    p[0] = ast.node(kind='function', name=name, body=body, parts=parts,
                    pos=_partsspan(parts))

def p_function_body(p):
    '''function_body : shell_command
                     | shell_command redirection_list'''
    assert p[1].kind == 'compound'

    p[0] = p[1]
    if len(p) == 3:
        p[0].redirects.extend(p[2])
        assert p[0].pos[0] < p[0].redirects[-1].pos[1]
        p[0].pos = (p[0].pos[0], p[0].redirects[-1].pos[1])

def p_subshell(p):
    '''subshell : LEFT_PAREN compound_list RIGHT_PAREN'''
    lparen = ast.node(kind='reservedword', word=p[1], pos=p.lexspan(1))
    rparen = ast.node(kind='reservedword', word=p[3], pos=p.lexspan(3))
    parts = [lparen, p[2], rparen]
    p[0] = ast.node(kind='compound', list=parts, redirects=[],
                    pos=_partsspan(parts))

def p_coproc(p):
    '''coproc : COPROC shell_command
              | COPROC shell_command redirection_list
              | COPROC WORD shell_command
              | COPROC WORD shell_command redirection_list
              | COPROC simple_command'''
    raise NotImplementedError('coproc')

def p_if_command(p):
    '''if_command : IF compound_list THEN compound_list FI
                  | IF compound_list THEN compound_list ELSE compound_list FI
                  | IF compound_list THEN compound_list elif_clause FI'''
    # we currently don't distinguish the various lists that make up the
    # command, because it's not needed later on. if there will be a need
    # we can always add different nodes for elif/else.
    parts = _makeparts(p)
    p[0] = ast.node(kind='compound',
                    redirects=[],
                    list=[ast.node(kind='if', parts=parts, pos=_partsspan(parts))],
                    pos=_partsspan(parts))

def p_group_command(p):
    '''group_command : LEFT_CURLY compound_list RIGHT_CURLY'''
    lcurly = ast.node(kind='reservedword', word=p[1], pos=p.lexspan(1))
    rcurly = ast.node(kind='reservedword', word=p[3], pos=p.lexspan(3))
    parts = [lcurly, p[2], rcurly]
    p[0] = ast.node(kind='compound', list=parts, redirects=[],
                    pos=_partsspan(parts))

def p_arith_command(p):
    '''arith_command : ARITH_CMD'''
    raise NotImplementedError('arithmetic command')

def p_cond_command(p):
    '''cond_command : COND_START COND_CMD COND_END'''
    raise NotImplementedError('cond command')

def p_elif_clause(p):
    '''elif_clause : ELIF compound_list THEN compound_list
                   | ELIF compound_list THEN compound_list ELSE compound_list
                   | ELIF compound_list THEN compound_list elif_clause'''
    parts = []
    for i in range(1, len(p)):
        if isinstance(p[i], ast.node):
            parts.append(p[i])
        else:
            parts.append(ast.node(kind='reservedword', word=p[i], pos=p.lexspan(i)))
    p[0] = parts

def p_case_clause(p):
    '''case_clause : pattern_list
                   | case_clause_sequence pattern_list'''
    raise NotImplementedError('case clause')

def p_pattern_list(p):
    '''pattern_list : newline_list pattern RIGHT_PAREN compound_list
                    | newline_list pattern RIGHT_PAREN newline_list
                    | newline_list LEFT_PAREN pattern RIGHT_PAREN compound_list
                    | newline_list LEFT_PAREN pattern RIGHT_PAREN newline_list'''
    raise NotImplementedError('pattern list')

def p_case_clause_sequence(p):
    '''case_clause_sequence : pattern_list SEMI_SEMI
                            | case_clause_sequence pattern_list SEMI_SEMI
                            | pattern_list SEMI_AND
                            | case_clause_sequence pattern_list SEMI_AND
                            | pattern_list SEMI_SEMI_AND
                            | case_clause_sequence pattern_list SEMI_SEMI_AND'''
    raise NotImplementedError('case clause')

def p_pattern(p):
    '''pattern : WORD
               | pattern BAR WORD'''
    raise NotImplementedError('pattern')

def p_list(p):
    '''list : newline_list list0'''
    p[0] = p[2]

def p_compound_list(p):
    '''compound_list : list
                     | newline_list list1'''
    if len(p) == 2:
        p[0] = p[1]
    else:
        parts = p[2]
        if len(parts) > 1:
            p[0] = ast.node(kind='list', parts=parts, pos=_partsspan(parts))
        else:
            p[0] = parts[0]

def p_list0(p):
    '''list0 : list1 NEWLINE newline_list
             | list1 AMPERSAND newline_list
             | list1 SEMICOLON newline_list'''
    parts = p[1]
    if len(parts) > 1 or p.slice[2].ttype != tokenizer.tokentype.NEWLINE:
        parts.append(ast.node(kind='operator', op=p[2], pos=p.lexspan(2)))
        p[0] = ast.node(kind='list', parts=parts, pos=_partsspan(parts))
    else:
        p[0] = parts[0]

def p_list1(p):
    '''list1 : list1 AND_AND newline_list list1
             | list1 OR_OR newline_list list1
             | list1 AMPERSAND newline_list list1
             | list1 SEMICOLON newline_list list1
             | list1 NEWLINE newline_list list1
             | pipeline_command'''
    if len(p) == 2:
        p[0] = [p[1]]
    else:
        p[0] = p[1]
        # XXX newline
        p[0].append(ast.node(kind='operator', op=p[2], pos=p.lexspan(2)))
        p[0].extend(p[len(p) - 1])

def p_simple_list_terminator(p):
    '''simple_list_terminator : NEWLINE
                              | EOF'''
    pass

def p_list_terminator(p):
    '''list_terminator : NEWLINE
                       | SEMICOLON
                       | EOF'''
    if p[1] == ';':
        p[0] = ast.node(kind='operator', op=';', pos=p.lexspan(1))

def p_newline_list(p):
    '''newline_list : empty
                    | newline_list NEWLINE'''
    pass

def p_simple_list(p):
    '''simple_list : simple_list1
                   | simple_list1 AMPERSAND
                   | simple_list1 SEMICOLON'''
    tok = p.lexer
    heredoc.gatherheredocuments(tok)

    if len(p) == 3 or len(p[1]) > 1:
        parts = p[1]
        if len(p) == 3:
            parts.append(ast.node(kind='operator', op=p[2], pos=p.lexspan(2)))
        p[0] = ast.node(kind='list', parts=parts, pos=_partsspan(parts))
    else:
        assert len(p[1]) == 1
        p[0] = p[1][0]

    if (len(p) == 2 and p.lexer._parserstate & flags.parser.CMDSUBST and
            p.lexer._current_token.nopos() == p.lexer._shell_eof_token):
        # accept the input
        p.accept()

def p_simple_list1(p):
    '''simple_list1 : simple_list1 AND_AND newline_list simple_list1
                    | simple_list1 OR_OR newline_list simple_list1
                    | simple_list1 AMPERSAND simple_list1
                    | simple_list1 SEMICOLON simple_list1
                    | pipeline_command'''
    if len(p) == 2:
        p[0] = [p[1]]
    else:
        p[0] = p[1]
        p[0].append(ast.node(kind='operator', op=p[2], pos=p.lexspan(2)))
        p[0].extend(p[len(p) - 1])

def p_pipeline_command(p):
    '''pipeline_command : pipeline
                        | BANG pipeline_command
                        | timespec pipeline_command
                        | timespec list_terminator
                        | BANG list_terminator'''
    if len(p) == 2:
        if len(p[1]) == 1:
            p[0] = p[1][0]
        else:
            p[0] = ast.node(kind='pipeline', parts=p[1],
                            pos=(p[1][0].pos[0], p[1][-1].pos[1]))
    else:
        # XXX timespec
        node = ast.node(kind='reservedword', word='!', pos=p.lexspan(1))
        if p[2].kind == 'pipeline':
            p[0] = p[2]
            p[0].parts.insert(0, node)
            p[0].pos = (p[0].parts[0].pos[0], p[0].parts[-1].pos[1])
        else:
            p[0] = ast.node(kind='pipeline', parts=[node, p[2]],
                            pos=(node.pos[0], p[2].pos[1]))

def p_pipeline(p):
    '''pipeline : pipeline BAR newline_list pipeline
                | pipeline BAR_AND newline_list pipeline
                | command'''
    if len(p) == 2:
        p[0] = [p[1]]
    else:
        p[0] = p[1]
        p[0].append(ast.node(kind='pipe', pipe=p[2], pos=p.lexspan(2)))
        p[0].extend(p[len(p) - 1])

def p_timespec(p):
    '''timespec : TIME
                | TIME TIMEOPT
                | TIME TIMEOPT TIMEIGN'''
    raise NotImplementedError('time command')

def p_empty(p):
    '''empty :'''
    pass

def p_error(p):
    assert isinstance(p, tokenizer.token)

    if p.ttype == tokenizer.tokentype.EOF:
        raise errors.ParsingError('unexpected EOF',
                                  p.lexer.source,
                                  len(p.lexer.source))
    else:
        raise errors.ParsingError('unexpected token %r' % p.value,
                                  p.lexer.source, p.lexpos)

yaccparser = yacc.yacc(tabmodule='bashlex.parsetab',
              outputdir=os.path.dirname(__file__),
              debug=False)

# some hack to fix yacc's reduction on command substitutions:
# which state to fix is derived from static transition tables
# as states are changeable among python versions and architectures
# the only state that is considered fixed is the initial state: 0
def get_correction_states():
    reduce = yaccparser.goto[0]['simple_list'] #~10
    state2 = yaccparser.action[reduce]['NEWLINE'] #63
    state1 = yaccparser.goto[reduce]['simple_list_terminator'] #~10
    return state1, state2

def get_correction_rightparen_states():
    state1 = yaccparser.goto[0]['pipeline_command']
    state2 = yaccparser.goto[0]['simple_list1'] #11
    state_temp = yaccparser.action[state2]['SEMICOLON'] #65
    state3 = yaccparser.goto[state_temp]['simple_list1']
    return state1, state2, state3

for tt in tokenizer.tokentype:
    states = get_correction_states()
    yaccparser.action[states[0]][tt.name] = -1
    yaccparser.action[states[1]][tt.name] = -141

states = get_correction_rightparen_states()
yaccparser.action[states[0]]['RIGHT_PAREN'] = -155
yaccparser.action[states[1]]['RIGHT_PAREN'] = -148
yaccparser.action[states[2]]['RIGHT_PAREN'] = -154

def parsesingle(s, strictmode=True, expansionlimit=None, convertpos=False):
    '''like parse, but only consumes a single top level node, e.g. parsing
    'a\nb' will only return a node for 'a', leaving b unparsed'''
    p = _parser(s, strictmode=strictmode, expansionlimit=expansionlimit)
    tree = p.parse()
    if convertpos:
        ast.posconverter(s).visit(tree)
    return tree

def parse(s, strictmode=True, expansionlimit=None, convertpos=False):
    '''parse the input string, returning a list of nodes

    top level node kinds are:

    - command - a simple command
    - pipeline - a series of simple commands
    - list - a series of one or more pipelines
    - compound - contains constructs for { list; }, (list), if, for..

    leafs are word nodes (which in turn can also contain any of the
    aforementioned nodes due to command substitutions).

    when strictmode is set to False, we will:
    - skip reading a heredoc if we're at the end of the input

    expansionlimit is used to limit the amount of recursive parsing done due to
    command substitutions found during word expansion.
    '''
    p = _parser(s, strictmode=strictmode, expansionlimit=expansionlimit)
    parts = [p.parse()]

    class endfinder(ast.nodevisitor):
        def __init__(self):
            self.end = -1
        def visitheredoc(self, node, value):
            self.end = node.pos[1]

    # find the 'real' end incase we have a heredoc in there
    ef = _endfinder()
    ef.visit(parts[-1])
    index = max(parts[-1].pos[1], ef.end) + 1
    while index < len(s):
        part = _parser(s[index:], strictmode=strictmode).parse()

        if not isinstance(part, ast.node):
            break

        ast.posshifter(index).visit(part)
        parts.append(part)
        ef = _endfinder()
        ef.visit(parts[-1])
        index = max(parts[-1].pos[1], ef.end) + 1

    if convertpos:
        for tree in parts:
            ast.posconverter(s).visit(tree)

    return parts

def split(s):
    '''a utility function that mimics shlex.split but handles more
    complex shell constructs such as command substitutions inside words

    >>> list(split('a b"c"\\'d\\''))
    ['a', 'bcd']
    >>> list(split('a "b $(c)" $(d) \\'$(e)\\''))
    ['a', 'b $(c)', '$(d)', '$(e)']
    >>> list(split('a b\\n'))
    ['a', 'b', '\\n']
    '''
    p = _parser(s)
    for t in p.tok:
        if t.ttype == tokenizer.tokentype.WORD:
            quoted = bool(t.flags & flags.word.QUOTED)
            doublequoted = quoted and t.value[0] == '"'
            parts, expandedword = subst._expandwordinternal(p, t, 0,
                                                            doublequoted, 0, 0)
            yield expandedword
        else:
            yield s[t.lexpos:t.endlexpos]

class _parser(object):
    '''
    this class is mainly used to provide context to the productions
    when we're in the middle of parsing. as a hack, we shove it into the
    YaccProduction context attribute to make it accessible.
    '''
    def __init__(self, s, strictmode=True, expansionlimit=None, tokenizerargs=None):
        assert expansionlimit is None or isinstance(expansionlimit, int)

        self.s = s
        self._strictmode = strictmode
        self._expansionlimit = expansionlimit

        if tokenizerargs is None:
            tokenizerargs = {}
        self.parserstate = tokenizerargs.pop('parserstate', state.parserstate())

        self.tok = tokenizer.tokenizer(s,
                                       parserstate=self.parserstate,
                                       strictmode=strictmode,
                                       **tokenizerargs)

        self.redirstack = self.tok.redirstack

    def parse(self):
        # yacc.yacc returns a parser object that is not reentrant, it has
        # some mutable state. we make a shallow copy of it so no
        # state spills over to the next call to parse on it
        theparser = copy.copy(yaccparser)
        tree = theparser.parse(lexer=self.tok, context=self)

        return tree

class _endfinder(ast.nodevisitor):
    '''helper class to find the "real" end pos of a node that contains
    a heredoc. this is a hack because heredoc aren't really part of any node
    since they don't always follow the end of a node and might appear on
    a different line'''
    def __init__(self):
        self.end = -1
    def visitheredoc(self, node, value):
        self.end = node.pos[1]
