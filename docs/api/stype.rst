
.. xclass:: datatable.stype
    :src: src/datatable/types.py stype

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
          - :class:`ltype` corresponding to this stype.

        * - :attr:`.struct <datatable.stype.struct>`
          - :mod:`struct` string corresponding to this stype.

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
