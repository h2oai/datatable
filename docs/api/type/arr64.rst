
.. xmethod:: datatable.Type.arr64
    :src: src/core/types/py_type.cc PyType::array
    :cvar: doc_Type_arr64
    :signature: arr64(T)

    .. x-version-added:: 1.1.0

    Array type with 64-bit offsets. In a column of this type, every element
    is a list of values of type `T`.


    See Also
    --------
    :meth:`.arr32(T)` -- another array type, but with 32-bit offsets.
