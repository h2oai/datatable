
.. py:module:: datatable.math

.. xpy:module:: datatable.math

datatable.math
==============


Trigonometric functions
-----------------------

.. list-table::
   :widths: auto
   :class: api-table

   * - :func:`sin(x) <datatable.math.sin>`
     - Compute :math:`\sin x` (the trigonometric sine of ``x``).

   * - :func:`cos(x) <datatable.math.cos>`
     - Compute :math:`\cos x` (the trigonometric cosine of ``x``).

   * - :func:`tan(x) <datatable.math.tan>`
     - Compute :math:`\tan x` (the trigonometric tangent of ``x``).

   * - :func:`arcsin(x) <datatable.math.arcsin>`
     - Compute :math:`\sin^{-1} x` (the inverse sine of ``x``).

   * - :func:`arccos(x) <datatable.math.arccos>`
     - Compute :math:`\cos^{-1} x` (the inverse cosine of ``x``).

   * - :func:`arctan(x) <datatable.math.arctan>`
     - Compute :math:`\tan^{-1} x` (the inverse tangent of ``x``).

   * - :func:`atan2(x, y) <datatable.math.atan2>`
     - Compute :math:`\tan^{-1} (x/y)`.

   * - :func:`hypot(x, y) <datatable.math.hypot>`
     - Compute :math:`\sqrt{x^2 + y^2}`.

   * - :func:`deg2rad(x) <datatable.math.deg2rad>`
     - Convert an angle measured in degrees into radians.

   * - :func:`rad2deg(x) <datatable.math.rad2deg>`
     - Convert an angle measured in radians into degrees.


Hyperbolic functions
--------------------

.. list-table::
   :widths: auto
   :class: api-table

   * - :func:`sinh(x) <datatable.math.sinh>`
     - Compute :math:`\sinh x` (the hyperbolic sine of ``x``).

   * - :func:`cosh(x) <datatable.math.cosh>`
     - Compute :math:`\cosh x` (the hyperbolic cosine of ``x``).

   * - :func:`tanh(x) <datatable.math.tanh>`
     - Compute :math:`\tanh x` (the hyperbolic tangent of ``x``).

   * - :func:`arsinh(x) <datatable.math.arsinh>`
     - Compute :math:`\sinh^{-1} x` (the inverse hyperbolic sine of ``x``).

   * - :func:`arcosh(x) <datatable.math.arcosh>`
     - Compute :math:`\cosh^{-1} x` (the inverse hyperbolic cosine of ``x``).

   * - :func:`artanh(x) <datatable.math.artanh>`
     - Compute :math:`\tanh^{-1} x` (the inverse hyperbolic tangent of ``x``).


Exponential/logarithmic functions
---------------------------------

.. list-table::
   :widths: auto
   :class: api-table

   * - :func:`exp(x) <datatable.math.exp>`
     - Compute :math:`e^x` (the exponent of ``x``).

   * - :func:`exp2(x) <datatable.math.exp2>`
     - Compute :math:`2^x`.

   * - :func:`expm1(x) <datatable.math.expm1>`
     - Compute :math:`e^x - 1`.

   * - :func:`log(x) <datatable.math.log>`
     - Compute :math:`\ln x` (the natural logarithm of ``x``).

   * - :func:`log10(x) <datatable.math.log10>`
     - Compute :math:`\log_{10} x` (the decimal logarithm of ``x``).

   * - :func:`log1p(x) <datatable.math.log1p>`
     - Compute :math:`\ln(1 + x)`.

   * - :func:`log2(x) <datatable.math.log2>`
     - Compute :math:`\log_{2} x` (the binary logarithm of ``x``).

   * - :func:`logaddexp(x) <datatable.math.logaddexp>`
     - Compute :math:`\ln(e^x + e^y)`.

   * - :func:`logaddexp2(x) <datatable.math.logaddexp2>`
     - Compute :math:`\log_2(2^x + 2^y)`.

   * - :func:`cbrt(x) <datatable.math.cbrt>`
     - Compute :math:`\sqrt[3]{x}` (the cubic root of ``x``).

   * - :func:`pow(x, a) <datatable.math.pow>`
     - Compute :math:`x^a`.

   * - :func:`sqrt(x) <datatable.math.sqrt>`
     - Compute :math:`\sqrt{x}` (the square root of ``x``).

   * - :func:`square(x) <datatable.math.square>`
     - Compute :math:`x^2` (the square of ``x``).


