
.. xpy:module:: datatable.math

datatable.math
==============


Trigonometric functions
-----------------------

.. list-table::
   :widths: auto
   :class: api-table

   * - :func:`sin(x)`
     - Compute :math:`\sin x` (the trigonometric sine of ``x``).

   * - :func:`cos(x)`
     - Compute :math:`\cos x` (the trigonometric cosine of ``x``).

   * - :func:`tan(x)`
     - Compute :math:`\tan x` (the trigonometric tangent of ``x``).

   * - :func:`arcsin(x)`
     - Compute :math:`\sin^{-1} x` (the inverse sine of ``x``).

   * - :func:`arccos(x)`
     - Compute :math:`\cos^{-1} x` (the inverse cosine of ``x``).

   * - :func:`arctan(x)`
     - Compute :math:`\tan^{-1} x` (the inverse tangent of ``x``).

   * - :func:`atan2(x, y)`
     - Compute :math:`\tan^{-1} (x/y)`.

   * - :func:`hypot(x, y)`
     - Compute :math:`\sqrt{x^2 + y^2}`.

   * - :func:`deg2rad(x)`
     - Convert an angle measured in degrees into radians.

   * - :func:`rad2deg(x)`
     - Convert an angle measured in radians into degrees.


Hyperbolic functions
--------------------

.. list-table::
   :widths: auto
   :class: api-table

   * - :func:`sinh(x)`
     - Compute :math:`\sinh x` (the hyperbolic sine of ``x``).

   * - :func:`cosh(x)`
     - Compute :math:`\cosh x` (the hyperbolic cosine of ``x``).

   * - :func:`tanh(x)`
     - Compute :math:`\tanh x` (the hyperbolic tangent of ``x``).

   * - :func:`arsinh(x)`
     - Compute :math:`\sinh^{-1} x` (the inverse hyperbolic sine of ``x``).

   * - :func:`arcosh(x)`
     - Compute :math:`\cosh^{-1} x` (the inverse hyperbolic cosine of ``x``).

   * - :func:`artanh(x)`
     - Compute :math:`\tanh^{-1} x` (the inverse hyperbolic tangent of ``x``).


Exponential/logarithmic functions
---------------------------------

.. list-table::
   :widths: auto
   :class: api-table

   * - :func:`exp(x)`
     - Compute :math:`e^x` (the exponent of ``x``).

   * - :func:`exp2(x)`
     - Compute :math:`2^x`.

   * - :func:`expm1(x)`
     - Compute :math:`e^x - 1`.

   * - :func:`log(x)`
     - Compute :math:`\ln x` (the natural logarithm of ``x``).

   * - :func:`log10(x)`
     - Compute :math:`\log_{10} x` (the decimal logarithm of ``x``).

   * - :func:`log1p(x)`
     - Compute :math:`\ln(1 + x)`.

   * - :func:`log2(x)`
     - Compute :math:`\log_{2} x` (the binary logarithm of ``x``).

   * - :func:`logaddexp(x)`
     - Compute :math:`\ln(e^x + e^y)`.

   * - :func:`logaddexp2(x)`
     - Compute :math:`\log_2(2^x + 2^y)`.

   * - :func:`cbrt(x)`
     - Compute :math:`\sqrt[3]{x}` (the cubic root of ``x``).

   * - :func:`pow(x, a)`
     - Compute :math:`x^a`.

   * - :func:`sqrt(x)`
     - Compute :math:`\sqrt{x}` (the square root of ``x``).

   * - :func:`square(x)`
     - Compute :math:`x^2` (the square of ``x``).


Special mathemetical functions
------------------------------

.. list-table::
   :widths: auto
   :class: api-table

   * - :func:`erf(x)`
     - The error function :math:`\operatorname{erf}(x)`.

   * - :func:`erfc(x)`
     - The complimentary error function :math:`1 - \operatorname{erf}(x)`.

   * - :func:`gamma(x)`
     - Euler gamma function of ``x``.

   * - :func:`lgamma(x)`
     - Natual logarithm of the Euler gamma function of.


Floating-point functions
------------------------

