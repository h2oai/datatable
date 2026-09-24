
.. xfunction:: datatable.math.arccos
    :src: src/core/expr/funary/trigonometric.cc pyfn_arccos
    :cvar: doc_math_arccos
    :signature: arccos(x)

    Inverse trigonometric cosine of `x`.

    In mathematics, this may be written as :math:`\arccos x` or
    :math:`\cos^{-1}x`.

    The returned value is in the interval :math:`[0, \frac12\tau]`,
    and NA for the values of ``x`` that lie outside the interval
    ``[-1, 1]``. This function is the inverse of
    :func:`cos()` in the sense that
    `cos(arccos(x)) == x` for all ``x`` in the interval ``[-1, 1]``.

    See also
    --------
    - :func:`cos(x)` -- the trigonometric cosine function;
    - :func:`arcsin(x)` -- the inverse sine function.