Special mathemetical functions
------------------------------

.. list-table::
   :widths: auto
   :class: api-table

   * - :func:`erf(x) <datatable.math.erf>`
     - The error function :math:`\operatorname{erf}(x)`.

   * - :func:`erfc(x) <datatable.math.erfc>`
     - The complimentary error function :math:`1 - \operatorname{erf}(x)`.

   * - :func:`gamma(x) <datatable.math.gamma>`
     - Euler gamma function of ``x``.

   * - :func:`lgamma(x) <datatable.math.lgamma>`
     - Natual logarithm of the Euler gamma function of.


Floating-point functions
------------------------

.. list-table::
   :widths: auto
   :class: api-table

   * - :func:`abs(x) <datatable.math.abs>`
     - Absolute value of ``x``.

   * - :func:`ceil(x) <datatable.math.ceil>`
     - The smallest integer not less than ``x``.

   * - :func:`copysign(x, y) <datatable.math.copysign>`
     - Number with the magnitude of ``x`` and the sign of ``y``.

   * - :func:`fabs(x) <datatable.math.fabs>`
     - The absolute value of ``x``, returned as a float.

   * - :func:`floor(x) <datatable.math.floor>`
     - The largest integer not greater than ``x``.

   * - :func:`fmod(x, y) <datatable.math.fmod>`
     - Remainder of a floating-point division ``x/y``.

   * - :func:`isclose(x, y) <datatable.math.isclose>`
     - Check whether ``x ≈ y`` (up to some tolerance level).

   * - :func:`isfinite(x) <datatable.math.isfinite>`
     - Check if ``x`` is finite.

   * - :func:`isinf(x) <datatable.math.isfinite>`
     - Check if ``x`` is a positive or negative infinity.

   * - :func:`isna(x) <datatable.math.isfinite>`
     - Check if ``x`` is a valid (not-NaN) value.

   * - :func:`ldexp(x, y) <datatable.math.ldexp>`
     - Compute :math:`x\cdot 2^y`.

   * - :func:`rint(x) <datatable.math.rint>`
     - Round ``x`` to the nearest integer.

   * - :func:`sign(x) <datatable.math.sign>`
     - The sign of ``x``, as a floating-point value.

   * - :func:`signbit(x) <datatable.math.signbit>`
     - The sign of ``x``, as a boolean value.

   * - :func:`trunc(x) <datatable.math.trunc>`
     - The value of ``x`` truncated towards zero.


Mathematical constants
----------------------

.. list-table::
   :widths: auto
   :class: api-table

   * - :data:`e <datatable.math.e>`
     - Euler's constant :math:`e`.

   * - :data:`golden <datatable.math.golden>`
     - Golden ratio :math:`\varphi`.

   * - :data:`inf <datatable.math.inf>`
     - Positive infinity.

   * - :data:`nan <datatable.math.nan>`
     - Not-a-number.

   * - :data:`pi <datatable.math.pi>`
     - Mathematical constant :math:`\pi`.

   * - :data:`tau <datatable.math.tau>`
     - Mathematical constant :math:`\tau`.



Comparison table
----------------

The set of functions provided by the :mod:`datatable.math` module is very
similar to the standard Python's :mod:`math` module, or
`numpy math functions`_. Below is the comparison table showing which functions
are available:

