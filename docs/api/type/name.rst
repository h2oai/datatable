
.. xattr:: datatable.Type.name
    :src: src/core/types/py_type.cc get_name
    :cvar: doc_Type_name

    Return the canonical name of this type, as a string.

    Examples
    --------
    >>> dt.Type.int64.name
    'int64'
    >>> dt.Type(np.bool_).name
    'bool8'
