.. _`math`:

math module
===========

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

.. function:: exp(x)

    The exponent of ``x``, i.e. Euler's number :const:`e` raised to the power
    ``x``.


.. function:: exp2(x)

    Two raised to the power ``x``.


.. function:: expm1(x)

    Computes :math:`e^x - 1`, however offering a better precision than
    ``exp(x) - 1`` for small values of ``x``.


.. function:: log(x)

    The natural logarithm of ``x``. This function is the inverse of ``exp(x)``:
    ``exp(log(x)) == x``.


.. function:: log10(x)

    The base-10 logarithm of ``x``, also denoted as :math:`\lg(x)` in
    mathematics. This function is the inverse of ``power(10, x)``.


.. function:: log1p(x)

    The natural logarithm of 1 plus ``x``, i.e. :math:`\ln(1 + x)`.


.. function:: log2(x)

    The base-2 logarithm of ``x``, this function is the inverse of ``exp2(x)``.


.. function:: logaddexp(x, y)

    Logarithm of the sum of exponents of ``x`` and ``y``:
    :math:`\ln(e^x + e^y)`. The result avoids loss of precision from
    exponentiating small numbers.


.. function:: logaddexp2(x, y)

    Binary logarithm of the sum of binary exponents of ``x`` and ``y``:
    :math:`\log_2(2^x + 2^y)`. The result avoids loss of precision from
    exponentiating small numbers.


.. function:: cbrt(x)

    Compute the cubic root of ``x``, i.e. :math:`\sqrt[3]{x}`.


.. function:: pow(x, a)

    Raise ``x`` to the power ``a``, i.e. calculate :math:`x^a`.


.. function:: sqrt(x)

    The square root of ``x``, i.e. :math:`\sqrt{x}`.


.. function:: square(x)

    The square of ``x``, i.e. :math:`x^2`.



Special mathemetical functions
------------------------------

.. function:: erf(x)

    The error function :math:`\operatorname{erf}(x)`.

    This function is defined as the integral
    :math:`\operatorname{erf}(x) = \frac{2}{\sqrt{\pi}} \int^x_0 e^{-t^2}dt`.
    This function is used in computing probabilities arising from the normal
    distribution.


.. function:: erfc(x)

    The complementary error function
    :math:`\operatorname{erfc}(x) = 1 - \operatorname{erf}(x)`.

    This function is defined as the integral
    :math:`\operatorname{erfc}(x) = \frac{2}{\sqrt{\pi}} \int^{\infty}_x e^{-t^2}dt`.

    For large values of ``x`` this function computes the result much more
    precisely than ``1 - erf(x)``.


.. function:: gamma(x)

    Euler gamma function of ``x``.

    The gamma function is defined for all ``x`` except for the negative
    integers. For positive ``x`` it can be computed via the integral
    :math:`\Gamma(x) = \int_0^\infty t^{x-1}e^{-t}dt`. For negative ``x`` it
    can be computed as
    :math:`\Gamma(x) = \frac{\Gamma(x + k)}{x(x+1)\cdot...\cdot(x+k-1)}`,
    where :math:`k` is any integer such that :math:`x+k` is positive.

    If `x` is a positive integer, then :math:`\Gamma(x) = (x - 1)!`.


.. function:: lgamma(x)

    Natural logarithm of the absolute value of gamma function of ``x``.



Floating-point functions
------------------------

.. function:: abs(x)

    Return the absolute value of ``x``. This function can only be applied
    to numeric arguments (i.e. boolean, integer, or real).

    The argument ``x`` can be one of the following:

    - a :class:`Frame`, in which case the function is applied to all elements
      of the frame, and returns a new frame with the same shape and stypes as
      ``x``. An error will be raised if any columns in ``x`` are not numeric.

    - a column-expression, in which case ``abs(x)`` is also a column-expression
      that, when applied to some frame ``DT``, will evaluate to a column with
      the absolute values of ``x``. The stype of the resulting column will be
      the same as the stype of ``x``.

    - an ``int``, or a ``float``, in which case ``abs(x)`` returns the absolute
      value of that number, similar to the python built-in function ``abs()``.

    **Examples**::

        DT = dt.Frame(A=[-3, 2, 4, -17, 0])
        DT[:, abs(f.A)]

    .. dtframe::
        :names: C0
        :types: int8
        :shape: 5, 1

        0,   3
        1,   2
        2,   4
        3,  17
        4,   0

    **See also**

    - :func:`fabs`


.. function:: ceil(x)

    The smallest integer not less than ``x``.


