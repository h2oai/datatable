
.. xfunction:: datatable.math.abs
    :src: src/core/expr/funary/floating.cc resolve_op_abs
    :cvar: doc_math_abs
    :signature: abs(x)

    Return the absolute value of ``x``. This function can only be applied
    to numeric arguments (i.e. boolean, integer, or real).

    This function upcasts columns of types `bool8`, `int8` and `int16` into
    `int32`; for columns of other types the stype is kept.

    Parameters
    ----------
    x: FExpr
        Column expression producing one or more numeric columns.

    return: FExpr
        The resulting FExpr evaluates absolute values in all elements
        in all columns of `x`.

    Examples
    --------

    .. code-block::

        >>> DT = dt.Frame(A=[-3, 2, 4, -17, 0])
        >>> DT[:, abs(f.A)]
           |     A
           | int32
        -- + -----
         0 |     3
         1 |     2
         2 |     4
         3 |    17
         4 |     0
        [5 rows x 1 column]

    See also
    --------
    - :func:`fabs()`
