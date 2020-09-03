
.. xclass:: datatable.FExpr
    :src: src/core/expr/fexpr.h FExpr
    :doc: src/core/expr/fexpr.cc doc_fexpr


    Arithmeritc operators
    ---------------------

    These operators apply when either ``x`` or ``y`` or both are ``FExpr``s.

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`x + y <FExpr.__add__>`
          - Addition;

        * - :meth:`x - y <FExpr.__sub__>`
          - Subtraction;

        * - :meth:`x * y <FExpr.__mul__>`
          - Multiplication;

        * - :meth:`x / y <FExpr.__truediv__>`
          - Division;

        * - :meth:`x // y <FExpr.__floordiv__>`
          - Integer division;

        * - :meth:`x % y <FExpr.__mod__>`
          - Modulus (remainder after integer division);



.. toctree::
    :hidden:

    .__add__()      <fexpr/__add__>
    .__sub__()      <fexpr/__sub__>
    .__mul__()      <fexpr/__mul__>
    .__truediv__()  <fexpr/__truediv__>
    .__floordiv__() <fexpr/__floordiv__>
    .__mod__()      <fexpr/__mod__>
