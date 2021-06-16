
.. xattr:: datatable.Type.int32
    :src: --

    Integer type, corresponding to ``int32_t`` in C. This type uses 4 bytes per
    data element, and can store values in the range ``-2,147,483,647 ..
    2,147,483,647``.

    This is the most common type for handling integer data. When a python list
    of integers is converted into a Frame, a column of this type will usually
    be created.


    Examples
    --------
    >>> DT = dt.Frame([None, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55])
    >>> DT
       |    C0
       | int32
    -- + -----
     0 |    NA
     1 |     1
     2 |     1
     3 |     2
     4 |     3
     5 |     5
     6 |     8
     7 |    13
     8 |    21
     9 |    34
    10 |    55
    [11 rows x 1 column]
