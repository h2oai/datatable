
.. xattr:: datatable.Type.is_object
    :src: src/core/types/py_type.cc is_object
    :cvar: doc_Type_is_object

    .. x-version-added:: 1.1.0

    Test whether this type belongs to the category of "object" types. This
    property returns ``True`` for :attr:`obj64 <dt.Type.obj64>`, and
    :attr:`void <dt.Type.void>` types only.


    Parameters
    ----------
    return: bool


    Examples
    --------
    >>> dt.Type.obj64.is_object
    True
    >>> dt.Type.void.is_object
    True
    >>> dt.Type.int8.is_object
    False
