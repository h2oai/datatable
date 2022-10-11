
.. xmethod:: datatable.Type.cat32
    :src: src/core/types/py_type.cc PyType::categorical
    :cvar: doc_Type_cat32
    :signature: cat32(T)

    .. x-version-added:: 1.1.0

    Categorical type with 32-bit codes. In a column of this type, every element
    is a value of type `T`.


    See Also
    --------
    - :meth:`.cat8(T)` -- another categorical type, but with 8-bit codes.
    - :meth:`.cat16(T)` -- another categorical type, but with 16-bit codes.
