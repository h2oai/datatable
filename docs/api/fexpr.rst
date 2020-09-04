
.. xclass:: datatable.FExpr
    :src: src/core/expr/fexpr.h FExpr
    :doc: src/core/expr/fexpr.cc doc_fexpr

    All binary operators ``op(x, y)`` listed below work when either ``x``
    or ``y``, or both are ``FExpr``s.


    Construction
    ------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`.__init__(e) <FExpr.__init__>`
          - Create an ``FExpr``.

        * - :meth:`.extend() <FExpr.extend>`
          - Append another FExpr.

        * - :meth:`.remove() <FExpr.remove>`
          - Remove columns from the FExpr.


    Arithmeritc operators
    ---------------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`__add__(x, y) <FExpr.__add__>`
          - Addition ``x + y``.

        * - :meth:`__sub__(x,  y) <FExpr.__sub__>`
          - Subtraction ``x - y``.

        * - :meth:`__mul__(x, y) <FExpr.__mul__>`
          - Multiplication ``x * y``.

        * - :meth:`__truediv__(x, y) <FExpr.__truediv__>`
          - Division ``x / y``.

        * - :meth:`__floordiv__(x, y) <FExpr.__floordiv__>`
          - Integer division ``x // y``.

        * - :meth:`__mod__(x, y) <FExpr.__mod__>`
          - Modulus ``x % y`` (the remainder after integer division).

        * - :meth:`__pow__(x, y) <FExpr.__pow__>`
          - Power ``x ** y``.

        * - :meth:`__pos__(x) <FExpr.__pos__>`
          - Unary plus ``+x``.

        * - :meth:`__neg__(x) <FExpr.__neg__>`
          - Unary minus ``-x``.


    Bitwise operators
    -----------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`__and__(x, y) <FExpr.__and__>`
          - Bitwise AND ``x & y``.

        * - :meth:`__or__(x, y) <FExpr.__or__>`
          - Bitwise OR ``x | y``.

        * - :meth:`__xor__(x, y) <FExpr.__xor__>`
          - Bitwise XOR ``x ^ y``.

        * - :meth:`__invert__(x) <FExpr.__invert__>`
          - Bitwise NOT ``~x``.

        * - :meth:`__lshift__(x, y) <FExpr.__lshift__>`
          - Left shift ``x << y``.

        * - :meth:`__rshift__(x, y) <FExpr.__rshift__>`
          - Right shift ``x >> y``.


    Relational operators
    --------------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`__eq__(x, y) <FExpr.__eq__>`
          - Equal ``x == y``.

        * - :meth:`__ne__(x, y) <FExpr.__ne__>`
          - Not equal ``x != y``.

        * - :meth:`__lt__(x, y) <FExpr.__lt__>`
          - Less than ``x < y``.

        * - :meth:`__le__(x, y) <FExpr.__le__>`
          - Less than or equal ``x <= y``.

        * - :meth:`__gt__(x, y) <FExpr.__gt__>`
          - Greater than ``x > y``.

        * - :meth:`__ge__(x, y) <FExpr.__ge__>`
          - Greater than or equal ``x >= y``.


    Miscellaneous
    -------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`.__bool__() <FExpr.__bool__>`
          - Implicitly convert FExpr into a boolean value.

        * - :meth:`.__repr__() <FExpr.__repr__>`
          - Used by Python function :func:`repr`.

        * - :meth:`.len() <FExpr.len>`
          - String length.

        * - :meth:`.re_match(pattern) <FExpr.re_match>`
          - Check whether the string column matches a pattern.


.. toctree::
    :hidden:

    .__add__()      <fexpr/__add__>
    .__and__()      <fexpr/__and__>
    .__bool__()     <fexpr/__bool__>
    .__eq__()       <fexpr/__eq__>
    .__floordiv__() <fexpr/__floordiv__>
    .__ge__()       <fexpr/__ge__>
    .__gt__()       <fexpr/__gt__>
    .__init__()     <fexpr/__init__>
    .__invert__()   <fexpr/__invert__>
    .__le__()       <fexpr/__le__>
    .__lshift__()   <fexpr/__lshift__>
    .__lt__()       <fexpr/__lt__>
    .__mod__()      <fexpr/__mod__>
    .__mul__()      <fexpr/__mul__>
    .__ne__()       <fexpr/__ne__>
    .__neg__()      <fexpr/__neg__>
    .__or__()       <fexpr/__or__>
    .__pos__()      <fexpr/__pos__>
    .__pow__()      <fexpr/__pow__>
    .__repr__()     <fexpr/__repr__>
    .__rshift__()   <fexpr/__rshift__>
    .__sub__()      <fexpr/__sub__>
    .__truediv__()  <fexpr/__truediv__>
    .__xor__()      <fexpr/__xor__>
    .extend()       <fexpr/extend>
    .len()          <fexpr/len>
    .re_match()     <fexpr/re_match>
    .remove()       <fexpr/remove>
