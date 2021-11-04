
.. xattr:: datatable.Type.is_categorical
    :src: src/core/types/py_type.cc is_categorical
    :cvar: doc_Type_is_categorical

    .. x-version-added:: 1.1.0

    Test whether this type is a categorical one. This
    property returns ``True`` for :meth:`cat8(T) <dt.Type.cat8>`,
    :meth:`cat16(T) <dt.Type.cat16>` and :meth:`cat32(T) <dt.Type.cat32>` types only.


    Parameters
    ----------
    return: bool


    Examples
    --------
    >>> dt.Type.cat8(int).is_categorical
    True
    >>> dt.Type.cat32(dt.Type.str64).is_categorical
    True
    >>> dt.Type.void.is_categorical
    False
