
.. xclass:: datatable.ltype
    :src: src/datatable/types.py ltype

    .. x-version-deprecated:: 1.0.0

        This class is deprecated and will be removed in version 1.2.0.
        Please use :class:`dt.Type` instead.

    Enumeration of possible "logical" types of a column.

    Logical type is the type stripped away from the details of its physical
    storage. For example, ``ltype.int`` represents an integer. Under the hood,
    this integer can be stored in several "physical" formats: from
    ``stype.int8`` to ``stype.int64``. Thus, there is a one-to-many relationship
    between ltypes and stypes.


    Values
    ------
    The following ltype values are currently available:

        - `ltype.bool`
        - `ltype.int`
        - `ltype.real`
        - `ltype.str`
        - `ltype.time`
        - `ltype.obj`


    Methods
    -------
    .. list-table::
        :widths: auto
        :class: api-table

        * - :meth:`ltype(x) <datatable.ltype.__new__>`
          - Find ltype corresponding to value ``x``.

        * - :meth:`.stypes()`
          - The list of :class:`dt.stype`s that correspond to this ltype.

    Examples
    --------
    >>> dt.ltype.bool
    ltype.bool
    >>> dt.ltype("int32")
    ltype.int

    For each ltype, you can find the set of stypes that correspond to it:

    >>> dt.ltype.real.stypes
    [stype.float32, stype.float64]
    >>> dt.ltype.time.stypes
    []


.. toctree::
    :hidden:

    .__new__()    <ltype/__new__>
    .stypes       <ltype/stypes>
