
.. xmethod:: datatable.FExpr.__xor__
    :src: src/core/expr/fbinary/bitwise_and_or_xor.cc PyFExpr::nb__xor__

    __xor__(x, y)
    --

    Compute bitwise XOR of `x` and `y`.

    If `x` or `y` are multi-column expressions, then they must have the
    same number of columns, and the ``^`` operator will be applied to each
    corresponding pair of columns. If either `x` or `y` are single-column
    while the other is multi-column, then the single-column expression
    will be repeated to the same number of columns as its opponent.

    The XOR operator can only be applied to integer or boolean columns.
    The resulting column will have stype equal to the larger of the stypes
    of its arguments.

    When both `x` and `y` are boolean, then the bitwise XOR operator is
    equivalent to logical XOR. This can be used to combine several logical
    conditions into a compound (since Python doesn't allow overloading of
    operator ``xor``). Beware, however, that ``^`` has higher precedence
    than ``xor``, so it is advisable to always use parentheses::

        >>> DT[(f.x == 0) ^ (f.y == 0), :]

    Parameters
    ----------
    x, y: FExpr
        The arguments must be either `FExpr`s, or expressions that can be
        converted into `FExpr`s.

    return: FExpr
        An expression that evaluates ``x ^ y``.