.. list-table::
   :widths: auto
   :class: api-table

   * - :func:`abs(x)`
     - Absolute value of ``x``.

   * - :func:`ceil(x)`
     - The smallest integer not less than ``x``.

   * - :func:`copysign(x, y)`
     - Number with the magnitude of ``x`` and the sign of ``y``.

   * - :func:`fabs(x)`
     - The absolute value of ``x``, returned as a float.

   * - :func:`floor(x)`
     - The largest integer not greater than ``x``.

   * - :func:`fmod(x, y)`
     - Remainder of a floating-point division ``x/y``.

   * - :func:`isclose(x, y)`
     - Check whether ``x ≈ y`` (up to some tolerance level).

   * - :func:`isfinite(x)`
     - Check if ``x`` is finite.

   * - :func:`isinf(x)`
     - Check if ``x`` is a positive or negative infinity.

   * - :func:`isna(x)`
     - Check if ``x`` is a valid (not-NaN) value.

   * - :func:`ldexp(x, y)`
     - Compute :math:`x\cdot 2^y`.

   * - :func:`rint(x)`
     - Round ``x`` to the nearest integer.

   * - :func:`sign(x)`
     - The sign of ``x``, as a floating-point value.

   * - :func:`signbit(x)`
     - The sign of ``x``, as a boolean value.

   * - :func:`trunc(x)`
     - The value of ``x`` truncated towards zero.


Mathematical constants
----------------------

.. list-table::
   :widths: auto
   :class: api-table

   * - :data:`e`
     - Euler's constant :math:`e`.

   * - :data:`golden`
     - Golden ratio :math:`\varphi`.

   * - :data:`inf`
     - Positive infinity.

   * - :data:`nan`
     - Not-a-number.

   * - :data:`pi`
     - Mathematical constant :math:`\pi`.

   * - :data:`tau`
     - Mathematical constant :math:`\tau`.



Comparison table
----------------

The set of functions provided by the :mod:`datatable.math` module is very
similar to the standard Python's :ext-mod:`math` module, or
`numpy math functions`_. Below is the comparison table showing which functions
are available:

==================  ====================  =====================================
math                numpy                 datatable
==================  ====================  =====================================
**Trigonometric/hyperbolic functions**
-------------------------------------------------------------------------------
``sin(x)``          ``sin(x)``            :func:`sin(x)`
``cos(x)``          ``cos(x)``            :func:`cos(x)`
``tan(x)``          ``tan(x)``            :func:`tan(x)`
``asin(x)``         ``arcsin(x)``         :func:`arcsin(x)`
``acos(x)``         ``arccos(x)``         :func:`arccos(x)`
``atan(x)``         ``arctan(x)``         :func:`arctan(x)`
``atan2(y, x)``     ``arctan2(y, x)``     :func:`atan2(y, x)`
``sinh(x)``         ``sinh(x)``           :func:`sinh(x)`
``cosh(x)``         ``cosh(x)``           :func:`cosh(x)`
``tanh(x)``         ``tanh(x)``           :func:`tanh(x)`
``asinh(x)``        ``arcsinh(x)``        :func:`arsinh(x)`
``acosh(x)``        ``arccosh(x)``        :func:`arcosh(x)`
``atanh(x)``        ``arctanh(x)``        :func:`artanh(x)`
``hypot(x, y)``     ``hypot(x, y)``       :func:`hypot(x, y)`
``radians(x)``      ``deg2rad(x)``        :func:`deg2rad(x)`
``degrees(x)``      ``rad2deg(x)``        :func:`rad2deg(x)`

**Exponential/logarithmic/power functions**
-------------------------------------------------------------------------------
``exp(x)``          ``exp(x)``            :func:`exp(x)`
\                   ``exp2(x)``           :func:`exp2(x)`
``expm1(x)``        ``expm1(x)``          :func:`expm1(x)`
``log(x)``          ``log(x)``            :func:`log(x)`
``log10(x)``        ``log10(x)``          :func:`log10(x)`
``log1p(x)``        ``log1p(x)``          :func:`log1p(x)`
``log2(x)``         ``log2(x)``           :func:`log2(x)`
\                   ``logaddexp(x, y)``   :func:`logaddexp(x, y)`
\                   ``logaddexp2(x, y)``  :func:`logaddexp2(x, y)`
\                   ``cbrt(x)``           :func:`cbrt(x)`
``pow(x, a)``       ``power(x, a)``       :func:`pow(x, a)`
``sqrt(x)``         ``sqrt(x)``           :func:`sqrt(x)`
\                   ``square(x)``         :func:`square(x)`

