
.. xmethod:: datatable.FExpr.__lshift__
    :src: src/core/expr/fbinary/bitwise_shift.cc PyFExpr::nb__lshift__

    __lshift__(x, y)
    --

    Shift `x` by `y` bits to the left, i.e. ``x << y``. Mathematically this
    is equivalent to :math:`x\cdot 2^y`.

    If `x` or `y` are multi-column expressions, then they must have the
    same number of columns, and the ``<<`` operator will be applied to each
    corresponding pair of columns. If either `x` or `y` are single-column
    while the other is multi-column, then the single-column expression
    will be repeated to the same number of columns as its opponent.

    The left-shift operator can only be applied to integer columns,
    and the resulting column will have the same stype as its argument.


    Parameters
    ----------
    x, y: FExpr
        The arguments must be either `FExpr`s, or expressions that can be
        converted into `FExpr`s.

    return: FExpr
        An expression that evaluates ``x << y``.


    See also
    --------
    - :meth:`__rshift__(x, y) <dt.FExpr.__rshift__>` -- right shift.
