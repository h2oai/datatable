
.. xfunction:: datatable.math.erfc
    :src: src/core/expr/funary/special.cc resolve_op_erfc
    :cvar: doc_math_erfc
    :signature: erfc(x)

    Complementary error function ``erfc(x) = 1 - erf(x)``.

    The complementary error function is defined as the integral

    .. math::

        \operatorname{erfc}(x) = \frac{2}{\sqrt{\tau}} \int^{\infty}_{x/\sqrt{2}} e^{-\frac12 t^2}dt

    Although mathematically `erfc(x) = 1-erf(x)`, in practice the RHS
    suffers catastrophic loss of precision at large values of `x`. This
    function, however, does not have such a drawback.

    See also
    --------
    - :func:`erf(x) <datatable.math.erf>` -- the error function.
