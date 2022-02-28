
.. xfunction:: datatable.re.match
    :src: src/core/expr/re/fexpr_match.cc fn_match
    :tests: tests/re/test-match.py
    :cvar: doc_re_match
    :signature: match(column, pattern, icase=False)

    Test whether values in a string column match a regular expression.


    Parameters
    ----------
    column: FExpr[str]
        The column expression where you want to search for regular
        expression matches.

    pattern: str
        The regular expression that will be tested against each value
        in the `column`.


    icase: bool

        .. x-version-added:: 1.1.0

        If `True`, character matching will be performed without regard to case.

    return: FExpr[bool8]
        A boolean column that tells whether the value in each row of
        `column` matches the `pattern` or not.

