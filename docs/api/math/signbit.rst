
.. xfunction:: datatable.math.signbit
    :src: src/core/expr/funary/floating.cc resolve_op_signbit
    :cvar: doc_math_signbit
    :signature: signbit(x)

    Returns `True` if `x` is negative (its sign bit is set), and `False` if
    `x` is positive. This function is able to distinguish between `-0.0` and
    `+0.0`, returning `True`/`False` respectively. If `x` is an NA value, this
    function will also return NA.
