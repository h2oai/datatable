
.. xfunction:: datatable.math.arcosh
    :src: src/core/expr/funary/hyperbolic.cc pyfn_arcosh
    :cvar: doc_math_arcosh
    :signature: arcosh(x)

    The inverse hyperbolic cosine of `x`.

    This function satisfies the property that ``cosh(arccosh(x)) == x``.
    Alternatively, this function can also be computed as
    :math:`\cosh^{-1}(x) = \ln(x + \sqrt{x^2 - 1})`.

    See also
    --------
    - :func:`cosh` -- hyperbolic cosine;
    - :func:`arsinh` -- inverse hyperbolic sine.
