
.. xfunction:: datatable.math.arsinh
    :src: src/core/expr/funary/hyperbolic.cc pyfn_arsinh
    :cvar: doc_math_arsinh
    :signature: arsinh(x)

    The inverse hyperbolic sine of `x`.

    This function satisfies the property that ``sinh(arcsinh(x)) == x``.
    Alternatively, this function can also be computed as
    :math:`\sinh^{-1}(x) = \ln(x + \sqrt{x^2 + 1})`.

    See also
    --------
    - :func:`sinh` -- hyperbolic sine;
    - :func:`arcosh` -- inverse hyperbolic cosine.