.. function:: copysign(x, y)

    Return a float with the magnitude of ``x`` and the sign of ``y``.


.. function:: fabs(x)

    The absolute value of ``x``, returned as a float.


.. function:: floor(x)

    The largest integer not greater than ``x``.


.. function:: fmod(x, y)

    Return the remainder of a floating-point division ``x/y``.


.. function:: isclose(x, y, *, rtol=1e-5, atol=1e-8)

    Return True if ``x ≈ y``, and False otherwise.

    The comparison is done using the relative tolerance ``rtol`` and the
    absolute tolerance ``atol`` parameters. The numbers ``x`` and ``y`` are
    considered close if :math:`|x-y| \le atol + rtol|y|`. Note that this
    relationship is not symmetric: it is possible to have ``x`` "close" to
    ``y``, while ``y`` not "close" to ``x``.


.. function:: isfinite(x)

    Returns True if ``x`` is a finite value, and False if ``x`` is a
    positive/negative infinity of NA.


.. function:: isinf(x)

    Returns True if ``x`` is a positive or negative infinity, and False
    otherwise.


.. function:: isna(x)

    Returns True if ``x`` is an NA value, and False otherwise.

    - If ``x`` is a :class:`Frame`, the function is applied separately to each
      element in the frame. The result is a new Frame where all columns are
      boolean, and with the same shape as ``x``. Each element in this new frame
      is a boolean indicator of whether the corresponding element in ``x`` is
      an NA value or not.

    - If ``x`` is a column-expression, then ``isna(x)`` is also an expression.
      The argument column ``x`` can be of any stype, and the result is a column
      with stype `bool8`. When evaluated within ``DT[i, j, ...]``, the expression
      ``isna(x)`` produces a column where each element is an indicator of whether
      the corresponding value in ``x`` is NA or not.

    - When ``x`` is a python integer, ``isna(x)`` returns False.

    - When ``x`` is a python float, ``isna(x)`` returns False for all values of
      ``x`` except for the float ``nan`` value.

    - ``isna(None)`` produces True.


.. function:: ldexp(x, n)

    Multiply ``x`` by 2 raised to the power ``y``, i.e. compute
    :math:`x \cdot 2^y`. Column ``x`` is expected to be float, and ``y`` integer.


.. function:: rint(x)

    Round ``x`` to the nearest integer.


.. function:: sign(x)

    The sign of ``x``, returned as float.

    This function returns 1.0 if ``x`` is positive (including positive
    infinity), -1.0 if ``x`` is negative, 0.0 if ``x`` is zero, and NA if
    ``x`` is NA.


.. function:: signbit(x)

    Returns True if ``x`` is negative (its sign bit is set), and False if
    ``x`` is positive. This function is able to distinguish between -0.0 and
    +0.0, returning True/False respectively. If ``x`` is an NA value, this
    function will also return NA.


.. function:: trunc(x)

    The nearest integer value not greater than ``x`` in magnitude.

    If ``x`` is integer or boolean, then ``trunc()`` will return this value
    converted to float64. If ``x`` is floating-point, then ``trunc(x)`` acts as
    ``floor(x)`` for positive values of ``x``, and as ``ceil(x)`` for negative
    values of ``x``. This rounding mode is also called rounding towards zero.



Miscellaneous functions
-----------------------



Mathematical constants
----------------------

.. attribute:: e

    The base of the natural logarithm, also known as the Euler's number.
    Its value is ``2.718281828459045``.


.. attribute:: golden

    The golden ratio :math:`\varphi = (1 + \sqrt{5})/2`. The value is
    ``1.618033988749895``.


.. attribute:: inf

    Positive infinity.


.. attribute:: nan

    Not-a-number, a special floating-point constant that denotes a missing
    number. In most ``datatable`` functions you can use ``None`` instead
    of ``nan``.


.. attribute:: pi

    Mathematical constant :math:`\pi = \frac12\tau`, the area of a circle with
    unit radius. The constant is stored with float64 precision, and its value is
    ``3.141592653589793``.


.. attribute:: tau

    Mathematical constant :math:`\tau = 2\pi`, the circumference of a circle
    with unit radius. Some mathematicians believe that :math:`\tau` is the
    `true circle constant`_, and :math:`\pi` is an impostor. The value
    of :math:`\tau` is ``6.283185307179586``.



.. _`math module`: https://docs.python.org/3/library/math.html
.. _`numpy math functions`: https://docs.scipy.org/doc/numpy-1.13.0/reference/routines.math.html
.. _`true circle constant`: https://hexnet.org/files/documents/tau-manifesto.pdf
