
.. xattr:: datatable.Type.is_integer
    :src: src/core/types/py_type.cc is_integer
    :cvar: doc_Type_is_integer

    .. x-version-added:: 1.1.0

    Test whether this type belongs to the category of "integer" types. This
    property returns ``True`` for :attr:`int8 <dt.Type.int8>`,
    :attr:`int16 <dt.Type.int16>`, :attr:`int32 <dt.Type.int32>`,
    :attr:`int64 <dt.Type.int64>`, and :attr:`void <dt.Type.void>` types.


    Parameters
    ----------
    return: bool


    Examples
    --------
    >>> dt.Type.int8.is_integer
    True
    >>> dt.Type.int64.is_integer
    True
    >>> dt.Type.void.is_integer
    True
    >>> dt.Type.float64.is_integer
    False
