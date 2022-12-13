import copy

from bashlex import ast, flags, tokenizer, errors

def _recursiveparse(parserobj, base, sindex, tokenizerargs=None):
    # TODO: fix this hack that prevents mutual import
    from bashlex import parser

    tok = parserobj.tok

    if tokenizerargs is None:
        tokenizerargs = {'parserstate' : copy.copy(tok._parserstate),
                         'lastreadtoken' : tok._last_read_token,
                         'tokenbeforethat' : tok._token_before_that,
                         'twotokensago' : tok._two_tokens_ago}

    string = base[sindex:]
    newlimit = parserobj._expansionlimit
    if newlimit is not None:
        newlimit -= 1
    p = parser._parser(string, tokenizerargs=tokenizerargs,
                       expansionlimit=newlimit)
    node = p.parse()

    endp = node.pos[1]
    _adjustpositions(node, sindex, len(base))

    return node, endp

def _parsedolparen(parserobj, base, sindex):
    copiedps = copy.copy(parserobj.parserstate)
    copiedps.add(flags.parser.CMDSUBST)
    copiedps.add(flags.parser.EOFTOKEN)
    string = base[sindex:]

    tokenizerargs = {'eoftoken' : tokenizer.token(tokenizer.tokentype.RIGHT_PAREN, ')'),
                     'parserstate' : copiedps,
                     'lastreadtoken' : parserobj.tok._last_read_token,
                     'tokenbeforethat' : parserobj.tok._token_before_that,
                     'twotokensago' : parserobj.tok._two_tokens_ago}

    node, endp = _recursiveparse(parserobj, base, sindex, tokenizerargs)

    if string[endp] != ')':
        while endp > 0 and string[endp-1] == '\n':
            endp -= 1

    return node, sindex + endp

def _extractcommandsubst(parserobj, string, sindex, sxcommand=False):
    if string[sindex] == '(':
        raise NotImplementedError('arithmetic expansion')
        #return _extractdelimitedstring(parserobj, string, sindex, '$(', '(', '(', sxcommand=True)
    else:
        node, si = _parsedolparen(parserobj, string, sindex)
        si += 1
        return ast.node(kind='commandsubstitution', command=node, pos=(sindex-2, si)), si

def _extractprocesssubst(parserobj, string, sindex):
    #return _extractdelimitedstring(tok, string, sindex, starter, '(', ')', sxcommand=True)
    node, si = _parsedolparen(parserobj, string, sindex)
    return node, si + 1

#def _extractdelimitedstring(parserobj, string, sindex, opener, altopener, closer,
#                            sxcommand=False):
#    parts = []
#    incomment = False
#    passchar = False
#    nestinglevel = 1
#    i = sindex

#    while nestinglevel:
#        if i >= len(string):
#            break
#        c = string[i]
#        if incomment:
#            if c == '\n':
#                incomment = False
#            i += 1
#            continue
#        elif passchar:
#            passchar = False
#            i += 1
#            continue

#        if sxcommand and c == '#' and (i == 0 or string[i-1] == '\n' or
#                                       tokenizer._shellblank(string[i-1])):
#            incomment = True
#            i += 1
#            continue

#        if c == '\\':
#            passchar = True
#            i += 1
#            continue

#        if sxcommand and string[i:i+2] == '$(':
#            si = i + 2
#            node, si = _extractcommandsubst(parserobj, string, si, sxcommand=sxcommand)
#            parts.append(node)
#            i = si + 1
#            continue

#        if string.startswith(opener, i):
#            si = i + len(opener)
#            nodes, si = _extractdelimitedstring(parserobj, string, si, opener, altopener,
#                                                closer, sxcommand=sxcommand)
#            parts.extend(nodes)
#            i = si + 1
#            continue

#        if string.startswith(altopener, i):
#            si = i + len(altopener)
#            nodes, si = _extractdelimitedstring(parserobj, string, si, altopener, altopener,
#                                                closer, sxcommand=sxcommand)
#            parts.extend(nodes)
#            i = si + 1
#            continue

