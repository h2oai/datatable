
.. xmethod:: datatable.FExpr.__pow__
    :src: src/core/expr/fbinary/fexpr__pow__.cc evaluate1
    :tests: tests/expr/fbinary/test-pow.py

    __pow__(x, y)
    --

    Raise `x` to the power `y`, or in math notation :math:`x^y`.

    If `x` or `y` are multi-column expressions, then they must have the
    same number of columns, and the `**` operator will be applied to each
    corresponding pair of columns. If either `x` or `y` are single-column
    while the other is multi-column, then the single-column expression
    will be repeated to the same number of columns as its opponent.

    The power operator can only be applied to numeric columns, and the
    resulting column will have stype ``float64`` in all cases except when both
    arguments are ``float32`` (in which case the result is also ``float32``).


    Parameters
    ----------
    x, y: FExpr
        The arguments must be either `FExpr`s, or expressions that can be
        converted into `FExpr`s.

    return: FExpr
        An expression that evaluates `x ** y`.
