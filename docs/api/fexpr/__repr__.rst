
.. xmethod:: datatable.FExpr.__repr__
    :src: src/core/expr/fexpr.cc PyFExpr::m__repr__

    __repr__(self)
    --

    Return string representation of this object. This method is used
    by Python's built-in function :ext-func:`repr()`.

    The returned string has the following format::

        >>> "FExpr<...>"

    where ``...`` will attempt to match the expression used to construct
    this ``FExpr``.

    Examples
    --------
    >>> repr(3 + 2*(f.A + f["B"]))
    "FExpr<3 + 2 * (f.A + f['B'])>"
