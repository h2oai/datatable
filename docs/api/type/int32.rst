
.. xattr:: datatable.Type.int32
    :src: --

    Integer type, corresponding to ``int32_t`` in C. This type uses 4 bytes per
    data element, and can store values in the range ``-2,147,483,647 ..
    2,147,483,647``.

    This is the most common type for handling integer data. When a python list
    of integers is converted into a Frame, a column of this type will usually
    be created.
