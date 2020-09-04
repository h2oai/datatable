
.. xmethod:: datatable.FExpr.__sub__
    :src: src/core/expr/fbinary/fexpr__sub__.cc evaluate1
    :tests: tests/expr/fbinary/test-sub.py

    __sub__(x, y)
    --

    Subtract two FExprs, which corresponds to python operation ``x - y``.

    If `x` or `y` are multi-column expressions, then they must have the
    same number of columns, and the `-` operator will be applied to each
    corresponding pair of columns. If either `x` or `y` are single-column
    while the other is multi-column, then the single-column expression
    will be repeated to the same number of columns as its opponent.

    The subtraction operation can only be applied to numeric columns. The
    resulting column will have stype equal to the larger of the stypes of its
    arguments, but at least ``int32``.


    Parameters
    ----------
    x, y: FExpr
        The arguments must be either `FExpr`s, or expressions that can be
        converted into `FExpr`s.

    return: FExpr
        An expression that evaluates ``x - y``.
