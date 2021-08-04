
.. xfunction:: datatable.math.cos
    :src: src/core/expr/funary/trigonometric.cc resolve_op_cos
    :cvar: doc_math_cos
    :signature: cos(x)

    Compute the trigonometric cosine of angle ``x`` measured in radians.

    This function can only be applied to numeric columns (real, integer, or
    boolean), and produces a float64 result, except when the argument ``x`` is
    float32, in which case the result is float32 as well.

    See also
    --------
    - :func:`sin(x)` -- the trigonometric sine function;
    - :func:`arccos(x)` -- the inverse cosine function.
