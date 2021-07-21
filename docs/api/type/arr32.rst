
.. xmethod:: datatable.Type.arr32
    :src: src/core/types/py_type.cc PyType::array
    :cvar: doc_Type_arr32
    :signature: arr32(T)

    Array type with 32-bit offsets. In a column of this type, every element
    is a list of values of type `T`.


    See Also
    --------
    :meth:`.arr64(T)` -- another array type, but with 64-bit offsets.
