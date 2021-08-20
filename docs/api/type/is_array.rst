
.. xattr:: datatable.Type.is_array
    :src: src/core/types/py_type.cc is_array
    :cvar: doc_Type_is_array

    .. x-version-added:: 1.1.0

    Test whether this type belongs to the category of "array" types. This
    property returns ``True`` for :meth:`arr32(T) <dt.Type.arr32>` and
    :meth:`arr64(T) <dt.Type.arr64>` types only.


    Parameters
    ----------
    return: bool


    Examples
    --------
    >>> dt.Type.arr32(int).is_array
    True
    >>> dt.Type.arr64("float32").is_array
    True
    >>> dt.Type.void.is_array
    False
