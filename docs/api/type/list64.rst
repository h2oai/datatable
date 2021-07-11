
.. xmethod:: datatable.Type.list64
    :src: src/core/types/py_type.cc PyType::list64
    :cvar: doc_Type_list64
    :signature: list64(T)

    List type with 64-bit offsets. In a column of this type, every element
    is a list of types `T`.


    See Also
    --------
    :meth:`.list32(T)` -- another list type, but with 32-bit offsets.
