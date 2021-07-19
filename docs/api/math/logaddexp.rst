
.. xfunction:: datatable.math.logaddexp
    :src: src/core/expr/fbinary/math.cc resolve_fn_logaddexp
    :cvar: doc_math_logaddexp
    :signature: logaddexp(x, y)

    The logarithm of the sum of exponents of x and y. This function is
    equivalent to ``log(exp(x) + exp(y))``, but does not suffer from
    catastrophic precision loss for small values of x and y.
