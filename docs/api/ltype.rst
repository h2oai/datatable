
.. xclass:: datatable.ltype
    :src: src/datatable/types.py ltype

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

        * - :attr:`.stypes <datatable.ltype.stypes>`
          - The list of :class:`stype`s that correspond to this ltype.

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
