
.. xfunction:: datatable.math.ceil
    :src: src/core/expr/funary/floating.cc resolve_op_ceil
    :cvar: doc_math_ceil
    :signature: ceil(x)

    The smallest integer value not less than `x`, returned as float.

    This function produces a ``float32`` column if the input is of type
    ``float32``, or ``float64`` columns for inputs of all other numeric
    stypes.

    Parameters
    ----------
    x: FExpr
        One or more numeric columns.

    return: FExpr
        Expression that computes the ``ceil()`` function for each row and
        column in `x`.
