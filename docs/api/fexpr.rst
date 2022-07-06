

.. xclass:: datatable.FExpr
    :src: src/core/expr/fexpr.h FExpr
    :cvar: doc_FExpr

    FExpr is an object that encapsulates computations to be done on a frame.

    FExpr objects are rarely constructed directly (though it is possible too),
    instead they are more commonly created as inputs/outputs from various
    functions in :mod:`datatable`.

    Consider the following example::

        math.sin(2 * f.Angle)

    Here accessing column "Angle" in namespace ``f`` creates an ``FExpr``.
    Multiplying this ``FExpr`` by a python scalar ``2`` creates a new ``FExpr``.
    And finally, applying the sine function creates yet another ``FExpr``. The
    resulting expression can be applied to a frame via the
    :meth:`DT[i,j] <dt.Frame.__getitem__>` method, which will compute that expression
    using the data of that particular frame.

    Thus, an ``FExpr`` is a stored computation, which can later be applied to a
    Frame, or to multiple frames.

    Because of its delayed nature, an ``FExpr`` checks its correctness at the time
    when it is applied to a frame, not sooner. In particular, it is possible for
    the same expression to work with one frame, but fail with another. In the
    example above, the expression may raise an error if there is no column named
    "Angle" in the frame, or if the column exists but has non-numeric type.

    Most functions in datatable that accept an ``FExpr`` as an input, return
    a new ``FExpr`` as an output, thus creating a tree of ``FExpr``s as the
    resulting evaluation graph.

    Also, all functions that accept ``FExpr``s as arguments, will also accept
    certain other python types as an input, essentially converting them into
    ``FExpr``s. Thus, we will sometimes say that a function accepts **FExpr-like**
    objects as arguments.

    All binary operators ``op(x, y)`` listed below work when either ``x``
    or ``y``, or both are ``FExpr``s.


    Construction
    ------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`.__init__(e)`
          - Create an ``FExpr``.

        * - :meth:`.extend()`
          - Append another FExpr.

        * - :meth:`.remove()`
          - Remove columns from the FExpr.


    Arithmeritc operators
    ---------------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`__add__(x, y) <dt.FExpr.__add__>`
          - Addition ``x + y``.

        * - :meth:`__sub__(x,  y) <dt.FExpr.__sub__>`
          - Subtraction ``x - y``.

        * - :meth:`__mul__(x, y) <dt.FExpr.__mul__>`
          - Multiplication ``x * y``.

        * - :meth:`__truediv__(x, y) <dt.FExpr.__truediv__>`
          - Division ``x / y``.

        * - :meth:`__floordiv__(x, y) <dt.FExpr.__floordiv__>`
          - Integer division ``x // y``.

        * - :meth:`__mod__(x, y) <dt.FExpr.__mod__>`
          - Modulus ``x % y`` (the remainder after integer division).

        * - :meth:`__pow__(x, y) <dt.FExpr.__pow__>`
          - Power ``x ** y``.

        * - :meth:`__pos__(x) <dt.FExpr.__pos__>`
          - Unary plus ``+x``.

        * - :meth:`__neg__(x) <dt.FExpr.__neg__>`
          - Unary minus ``-x``.


    Bitwise operators
    -----------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`__and__(x, y) <dt.FExpr.__and__>`
          - Bitwise AND ``x & y``.

        * - :meth:`__or__(x, y) <dt.FExpr.__or__>`
          - Bitwise OR ``x | y``.

        * - :meth:`__xor__(x, y) <dt.FExpr.__xor__>`
          - Bitwise XOR ``x ^ y``.

        * - :meth:`__invert__(x) <dt.FExpr.__invert__>`
          - Bitwise NOT ``~x``.

        * - :meth:`__lshift__(x, y) <dt.FExpr.__lshift__>`
          - Left shift ``x << y``.

        * - :meth:`__rshift__(x, y) <dt.FExpr.__rshift__>`
          - Right shift ``x >> y``.


    Relational operators
    --------------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`__eq__(x, y) <dt.FExpr.__eq__>`
          - Equal ``x == y``.

        * - :meth:`__ne__(x, y) <dt.FExpr.__ne__>`
          - Not equal ``x != y``.

        * - :meth:`__lt__(x, y) <dt.FExpr.__lt__>`
          - Less than ``x < y``.

        * - :meth:`__le__(x, y) <dt.FExpr.__le__>`
          - Less than or equal ``x <= y``.

        * - :meth:`__gt__(x, y) <dt.FExpr.__gt__>`
          - Greater than ``x > y``.

        * - :meth:`__ge__(x, y) <dt.FExpr.__ge__>`
          - Greater than or equal ``x >= y``.


    Equivalents of base datatable functions
    ---------------------------------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`.as_type()`
          - Same as :func:`dt.as_type()`.

        * - :meth:`.bfill()`
          - Same as :func:`dt.bfill()`.

        * - :meth:`.count()`
          - Same as :func:`dt.count()`.

        * - :meth:`.countna()`
          - Same as :func:`dt.countna()`.

        * - :meth:`.cummin()`
          - Same as :func:`dt.cummin()`.

        * - :meth:`.cummax()`
          - Same as :func:`dt.cummax()`.

        * - :meth:`.cumprod()`
          - Same as :func:`dt.cumprod()`.

        * - :meth:`.cumsum()`
          - Same as :func:`dt.cumsum()`.

        * - :meth:`.first()`
          - Same as :func:`dt.first()`.

        * - :meth:`.ffill()`
          - Same as :func:`dt.ffill()`.

        * - :meth:`.last()`
          - Same as :func:`dt.last()`.

        * - :meth:`.max()`
          - Same as :func:`dt.max()`.

        * - :meth:`.mean()`
          - Same as :func:`dt.mean()`.

        * - :meth:`.median()`
          - Same as :func:`dt.median()`.

        * - :meth:`.min()`
          - Same as :func:`dt.min()`.

        * - :meth:`.nunique()`
          - Same as :func:`dt.nunique()`.

        * - :meth:`.prod()`
          - Same as :func:`dt.prod()`.

        * - :meth:`.rowall()`
          - Same as :func:`dt.rowall()`.

        * - :meth:`.rowany()`
          - Same as :func:`dt.rowany()`.

        * - :meth:`.rowcount()`
          - Same as :func:`dt.rowcount()`.

        * - :meth:`.rowfirst()`
          - Same as :func:`dt.rowfirst()`.

        * - :meth:`.rowlast()`
          - Same as :func:`dt.rowlast()`.

        * - :meth:`.rowargmax()`
          - Same as :func:`dt.rowargmax()`.

        * - :meth:`.rowmax()`
          - Same as :func:`dt.rowmax()`.

        * - :meth:`.rowmean()`
          - Same as :func:`dt.rowmean()`.

        * - :meth:`.rowargmin()`
          - Same as :func:`dt.rowargmin()`.

        * - :meth:`.rowmin()`
          - Same as :func:`dt.rowmin()`.

        * - :meth:`.rowsd()`
          - Same as :func:`dt.rowsd()`.

        * - :meth:`.rowsum()`
          - Same as :func:`dt.rowsum()`.

        * - :meth:`.sd()`
          - Same as :func:`dt.sd()`.

        * - :meth:`.shift()`
          - Same as :func:`dt.shift()`.

        * - :meth:`.sum()`
          - Same as :func:`dt.sum()`.

    Miscellaneous
    -------------

    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`.__bool__()`
          - Implicitly convert FExpr into a boolean value.

        * - :meth:`.__getitem__()`
          - Apply slice to a string column.

        * - :meth:`.__repr__()`
          - Used by Python function :ext-func:`repr() <repr>`.

        * - :meth:`.len()`
          - String length.

        * - :meth:`.re_match(pattern)`
          - Check whether the string column matches a pattern.


