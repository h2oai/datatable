
.. xattr:: datatable.Type.is_temporal
    :src: src/core/types/py_type.cc is_temporal
    :cvar: doc_Type_is_temporal

    .. x-version-added:: 1.1.0

    Test whether this type belongs to the category of time-related types. This
    property returns ``True`` for :attr:`date32 <dt.Type.date32>` and
    :attr:`time64 <dt.Type.time64>` types only.


    Parameters
    ----------
    return: bool


    Examples
    --------
    >>> dt.Type.date32.is_temporal
    True
    >>> dt.Type.time64.is_temporal
    True
    >>> dt.Type.str32.is_temporal
    False
