
.. xfunction:: datatable.math.floor
    :src: src/core/expr/funary/floating.cc resolve_op_floor
    :cvar: doc_math_floor
    :signature: floor(x)

    The largest integer value not greater than `x`, returned as float.

    This function produces a ``float32`` column if the input is of type
    ``float32``, or ``float64`` columns for inputs of all other numeric
    stypes.

    Parameters
    ----------
    x: FExpr
        One or more numeric columns.

    return: FExpr
        Expression that computes the ``floor()`` function for each row and
        column in `x`.