.. toctree::
    :hidden:

    .__add__()      <fexpr/__add__>
    .__and__()      <fexpr/__and__>
    .__bool__()     <fexpr/__bool__>
    .__eq__()       <fexpr/__eq__>
    .__floordiv__() <fexpr/__floordiv__>
    .__ge__()       <fexpr/__ge__>
    .__getitem__()  <fexpr/__getitem__>
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
    .as_type()      <fexpr/as_type>
    .bfill()        </fexpr/bfill>
    .count()        <fexpr/count>
    .countna()      <fexpr/countna>
    .cummin()       <fexpr/cummin>
    .cummax()       <fexpr/cummax>
    .cumprod()      <fexpr/cumprod>
    .cumsum()       <fexpr/cumsum>
    .extend()       <fexpr/extend>
    .first()        <fexpr/first>
    .ffill()        </fexpr/ffill>
    .last()         <fexpr/last>
    .len()          <fexpr/len>
    .max()          <fexpr/max>
    .mean()         <fexpr/mean>
    .median()       <fexpr/median>
    .min()          <fexpr/min>
    .nunique()      <fexpr/nunique>
    .prod()         <fexpr/prod>
    .re_match()     <fexpr/re_match>
    .remove()       <fexpr/remove>
    .rowall()       <fexpr/rowall>
    .rowany()       <fexpr/rowany>
    .rowcount()     <fexpr/rowcount>
    .rowfirst()     <fexpr/rowfirst>
    .rowlast()      <fexpr/rowlast>
    .rowargmax()    <fexpr/rowargmax>
    .rowmax()       <fexpr/rowmax>
    .rowmean()      <fexpr/rowmean>
    .rowargmin()    <fexpr/rowargmin>
    .rowmin()       <fexpr/rowmin>
    .rowsd()        <fexpr/rowsd>
    .rowsum()       <fexpr/rowsum>
    .sd()           <fexpr/sd>
    .shift()        <fexpr/shift>
    .sum()          <fexpr/sum>


