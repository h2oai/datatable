
.. xfunction:: datatable.math.logaddexp2
    :src: src/core/expr/fbinary/math.cc resolve_fn_logaddexp2
    :cvar: doc_math_logaddexp2
    :signature: logaddexp2(x, y)

    Binary logarithm of the sum of binary exponents of ``x`` and ``y``. This
    function is equivalent to ``log2(exp2(x) + exp2(y))``, but does
    not suffer from catastrophic precision loss for small values of
    ``x`` and ``y``.
