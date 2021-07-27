
.. xattr:: datatable.Type.is_string
    :src: src/core/types/py_type.cc is_string
    :cvar: doc_Type_is_string

    .. x-version-added:: 1.1.0

    Test whether this type belongs to the category of "string" types. This
    property returns ``True`` for :attr:`str32 <dt.Type.str32>`,
    :attr:`str64 <dt.Type.str64>`, and :attr:`void <dt.Type.void>` types
    only.


    Parameters
    ----------
    return: bool


    Examples
    --------
    >>> dt.Type.str64.is_string
    True
    >>> dt.Type.void.is_string
    True
    >>> dt.Type.int64.is_string
    False