==================  ====================  =====================================
math                numpy                 datatable
==================  ====================  =====================================
**Trigonometric/hyperbolic functions**
-------------------------------------------------------------------------------
``sin(x)``          ``sin(x)``            :func:`sin(x) <sin>`
``cos(x)``          ``cos(x)``            :func:`cos(x) <cos>`
``tan(x)``          ``tan(x)``            :func:`tan(x) <tan>`
``asin(x)``         ``arcsin(x)``         :func:`arcsin(x) <arcsin>`
``acos(x)``         ``arccos(x)``         :func:`arccos(x) <arccos>`
``atan(x)``         ``arctan(x)``         :func:`arctan(x) <arctan>`
``atan2(y, x)``     ``arctan2(y, x)``     :func:`atan2(y, x) <atan2>`
``sinh(x)``         ``sinh(x)``           :func:`sinh(x) <sinh>`
``cosh(x)``         ``cosh(x)``           :func:`cosh(x) <cosh>`
``tanh(x)``         ``tanh(x)``           :func:`tanh(x) <tanh>`
``asinh(x)``        ``arcsinh(x)``        :func:`arsinh(x) <arsinh>`
``acosh(x)``        ``arccosh(x)``        :func:`arcosh(x) <arcosh>`
``atanh(x)``        ``arctanh(x)``        :func:`artanh(x) <artanh>`
``hypot(x, y)``     ``hypot(x, y)``       :func:`hypot(x, y) <hypot>`
``radians(x)``      ``deg2rad(x)``        :func:`deg2rad(x) <deg2rad>`
``degrees(x)``      ``rad2deg(x)``        :func:`rad2deg(x) <rad2deg>`

**Exponential/logarithmic/power functions**
-------------------------------------------------------------------------------
``exp(x)``          ``exp(x)``            :func:`exp(x) <exp>`
\                   ``exp2(x)``           :func:`exp2(x) <exp2>`
``expm1(x)``        ``expm1(x)``          :func:`expm1(x) <expm1>`
``log(x)``          ``log(x)``            :func:`log(x) <log>`
``log10(x)``        ``log10(x)``          :func:`log10(x) <log10>`
``log1p(x)``        ``log1p(x)``          :func:`log1p(x) <log1p>`
``log2(x)``         ``log2(x)``           :func:`log2(x) <log2>`
\                   ``logaddexp(x, y)``   :func:`logaddexp(x, y) <logaddexp>`
\                   ``logaddexp2(x, y)``  :func:`logaddexp2(x, y) <logaddexp2>`
\                   ``cbrt(x)``           :func:`cbrt(x) <cbrt>`
``pow(x, a)``       ``power(x, a)``       :func:`pow(x, a) <pow>`
``sqrt(x)``         ``sqrt(x)``           :func:`sqrt(x) <sqrt>`
\                   ``square(x)``         :func:`square(x) <square>`

**Special mathematical functions**
-------------------------------------------------------------------------------
``erf(x)``                                :func:`erf(x) <erf>`
``erfc(x)``                               :func:`erfc(x) <erfc>`
``gamma(x)``                              :func:`gamma(x) <gamma>`
\                   ``heaviside(x)``
\                   ``i0(x)``
``lgamma(x)``                             :func:`lgamma(x) <lgamma>`
\                   ``sinc(x)``

**Floating-point functions**
-------------------------------------------------------------------------------
``abs(x)``          ``abs(x)``            :func:`abs(x) <abs>`
``ceil(x)``         ``ceil(x)``           :func:`ceil(x) <ceil>`
``copysign(x, y)``  ``copysign(x, y)``    :func:`copysign(x, y) <copysign>`
``fabs(x)``         ``fabs(x)``           :func:`fabs(x) <fabs>`
``floor(x)``        ``floor(x)``          :func:`floor(x) <floor>`
``fmod(x, y)``      ``fmod(x, y)``        :func:`fmod(x) <fmod>`
``frexp(x)``        ``frexp(x)``
``isclose(x, y)``   ``isclose(x, y)``     :func:`isclose(x, y) <isclose>`
``isfinite(x)``     ``isfinite(x)``       :func:`isfinite(x) <isfinite>`
``isinf(x)``        ``isinf(x)``          :func:`isinf(x) <isinf>`
``isnan(x)``        ``isnan(x)``          :func:`isna(x) <isna>`
``ldexp(x, n)``     ``ldexp(x, n)``       :func:`ldexp(x, n) <ldexp>`
``modf(x)``         ``modf(x)``
\                   ``nextafter(x, y)``
\                   ``rint(x)``           :func:`rint(x) <rint>`
``round(x)``        ``round(x)``          :func:`round(x) <datatable.math.round>`
\                   ``sign(x)``           :func:`sign(x) <sign>`
\                   ``signbit(x)``        :func:`signbit(x) <signbit>`
\                   ``spacing(x)``
``trunc(x)``        ``trunc(x)``          :func:`trunc(x) <trunc>`

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
