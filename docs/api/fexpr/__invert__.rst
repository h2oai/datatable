
.. xmethod:: datatable.FExpr.__invert__
    :src: src/core/expr/funary/unary_invert.cc PyFExpr::nb__invert__

    __invert__(x)
    --

    Compute bitwise NOT of `x`, which corresponds to python operation ``~x``.

    If `x` is a multi-column expressions, then the ``~`` operator will be
    applied to each column in turn.

    Bitwise NOT can only be applied to integer or boolean columns. The
    resulting column will have the same stype as its argument.

    When the argument `x` is a boolean column, then ``~x`` is equivalent to
    logical NOT. This can be used to negate a condition, similar to python
    operator ``not`` (which is not overloadable).

    Parameters
    ----------
    x: FExpr
        Either an `FExpr`, or any object that can be converted into `FExpr`.

    return: FExpr
        An expression that evaluates ``~x``.


    Notes
    -----

    .. note::

        Use ``~x`` in order to negate a boolean FExpr. Using standard Python
        keyword ``not`` will result in an error.

