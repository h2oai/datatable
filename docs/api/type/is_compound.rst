
.. xattr:: datatable.Type.is_compound
    :src: src/core/types/py_type.cc is_compound
    :cvar: doc_Type_is_compound

    .. x-version-added:: 1.1.0

    Test whether this type represents a "compound" type, i.e. whether it may
    contain one or more dependent types. Currently, datatable implements only
    :meth:`arr32 <dt.Type.arr32>` and :meth:`arr64 <dt.Type.arr64>` compound
    types.


    Parameters
    ----------
    return: bool


    Examples
    --------
    >>> dt.Type.arr32(dt.int32).is_compound
    True
    >>> dt.Type.void.is_compound
    False
