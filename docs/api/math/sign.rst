
.. xfunction:: datatable.math.sign
    :src: src/core/expr/funary/floating.cc resolve_op_sign
    :cvar: doc_math_sign
    :signature: sign(x)

    The sign of x, returned as float.

    This function returns `1.0` if `x` is positive (including positive
    infinity), `-1.0` if `x` is negative, `0.0` if `x` is zero, and NA if
    `x` is NA.
