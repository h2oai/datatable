
.. xfunction:: datatable.math.sin
    :src: src/core/expr/funary/trigonometric.cc resolve_op_sin
    :cvar: doc_math_sin
    :signature: sin(x)

    Compute the trigonometric sine of angle ``x`` measured in radians.

    This function can only be applied to numeric columns (real, integer, or
    boolean), and produces a float64 result, except when the argument ``x`` is
    float32, in which case the result is float32 as well.

    See also
    --------
    - :func:`cos(x)` -- the trigonometric cosine function;
    - :func:`arcsin(x)` -- the inverse sine function.
