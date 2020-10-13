
.. xmethod:: datatable.FExpr.__truediv__
    :src: src/core/expr/fbinary/fexpr__truediv__.cc evaluate1
    :tests: tests/expr/fbinary/test-truediv.py

    __truediv__(x, y)
    --

    Divide two FExprs, which corresponds to python operation ``x / y``.

    If `x` or `y` are multi-column expressions, then they must have the
    same number of columns, and the `/` operator will be applied to each
    corresponding pair of columns. If either `x` or `y` are single-column
    while the other is multi-column, then the single-column expression
    will be repeated to the same number of columns as its opponent.

    The division operation can only be applied to numeric columns. The
    resulting column will have stype ``float64`` in all cases except when both
    arguments have stype ``float32`` (in which case the result is also
    ``float32``).


    Parameters
    ----------
    x, y: FExpr
        The arguments must be either `FExpr`s, or expressions that can be
        converted into `FExpr`s.

    return: FExpr
        An expression that evaluates `x / y`.


    See also
    --------
    - :meth:`x // y <dt.FExpr.__floordiv__>` -- integer division operator.
