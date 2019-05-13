
math functions
==============

``datatable`` provides the similar set of mathematical functions, as Python's
standard `math module`_, or `numpy math functions`_. Below is the comparison
table showing which functions are available:

.. _`math module`: https://docs.python.org/3/library/math.html
.. _`numpy math functions`: https://docs.scipy.org/doc/numpy-1.13.0/reference/routines.math.html

==================  ====================  =================
math                numpy                 datatable
==================  ====================  =================
**Trigonometric/hyperbolic functions**
-----------------------------------------------------------
``sin(x)``          ``sin(x)``            ``sin(x)``
``cos(x)``          ``cos(x)``            ``cos(x)``
``tan(x)``          ``tan(x)``            ``tan(x)``
``asin(x)``         ``arcsin(x)``         ``arcsin(x)``
``acos(x)``         ``arccos(x)``         ``arccos(x)``
``atan(x)``         ``arctan(x)``         ``arctan(x)``
``atan2(x, y)``     ``arctan2(x, y)``     ``arctan2(x, y)``
``sinh(x)``         ``sinh(x)``           ``sinh(x)``
``cosh(x)``         ``cosh(x)``           ``cosh(x)``
``tanh(x)``         ``tanh(x)``           ``tanh(x)``
``asinh(x)``        ``arcsinh(x)``        ``arcsinh(x)``
``acosh(x)``        ``arccosh(x)``        ``arccosh(x)``
``atanh(x)``        ``arctanh(x)``        ``arctanh(x)``
``hypot(x, y)``     ``hypot(x, y)``       ``hypot(x, y)``
``radians(x)``      ``deg2rad(x)``        ``deg2rad(x)``
``degrees(x)``      ``rad2deg(x)``        ``rad2deg(x)``

**Exponential/logarithmic/power functions**
-----------------------------------------------------------
``exp(x)``          ``exp(x)``            ``exp(x)``
\                   ``exp2(x)``           ``exp2(x)``
``expm1(x)``        ``expm1(x)``          ``expm1(x)``
``log(x)``          ``log(x)``            ``log(x)``
``log10(x)``        ``log10(x)``          ``log10(x)``
``log1p(x)``        ``log1p(x)``          ``log1p(x)``
``log2(x)``         ``log2(x)``           ``log2(x)``
\                   ``logaddexp(x, y)``
\                   ``logaddexp2(x, y)``
\                   ``cbrt(x)``           ``cbrt(x)``
``pow(x, a)``       ``power(x, a)``
``sqrt(x)``         ``sqrt(x)``           ``sqrt(x)``
\                   ``square(x)``         ``square(x)``

**Special mathematical functions**
-----------------------------------------------------------
``erf(x)``                                ``erf(x)``
``erfc(x)``                               ``erfc(x)``
``gamma(x)``                              ``gamma(x)``
\                   ``heaviside(x)``
\                   ``i0(x)``
``lgamma(x)``                             ``lgamma(x)``
\                   ``sinc(x)``

**Floating-point functions**
-----------------------------------------------------------
``ceil(x)``         ``ceil(x)``
``copysign(x, y)``  ``copysign(x, y)``
``fabs(x)``         ``fabs(x)``           ``fabs(x)``
``floor(x)``        ``floor(x)``
``frexp(x)``        ``frexp(x)``
``isclose(x, y)``   ``isclose(x, y)``
``isfinite(x)``     ``isfinite(x)``
``isinf(x)``        ``isinf(x)``
``isnan(x)``        ``isnan(x)``
``ldexp(x, n)``     ``ldexp(x, n)``
\                   ``nextafter(x)``
\                   ``rint(x)``
\                   ``sign(x)``           ``sign(x)``
\                   ``spacing(x)``
\                   ``signbit(x)``
``trunc(x)``        ``trunc(x)``

**Miscellaneous**
-----------------------------------------------------------
\                   ``clip(x, a, b)``
\                   ``divmod(x, y)``
``factorial(n)``
``fmod(x, y)``      ``fmod(x, y)``
``gcd(a, b)``       ``gcd(a, b)``
\                   ``maximum(x, y)``
\                   ``minimum(x, y)``
``modf(x)``         ``modf(x)``

**Mathematical constants**
-----------------------------------------------------------
``e``               ``e``                 ``e``
\                   \                     ``golden``
``inf``             ``inf``               ``inf``
``nan``             ``nan``               ``nan``
``pi``              ``pi``                ``pi``
``tau``                                   ``tau``
==================  ====================  =================


