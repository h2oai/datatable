
.. xattr:: datatable.Type.is_boolean
    :src: src/core/types/py_type.cc is_boolean
    :cvar: doc_Type_is_boolean

    .. x-version-added:: 1.1.0

    Test whether this type belongs to the category of "boolean" types. This
    property returns ``True`` for :attr:`bool8 <dt.Type.bool8>` type only.


    Parameters
    ----------
    return: bool


    Examples
    --------
    >>> dt.Type.bool8.is_boolean
    True
    >>> dt.Type.void.is_boolean
    False
    >>> dt.Type.int8.is_boolean
    False
