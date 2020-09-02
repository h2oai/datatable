
.. py:module:: datatable.math

datatable.math
==============

:mod:`datatable` provides the similar set of mathematical functions, as Python's
standard `math module`_, or `numpy math functions`_. Below is the comparison
table showing which functions are available:


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
\                   ``divmod(x, y)``
``factorial(n)``
``gcd(a, b)``       ``gcd(a, b)``
\                   ``maximum(x, y)``
\                   ``minimum(x, y)``

**Mathematical constants**
-------------------------------------------------------------------------------
``e``               ``e``                 :const:`e`
\                   \                     :const:`golden`
``inf``             ``inf``               :const:`inf`
``nan``             ``nan``               :const:`nan`
``pi``              ``pi``                :const:`pi`
``tau``                                   :const:`tau`
==================  ====================  =====================================


Trigonometric/hyperbolic functions
----------------------------------

.. function:: sin(x)

    Compute the trigonometric sine of angle ``x`` measured in radians.

    This function can only be applied to numeric columns (real, integer, or
    boolean), and produces a float64 result, except when the argument ``x`` is
    float32, in which case the result is float32 as well.

.. function:: cos(x)

    Compute the trigonometric cosine of angle ``x`` measured in radians.

    This function can only be applied to numeric columns (real, integer, or
    boolean), and produces a float64 result, except when the argument ``x`` is
    float32, in which case the result is float32 as well.


.. function:: tan(x)

    Compute the trigonometric tangent of ``x``, which is the ratio
    ``sin(x)/cos(x)``.

    This function can only be applied to numeric columns (real, integer, or
    boolean), and produces a float64 result, except when the argument ``x`` is
    float32, in which case the result is float32 as well.


.. function:: arcsin(x)

    The inverse trigonometric sine of ``x``. In mathematics, this may also be
    written as :math:`\sin^{-1}x`. This function satisfies the property that
    ``sin(arcsin(x)) == x`` for all ``x`` in the interval ``[-1, 1]``.

    For the values of ``x`` that are greater than 1 in magnitude, the function
    arc-sine produces NA values.


.. function:: arccos(x)

    The inverse trigonometric cosine of ``x``. In mathematics, this may also be
    written as :math:`\cos^{-1}x`. This function satisfies the property that
    ``cos(arccos(x)) == x`` for all ``x`` in the interval ``[-1, 1]``.

    For the values of ``x`` that are greater than 1 in magnitude, the function
    arc-sine produces NA values.


.. function:: arctan(x)

    The inverse trigonometric tangent of ``x``. This function satisfies the
    property that ``tan(arctan(x)) == x``.


.. function:: atan2(y, x)

    The inverse trigonometric tangent of ``y/x``, taking into account the signs
    of ``x`` and ``y`` to produce the correct result.

    If ``(x,y)`` is a point in a Cartesian plane, then ``arctan2(y, x)`` returns
    the radian measure of an angle formed by 2 rays: one starting at the origin
    and passing through point ``(0,1)``, and the other starting at the origin
    and passing through point ``(x,y)``. The angle is assumed positive if the
    rotation from the first ray to the second occurs counter-clockwise, and
    negative otherwise.

    As a special case, ``arctan2(0, 0) == 0``, and ``arctan2(0, -1) == tau/2``..


.. function:: sinh(x)

    The hyperbolic sine of ``x``, defined as
    :math:`\sinh(x) = \frac12(e^x - e^{-x})`.


.. function:: cosh(x)

    The hyperbolic cosine of ``x``, defined as
    :math:`\cosh(x) = \frac12(e^x + e^{-x})`.


.. function:: tanh(x)

    The hyperbolic tangent of ``x``, defines as
    :math:`\tanh(x) = \frac{\sinh x}{\cosh x} = \frac{e^x-e^{-x}}{e^x+e^{-x}}`.


.. function:: arsinh(x)

    The inverse hyperbolic sine of ``x``. This function satisfies the property
    that ``sinh(arcsinh(x)) == x``. Alternatively, this function can also be
    computed as :math:`\sinh^{-1}(x) = \ln(x + \sqrt{x^2 + 1})`.


.. function:: arcosh(x)

    The inverse hyperbolic cosine of ``x``. This function satisfies the property
    that ``cosh(arccosh(x)) == x``. Alternatively, this function can also be
    computed as :math:`\cosh^{-1}(x) = \ln(x + \sqrt{x^2 - 1})`.


.. function:: artanh(x)

    The inverse hyperbolic tangent of ``x``. This function satisfies the property
    that ``sinh(arcsinh(x)) == x``. Alternatively, this function can also be
    computed as :math:`\tanh^{-1}(x) = \frac12\ln\frac{1+x}{1-x}`.


.. function:: hypot(x, y)

    The length of a hypotenuse in a right triangle with sides ``x`` and ``y``,
    i.e. :math:`\operatorname{hypot}(x, y) = \sqrt{x^2 + y^2}`.


.. function:: deg2rad(x)

    Convert an angle measured in degrees into radians:
    :math:`\operatorname{deg2rad}(x) = x\cdot\frac{\tau}{360}`.


.. function:: rad2deg(x)

    Convert an angle measured in radians into degrees:
    :math:`\operatorname{rad2deg}(x) = x\cdot\frac{360}{\tau}`.




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


.. _`math module`: https://docs.python.org/3/library/math.html
.. _`numpy math functions`: https://docs.scipy.org/doc/numpy-1.13.0/reference/routines.math.html



.. toctree::
    :hidden:

    abs()        <math/abs>
    cbrt()       <math/cbrt>
    ceil()       <math/ceil>
    copysign()   <math/copysign>
    e            <math/e>
    erf(x)       <math/erf>
    erfc(x)      <math/erfc>
    exp(x)       <math/exp>
    exp2(x)      <math/exp2>
    expm1(x)     <math/expm1>
    fabs()       <math/fabs>
    fmod()       <math/fmod>
    floor()      <math/floor>
    gamma()      <math/gamma>
    golden       <math/golden>
    inf          <math/inf>
    isclose()    <math/isclose>
    isfinite()   <math/isfinite>
    isinf()      <math/isinf>
    isna()       <math/isna>
    log()        <math/log>
    log10()      <math/log10>
    log1p()      <math/log1p>
    log2()       <math/log2>
    logaddexp()  <math/logaddexp>
    logaddexp2() <math/logaddexp2>
    ldexp()      <math/ldexp>
    lgamma()     <math/lgamma>
    nan          <math/nan>
    pi           <math/pi>
    pow()        <math/pow>
    rint()       <math/rint>
    round()      <math/round>
    sign()       <math/sign>
    signbit()    <math/signbit>
    sqrt()       <math/sqrt>
    square()     <math/square>
    tau          <math/tau>
    trunc()      <math/trunc>
