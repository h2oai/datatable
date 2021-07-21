
.. xattr:: datatable.Type.bool8
    :src: --
    :tests: tests/types/test-bool8.py

    The type of a column with boolean data.

    In a column of this type each data element is stored as 1 byte. NA values
    are also supported.

    The boolean type is considered numeric, where ``True`` is 1 and ``False``
    is 0.


    Examples
    --------
    >>> DT = dt.Frame([True, False, None]).type
    >>> DT.type
    Type.bool8
    >>> DT
       |    C0
       | bool8
    -- + -----
     0 |     1
     1 |     0
     2 |    NA
    [3 rows x 1 column]
