
.. xfunction:: datatable.math.gamma
    :src: src/core/expr/funary/special.cc resolve_op_gamma
    :cvar: doc_math_gamma
    :signature: gamma(x)

    Euler Gamma function of x.

    The gamma function is defined for all ``x`` except for the negative
    integers. For positive ``x`` it can be computed via the integral

    .. math::
        \Gamma(x) = \int_0^\infty t^{x-1}e^{-t}dt

    For negative ``x`` it can be computed as

    .. math::
        \Gamma(x) = \frac{\Gamma(x + k)}{x(x+1)\cdot...\cdot(x+k-1)}

    where :math:`k` is any integer such that :math:`x+k` is positive.

    If `x` is a positive integer, then :math:`\Gamma(x) = (x - 1)!`.

    See also
    --------
    - :func:`lgamma(x) <datatable.math.lgamma>` -- log-gamma function.
