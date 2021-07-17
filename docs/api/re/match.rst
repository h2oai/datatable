
.. xfunction:: datatable.re.match
    :src: src/core/expr/re/fexpr_match.cc fn_match
    :tests: tests/re/test-match.py
    :cvar: doc_re_match
    :signature: match(column, pattern)

    Test whether values in a string column match a regular expression.


    Parameters
    ----------
    column: FExpr[str]
        The column expression where you want to search for regular
        expression matches.

    pattern: str
        The regular expression that will be tested against each value
        in the `column`.

    return: FExpr[bool8]
        A boolean column that tells whether the value in each row of
        `column` matches the `pattern` or not.
