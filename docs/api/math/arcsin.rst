
.. xfunction:: datatable.math.arcsin
    :src: src/core/expr/funary/trigonometric.cc pyfn_arcsin
    :cvar: doc_math_arcsin
    :signature: arcsin(x)

    Inverse trigonometric sine of `x`.

    In mathematics, this may be written as :math:`\arcsin x` or
    :math:`\sin^{-1}x`.

    The returned value is in the interval :math:`[-\frac14 \tau, \frac14\tau]`,
    and NA for the values of ``x`` that lie outside the interval ``[-1, 1]``.
    This function is the inverse of :func:`sin()` in the sense
    that `sin(arcsin(x)) == x` for all ``x`` in the interval ``[-1, 1]``.

    See also
    --------
    - :func:`sin(x)` -- the trigonometric sine function;
    - :func:`arccos(x)` -- the inverse cosine function.
