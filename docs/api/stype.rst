
.. xclass:: datatable.stype
    :src: src/datatable/types.py stype

    .. x-version-deprecated:: 1.0.0

        This class is deprecated and will be removed in version 1.2.0.
        Please use :class:`dt.Type` instead.

    Enumeration of possible "storage" types of columns in a Frame.

    Each column in a Frame is a vector of values of the same type. We call
    this column's type the "stype". Most stypes correspond to primitive C types,
    such as ``int32_t`` or ``double``. However some stypes (corresponding to
    strings and categoricals) have a more complicated underlying structure.

    Notably, :mod:`datatable` does not support arbitrary structures as
    elements of a Column, so the set of stypes is small.


    Values
    ------
    The following stype values are currently available:

        - `stype.bool8`
        - `stype.int8`
        - `stype.int16`
        - `stype.int32`
        - `stype.int64`
        - `stype.float32`
        - `stype.float64`
        - `stype.str32`
        - `stype.str64`
        - `stype.obj64`

    They are available either as properties of the `dt.stype` class,
    or directly as constants in the :mod:`dt. <datatable>` namespace.
    For example::

        >>> dt.stype.int32
        stype.int32
        >>> dt.int64
        stype.int64

    Methods
    -------
    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`stype(x) <datatable.stype.__new__>`
          - Find stype corresponding to value ``x``.

        * - :meth:`<stype>(col) <datatable.stype.__call__>`
          - Cast a column into the specific stype.

        * - :attr:`.ctype <datatable.stype.ctype>`
          - **ctypes** type corresponding to this stype.

        * - :attr:`.dtype <datatable.stype.dtype>`
          - **numpy** dtype corresponding to this stype.

        * - :attr:`.ltype <datatable.stype.ltype>`
          - :class:`dt.ltype` corresponding to this stype.

        * - :attr:`.struct <datatable.stype.struct>`
          - :ext-mod:`struct` string corresponding to this stype.

        * - :attr:`.min <datatable.stype.min>`
          - The smallest numeric value for this stype.

        * - :attr:`.max <datatable.stype.max>`
          - The largest numeric value for this stype.


.. toctree::
    :hidden:

    .__call__()   <stype/__call__>
    .__new__()    <stype/__new__>
    .ctype        <stype/ctype>
    .dtype        <stype/dtype>
    .ltype        <stype/ltype>
    .max          <stype/max>
    .min          <stype/min>
    .struct       <stype/struct>
