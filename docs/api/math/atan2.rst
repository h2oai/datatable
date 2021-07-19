
.. xfunction:: datatable.math.atan2
    :src: src/core/expr/fbinary/math.cc resolve_fn_atan2
    :cvar: doc_math_atan2
    :signature: atan2(x, y)

    The inverse trigonometric tangent of ``y/x``, taking into account the signs
    of ``x`` and ``y`` to produce the correct result.


    If ``(x,y)`` is a point in a Cartesian plane, then ``arctan2(y, x)`` returns
    the radian measure of an angle formed by two rays: one starting at the origin
    and passing through point ``(0,1)``, and the other starting at the origin
    and passing through point ``(x,y)``. The angle is assumed positive if the
    rotation from the first ray to the second occurs counter-clockwise, and
    negative otherwise.

    As a special case, ``arctan2(0, 0) == 0``, and ``arctan2(0, -1) == tau/2``.


    See also
    --------
    - :func:`arctan(x)` -- inverse tangent function.