**Special mathematical functions**
-------------------------------------------------------------------------------
``erf(x)``                                :func:`erf(x)`
``erfc(x)``                               :func:`erfc(x)`
``gamma(x)``                              :func:`gamma(x)`
\                   ``heaviside(x)``
\                   ``i0(x)``
``lgamma(x)``                             :func:`lgamma(x)`
\                   ``sinc(x)``

**Floating-point functions**
-------------------------------------------------------------------------------
``abs(x)``          ``abs(x)``            :func:`abs(x)`
``ceil(x)``         ``ceil(x)``           :func:`ceil(x)`
``copysign(x, y)``  ``copysign(x, y)``    :func:`copysign(x, y)`
``fabs(x)``         ``fabs(x)``           :func:`fabs(x)`
``floor(x)``        ``floor(x)``          :func:`floor(x)`
``fmod(x, y)``      ``fmod(x, y)``        :func:`fmod(x)`
``frexp(x)``        ``frexp(x)``
``isclose(x, y)``   ``isclose(x, y)``     :func:`isclose(x, y)`
``isfinite(x)``     ``isfinite(x)``       :func:`isfinite(x)`
``isinf(x)``        ``isinf(x)``          :func:`isinf(x)`
``isnan(x)``        ``isnan(x)``          :func:`isna(x)`
``ldexp(x, n)``     ``ldexp(x, n)``       :func:`ldexp(x, n)`
``modf(x)``         ``modf(x)``
\                   ``nextafter(x, y)``
\                   ``rint(x)``           :func:`rint(x)`
``round(x)``        ``round(x)``          :func:`round(x)`
\                   ``sign(x)``           :func:`sign(x)`
\                   ``signbit(x)``        :func:`signbit(x)`
\                   ``spacing(x)``
``trunc(x)``        ``trunc(x)``          :func:`trunc(x)`

**Miscellaneous**
-------------------------------------------------------------------------------
\                   ``clip(x, a, b)``
``comb(n, k)``
\                   ``divmod(x, y)``
``factorial(n)``
``gcd(a, b)``       ``gcd(a, b)``
\                   ``maximum(x, y)``
\                   ``minimum(x, y)``

**Mathematical constants**
-------------------------------------------------------------------------------
``e``               ``e``                 :data:`e`
\                   \                     :data:`golden`
``inf``             ``inf``               :data:`inf`
``nan``             ``nan``               :data:`nan`
``pi``              ``pi``                :data:`pi`
``tau``                                   :data:`tau`
==================  ====================  =====================================

.. _`numpy math functions`: https://docs.scipy.org/doc/numpy-1.13.0/reference/routines.math.html



.. toctree::
    :hidden:

    abs()        <math/abs>
    arccos()     <math/arccos>
    arcosh()     <math/arcosh>
    arcsin()     <math/arcsin>
    arctan()     <math/arctan>
    arsinh()     <math/arsinh>
    artanh()     <math/artanh>
    atan2()      <math/atan2>
    cbrt()       <math/cbrt>
    ceil()       <math/ceil>
    copysign()   <math/copysign>
    cos()        <math/cos>
    cosh()       <math/cosh>
    deg2rad()    <math/deg2rad>
    e            <math/e>
    erf(x)       <math/erf>
    erfc(x)      <math/erfc>
    exp(x)       <math/exp>
    exp2(x)      <math/exp2>
    expm1(x)     <math/expm1>
    fabs()       <math/fabs>
    floor()      <math/floor>
    fmod()       <math/fmod>
    gamma()      <math/gamma>
    golden       <math/golden>
    hypot()      <math/hypot>
    inf          <math/inf>
    isclose()    <math/isclose>
    isfinite()   <math/isfinite>
    isinf()      <math/isinf>
    isna()       <math/isna>
    ldexp()      <math/ldexp>
    lgamma()     <math/lgamma>
    log()        <math/log>
    log10()      <math/log10>
    log1p()      <math/log1p>
    log2()       <math/log2>
    logaddexp()  <math/logaddexp>
    logaddexp2() <math/logaddexp2>
    nan          <math/nan>
    pi           <math/pi>
    pow()        <math/pow>
    rad2deg()    <math/rad2deg>
    rint()       <math/rint>
    round()      <math/round>
    sign()       <math/sign>
    signbit()    <math/signbit>
    sin()        <math/sin>
    sinh()       <math/sinh>
    sqrt()       <math/sqrt>
    square()     <math/square>
    tan()        <math/tan>
    tanh()       <math/tanh>
    tau          <math/tau>
    trunc()      <math/trunc>
