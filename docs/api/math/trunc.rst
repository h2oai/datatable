
.. xfunction:: datatable.math.trunc
    :src: src/core/expr/funary/floating.cc resolve_op_trunc
    :cvar: doc_math_trunc
    :signature: trunc(x)

    The nearest integer value not greater than `x` in magnitude.

    If x is integer or boolean, then trunc() will return this value
    converted to float64. If x is floating-point, then trunc(x) acts as
    floor(x) for positive values of x, and as ceil(x) for negative values
    of x. This rounding mode is known as rounding towards zero.
