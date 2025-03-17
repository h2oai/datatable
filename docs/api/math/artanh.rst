
.. xfunction:: datatable.math.artanh
    :src: src/core/expr/funary/hyperbolic.cc pyfn_artanh
    :cvar: doc_math_artanh
    :signature: artanh(x)

    The inverse hyperbolic tangent of `x`.

    This function satisfies the property that ``tanh(artanh(x)) == x``.
    Alternatively, this function can also be computed as
    :math:`\tanh^{-1}(x) = \frac12\ln\frac{1+x}{1-x}`.

    See also
    --------
    - :func:`tanh` -- hyperbolic tangent;
