
.. xmethod:: datatable.FExpr.__and__
    :src: src/core/expr/fbinary/bitwise_and_or_xor.cc PyFExpr::nb__and__

    __and__(x, y)
    --

    Compute bitwise AND of `x` and `y`.

    If `x` or `y` are multi-column expressions, then they must have the
    same number of columns, and the `&` operator will be applied to each
    corresponding pair of columns. If either `x` or `y` are single-column
    while the other is multi-column, then the single-column expression
    will be repeated to the same number of columns as its opponent.

    The AND operator can only be applied to integer or boolean columns.
    The resulting column will have stype equal to the larger of the stypes
    of its arguments.

    When both `x` and `y` are boolean, then the bitwise AND operator is
    equivalent to logical AND. This can be used to combine several logical
    conditions into a compound (since Python doesn't allow overloading of
    operator ``and``). Beware, however, that ``&`` has higher precedence
    than ``and``, so it is advisable to always use parentheses::

        >>> DT[(f.x >= 0) & (f.x <= 1), :]

    Parameters
    ----------
    x, y: FExpr
        The arguments must be either `FExpr`s, or expressions that can be
        converted into `FExpr`s.

    return: FExpr
        An expression that evaluates ``x & y``.


    Notes
    -----

    .. note::

        Use ``x & y`` in order to AND two boolean `FExpr`s. Using standard
        Python keyword ``and`` will result in an error.

