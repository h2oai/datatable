import enum

parser = enum.Enum('parserflags', [
    'CASEPAT', # in a case pattern list
    'ALEXPNEXT', # expand next word for aliases
    'ALLOWOPNBRC', # allow open brace for function def
    'NEEDCLOSBRC', # need close brace
    'DBLPAREN', # double-paren parsing
    'SUBSHELL', # ( ... ) subshell
    'CMDSUBST', # $( ... ) command substitution
    'CASESTMT', # parsing a case statement
    'CONDCMD', # parsing a [[...]] command
    'CONDEXPR', # parsing the guts of [[...]]
    'ARITHFOR', # parsing an arithmetic for command - unused
    'ALEXPAND', # OK to expand aliases - unused
    'EXTPAT', # parsing an extended shell pattern
    'COMPASSIGN', # parsing x=(...) compound assignment
    'ASSIGNOK', # assignment statement ok in this context
    'EOFTOKEN', # yylex checks against shell_eof_token
    'REGEXP', # parsing an ERE/BRE as a single word
    'HEREDOC', # reading body of here-document
    'REPARSE', # re-parsing in parse_string_to_word_list
    'REDIRLIST', # parsing a list of redirections preceding a simple command name
    ])

word = enum.Enum('wordflags', [
    'HASDOLLAR', # Dollar sign present
    'QUOTED', # Some form of quote character is present
    'ASSIGNMENT', # This word is a variable assignment
    'SPLITSPACE', # Split this word on " " regardless of IFS
    'NOSPLIT', # Do not perform word splitting on this word because ifs is empty string
    'NOGLOB', # Do not perform globbing on this word
    'NOSPLIT2', # Don't split word except for $@ expansion (using spaces) because context does not allow it
    'TILDEEXP', # Tilde expand this assignment word
    'DOLLARAT', # $@ and its special handling
    'DOLLARSTAR', # $* and its special handling
    'NOCOMSUB', # Don't perform command substitution on this word
    'ASSIGNRHS', # Word is rhs of an assignment statement
    'NOTILDE', # Don't perform tilde expansion on this word
    'ITILDE', # Internal flag for word expansion
    'NOEXPAND', # Don't expand at all -- do quote removal
    'COMPASSIGN', # Compound assignment
    'ASSNBLTIN', # word is a builtin command that takes assignments
    'ASSIGNARG', # word is assignment argument to command
    'HASQUOTEDNULL', # word contains a quoted null character
    'DQUOTE', # word should be treated as if double-quoted
    'NOPROCSUB', # don't perform process substitution
    'HASCTLESC', # word contains literal CTLESC characters
    'ASSIGNASSOC', # word looks like associative array assignment
    'ASSIGNARRAY', # word looks like a compound indexed array assignment
    'ARRAYIND', # word is an array index being expanded
    'ASSNGLOBAL', # word is a global assignment to declare (declare/typeset -g)
    'NOBRACE', # Don't perform brace expansion
    'ASSIGNINT', # word is an integer assignment to declare
    ])
