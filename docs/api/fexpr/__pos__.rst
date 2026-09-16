
.. xmethod:: datatable.FExpr.__pos__
    :src: src/core/expr/funary/unary_plus.cc PyFExpr::nb__pos__

    __pos__(x)
    --

    Unary plus, which corresponds to python operation ``+x``.

    If `x` is a multi-column expressions, then the ``+`` operator will be
    applied to each column in turn.

    Unary plus can only be applied to numeric columns. The resulting column
    will have the same stype as its argument, but not less than ``int32``.


    Parameters
    ----------
    x: FExpr
        Either an `FExpr`, or any object that can be converted into `FExpr`.

    return: FExpr
        An expression that evaluates ``+x``.
