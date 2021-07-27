
.. xattr:: datatable.Type.is_float
    :src: src/core/types/py_type.cc is_float
    :cvar: doc_Type_is_float

    .. x-version-added:: 1.1.0

    Test whether this type belongs to the category of "float" types. This
    property returns ``True`` for :attr:`float32 <dt.Type.float32>`,
    :attr:`float64 <dt.Type.float64>`, and :attr:`void <dt.Type.void>` types
    only.


    Parameters
    ----------
    return: bool


    Examples
    --------
    >>> dt.Type.float64.is_float
    True
    >>> dt.Type.void.is_float
    True
    >>> dt.Type.int64.is_float
    False
