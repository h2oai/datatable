
.. xfunction:: datatable.math.erf
    :src: src/core/expr/funary/special.cc resolve_op_erf
    :cvar: doc_math_erf
    :signature: erf(x)

    Error function ``erf(x)``, which is defined as the integral

    .. math::

        \operatorname{erf}(x) = \frac{2}{\sqrt{\tau}} \int^{x/\sqrt{2}}_0 e^{-\frac12 t^2}dt

    This function is used in computing probabilities arising from the normal
    distribution.

    See also
    --------
    - :func:`erfc(x) <datatable.math.erfc>` -- complimentary error function.
