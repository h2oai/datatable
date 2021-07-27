
.. xattr:: datatable.Type.is_numeric
    :src: src/core/types/py_type.cc is_numeric
    :cvar: doc_Type_is_numeric

    .. x-version-added:: 1.1.0

    Test whether this type belongs to the category of "numeric" types. All
    boolean, integer, float and void types are considered numeric. Therefore,
    this method returns ``True`` for types

        - :attr:`void <dt.Type.void>`
        - :attr:`bool8 <dt.Type.bool8>`
        - :attr:`int8 <dt.Type.int8>`
        - :attr:`int16 <dt.Type.int16>`
        - :attr:`int32 <dt.Type.int32>`
        - :attr:`int64 <dt.Type.int64>`
        - :attr:`float32 <dt.Type.float32>`
        - :attr:`float64 <dt.Type.float64>`


    Parameters
    ----------
    return: bool


    Examples
    --------
    >>> dt.Type.int8.is_numeric
    True
    >>> dt.Type.float64.is_numeric
    True
    >>> dt.Type.void.is_numeric
    True
    >>> dt.Type.bool8.is_numeric
    True
    >>> dt.Type.str32.is_numeric
    False