#        # 1327
#        if string.startswith(closer, i):
#            i += len(closer) - 1
#            nestinglevel -= 1
#            if nestinglevel == 0:
#                break

#        if c == '`':
#            si = i + 1
#            t = _stringextract(string, si, '`', sxcommand=sxcommand)
#            i = si + 1
#            continue

#        if c in "'\"":
#            si = i +1
#            if c == '"':
#                i = _skipsinglequoted(string, si)
#            else:
#                i = _skipdoublequoted(string, si)
#            continue

#        i += 1

#    if i == len(string) and nestinglevel:
#        raise errors.ParsingError('bad substitution: no closing %r in %s' % (closer, string))

#    return parts, i

def _paramexpand(parserobj, string, sindex):
    node = None
    zindex = sindex + 1
    c = string[zindex] if zindex < len(string) else None
    if c and c in '0123456789$#?-!*@':
        # XXX 7685
        node = ast.node(kind='parameter', value=c,
                        pos=(sindex, zindex+1))
    elif c == '{':
        # XXX 7863
        # TODO not start enough, doesn't consider escaping
        zindex = string.find('}', zindex + 1)
        node = ast.node(kind='parameter', value=string[sindex+2:zindex],
                        pos=(sindex, zindex+1))
        # TODO
        # return _parameterbraceexpand(string, zindex)
    elif c == '(':
        return _extractcommandsubst(parserobj, string, zindex + 1)
    elif c == '[':
        raise NotImplementedError('arithmetic substitution')
        #return _extractarithmeticsubst(string, zindex + 1)
    else:
        tindex = zindex
        for zindex in range(tindex, len(string) + 1):
            if zindex == len(string):
                break
            if not string[zindex].isalnum() and not string[zindex] == '_':
                break
        temp1 = string[sindex:zindex]
        if temp1:
            return (ast.node(kind='parameter', value=temp1[1:], pos=(sindex, zindex)),
                    zindex)

    if zindex < len(string):
        zindex += 1

    return node, zindex

def _adjustpositions(node_, base, endlimit):
    class v(ast.nodevisitor):
        def visitnode(self, node):
            assert node.pos[1] + base <= endlimit
            node.pos = (node.pos[0] + base, node.pos[1] + base)
    visitor = v()
    visitor.visit(node_)

