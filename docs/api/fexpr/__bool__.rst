
.. xmethod:: datatable.FExpr.__bool__
    :src: src/core/expr/fexpr.cc PyFExpr::nb__bool__

    __bool__(self)
    --

    Using this operator will result in a ``TypeError``.

    The boolean-cast operator is used by Python whenever it wants to
    know whether the object is equivalent to a single ``True`` or ``False``
    value. This is not applicable for a :class:`dt.FExpr`, which represents
    *stored* computation on a *column* or multiple columns. As such, an
    error is raised.

    In order to convert a column into the boolean stype, you can use the
    type-cast operator ``dt.bool8(x)``.
