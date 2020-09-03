
.. xclass:: datatable.FExpr
    :src: src/core/expr/fexpr.h FExpr
    :doc: src/core/expr/fexpr.cc doc_fexpr

    All binary operators ``op(x, y)`` listed below work when either ``x``
    or ``y``, or both are ``FExpr``s.


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

        * - :meth:`__inverse__(x) <FExpr.__inverse__>`
          - Bitwise NOT ``~x``.

        * - :meth:`__lshift__(x, y) <FExpr.__lshift__>`
          - Left shift ``x << y``.

        * - :meth:`__rshift__(x, y) <FExpr.__rshift__>`
          - Right shift ``x >> y``.


.. toctree::
    :hidden:

    .__add__()      <fexpr/__add__>
    .__and__()      <fexpr/__and__>
    .__floordiv__() <fexpr/__floordiv__>
    .__mod__()      <fexpr/__mod__>
    .__mul__()      <fexpr/__mul__>
    .__neg__()      <fexpr/__neg__>
    .__or__()       <fexpr/__or__>
    .__pos__()      <fexpr/__pos__>
    .__pow__()      <fexpr/__pow__>
    .__sub__()      <fexpr/__sub__>
    .__truediv__()  <fexpr/__truediv__>
    .__xor__()      <fexpr/__xor__>
