
.. xfunction:: datatable.math.isfinite
    :src: src/core/expr/funary/floating.cc resolve_op_isfinite
    :cvar: doc_math_isfinite
    :signature: isfinite(x)

    Returns True if `x` has a finite value, and False if `x` is infinity
    or NaN. This function is equivalent to ``!(isna(x) or isinf(x))``.

    See also
    --------
    - :func:`isna()`
    - :func:`isinf()`
