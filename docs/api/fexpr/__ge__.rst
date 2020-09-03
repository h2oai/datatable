
.. xmethod:: datatable.FExpr.__ge__
    :src: src/core/expr/fbinary/fexpr__ge__.cc FExpr__ge__::evaluate1

    __ge__(x, y)
    --

    Compare whether ``x >= y``.

    Like all other FExpr operators, the greater-than-or-equal operator is
    elementwise: it produces a column where each element is the result of
    comparison ``x[i] >= y[i]``.

    If `x` or `y` are multi-column expressions, then they must have the
    same number of columns, and the ``>=`` operator will be applied to each
    corresponding pair of columns. If either `x` or `y` are single-column
    while the other is multi-column, then the single-column expression
    will be repeated to the same number of columns as its opponent.

    The greater-than-or-equal operator can be applied to columns of any type,
    and the types of ``x`` and ``y`` are allowed to be different. In the
    latter case the columns will be converted into a common stype before the
    comparison.


    Parameters
    ----------
    x, y: FExpr
        The arguments must be either `FExpr`s, or expressions that can be
        converted into `FExpr`s.

    return: FExpr
        An expression that evaluates ``x >= y``. The produced column will
        have stype ``bool8``.
