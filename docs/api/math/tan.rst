
.. xfunction:: datatable.math.tan
    :src: src/core/expr/funary/trigonometric.cc resolve_op_tan
    :cvar: doc_math_tan
    :signature: tan(x)

    Compute the trigonometric tangent of ``x``, which is the ratio
    ``sin(x)/cos(x)``.

    This function can only be applied to numeric columns (real, integer, or
    boolean), and produces a float64 result, except when the argument ``x`` is
    float32, in which case the result is float32 as well.

    See also
    --------
    - :func:`arctan(x)` -- the inverse tangent function.