def _expandwordinternal(parserobj, wordtoken, qheredocument, qdoublequotes, quoted, isexp):
    # bash/subst.c L8132
    istring = ''
    parts = []
    tindex = [0]
    sindex = [0]
    string = wordtoken.value
    def nextchar():
        sindex[0] += 1
        if sindex[0] < len(string):
            return string[sindex[0]]
    def peekchar():
        if sindex[0]+1 < len(string):
            return string[sindex[0]+1]

    while True:
        if sindex[0] == len(string):
            break
            # goto finished_with_string
        c = string[sindex[0]]
        if c in '<>':
            if (nextchar() != '(' or qheredocument or qdoublequotes or
                (wordtoken.flags & set([flags.word.DQUOTE, flags.word.NOPROCSUB]))):
                sindex[0] -= 1

                # goto add_character
                sindex[0] += 1
                istring += c
            else:
                tindex = sindex[0] + 1

                node, sindex[0] = _extractprocesssubst(parserobj, string, tindex)

                parts.append(ast.node(kind='processsubstitution', command=node,
                                      pos=(tindex - 2, sindex[0])))
                istring += string[tindex - 2:sindex[0]]
                # goto dollar_add_string
        # TODO
        # elif c == '=':
        #     pass
        # elif c == ':':
        #     pass
        elif c == '~':
            if (wordtoken.flags & set([flags.word.NOTILDE, flags.word.DQUOTE]) or
                (sindex[0] > 0 and not (wordtoken.flags & flags.word.NOTILDE)) or
                qdoublequotes or qheredocument):
                wordtoken.flags.clear()
                wordtoken.flags.add(flags.word.ITILDE)
                sindex[0] += 1
                istring += c
            else:
                stopatcolon = wordtoken.flags & set([flags.word.ASSIGNRHS,
                                                    flags.word.ASSIGNMENT,
                                                    flags.word.TILDEEXP])
                expand = True
                for i in range(sindex[0], len(string)):
                    r = string[i]
                    if r == '/':
                        break
                    if r in "\\'\"":
                        expand = False
                        break
                    if stopatcolon and r == ':':
                        break
                else:
                    # go one past the end if we didn't exit early
                    i += 1

                if i > sindex[0] and expand:
                    node = ast.node(kind='tilde', value=string[sindex[0]:i],
                                    pos=(sindex[0], i))
                    parts.append(node)
                istring += string[sindex[0]:i]
                sindex[0] = i

        elif c == '$' and len(string) > 1:
            tindex = sindex[0]
            node, sindex[0] = _paramexpand(parserobj, string, sindex[0])
            if node:
                parts.append(node)
            istring += string[tindex:sindex[0]]
        elif c == '`':
            tindex = sindex[0]
            # bare instance of ``
            if nextchar() == '`':
                sindex[0] += 1
                istring += '``'
            else:
                x = _stringextract(string, sindex[0], "`")
                if x == -1:
                    raise errors.ParsingError('bad substitution: no closing "`" '
                                              'in %s' % string)
                else:
                    if wordtoken.flags & flags.word.NOCOMSUB:
                        pass
                    else:
                        sindex[0] = x

                        word = string[tindex+1:sindex[0]]
                        command, ttindex = _recursiveparse(parserobj, word, 0)
                        _adjustpositions(command, tindex+1, len(string))
                        ttindex += 1 # ttindex is on the closing char

                        # assert sindex[0] == ttindex
                        # go one past the closing `
                        sindex[0] += 1

                        node = ast.node(kind='commandsubstitution',
                                        command=command,
                                        pos=(tindex, sindex[0]))
                        parts.append(node)
                        istring += string[tindex:sindex[0]]

        elif c == '\\':
            istring += string[sindex[0]+1:sindex[0]+2]
            sindex[0] += 2
        elif c == '"':
            sindex[0] += 1
            continue

            # 8513
            #if qdoublequotes or qheredocument:
            #    sindex[0] += 1
            #else:
            #    tindex = sindex[0] + 1
            #    parts, sindex[0] = _stringextractdoublequoted(string, sindex[0])
            #    if tindex == 1 and sindex[0] == len(string):
            #        quotedstate = 'wholly'
            #    else:
            #        quotedstate = 'partially'

        elif c == "'":
            # entire string surronded by single quotes, no expansion is
            # going to happen
            if sindex[0] == 0 and string[-1] == "'":
                return [], string[1:-1]

            # check if we're inside double quotes
            if not qdoublequotes:
                # look for the closing ', we know we have one or otherwise
                # this wouldn't tokenize due to unmatched '
                tindex = sindex[0]
                sindex[0] = string.find("'", sindex[0]) + 1

                istring += string[tindex+1:sindex[0]-1]
            else:
                # this is a single quote inside double quotes, add it
                istring += c
                sindex[0] += 1
        else:
            istring += string[sindex[0]:sindex[0]+1]
            sindex[0] += 1

    if parts:
        class v(ast.nodevisitor):
            def visitnode(self, node):
                assert node.pos[1] + wordtoken.lexpos <= wordtoken.endlexpos
                node.pos = (node.pos[0] + wordtoken.lexpos,
                            node.pos[1] + wordtoken.lexpos)
        visitor = v()
        for node in parts:
            visitor.visit(node)

    return parts, istring

def _stringextract(string, sindex, charlist, sxvarname=False):
    found = False
    i = sindex
    while i < len(string):
        c = string[i]
        if c == '\\':
            if i + 1 < len(string):
                i += 1
            else:
                break
        elif sxvarname and c == '[':
            ni = _skipsubscript(string, i, 0)
            if string[ni] == ']':
                i = ni
        elif c in charlist:
            found = True
            break
        else:
            i += 1
    if found:
        return i
    else:
        return -1
