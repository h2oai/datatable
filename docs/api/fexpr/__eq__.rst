
.. xmethod:: datatable.FExpr.__eq__
    :src: src/core/expr/fbinary/fexpr__eq__.cc FExpr__eq__::evaluate1

    __eq__(x, y)
    --

    Compare whether values in columns `x` and `y` are equal.

    Like all other FExpr operators, the equality operator is elementwise:
    it produces a column where each element is the result of comparison
    ``x[i] == y[i]``.

    If `x` or `y` are multi-column expressions, then they must have the
    same number of columns, and the `==` operator will be applied to each
    corresponding pair of columns. If either `x` or `y` are single-column
    while the other is multi-column, then the single-column expression
    will be repeated to the same number of columns as its opponent.

    The equality operator can be applied to columns of any type, and the
    types of ``x`` and ``y`` are allowed to be different. In the latter
    case the columns will be converted into a common stype before the
    comparison. In practice it means, for example, that ``1 == "1"`.

    Lastly, the comparison ``x == None`` is exactly equivalent to the
    :func:`isna() <dt.math.isna>` function.


    Parameters
    ----------
    x, y: FExpr
        The arguments must be either `FExpr`s, or expressions that can be
        converted into `FExpr`s.

    return: FExpr
        An expression that evaluates ``x == y``. The produced column will
        have stype ``bool8``.
