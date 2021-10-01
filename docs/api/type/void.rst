
.. xattr:: datatable.Type.void
    :src: --
    :tests: tests/types/test-void.py

    The type of a column where all values are NAs.

    In datatable, any column can have NA values in it. There is, however,
    a special type for a column where *all* values are NAs: ``void``. This
    type's special property is that it can be used in
    place where any other type could be expected.

    A column of this type does not occupy any storage space. Unlike other types,
    it does not use the validity buffer either: all values are known to be
    invalid.

    It converts into pyarrow's ``pa.null()`` type, or ``float64`` dtype in numpy.
    When converting into pandas, a Series of ``object`` type is created,
    consisting of python ``None`` objects.


    Examples
    --------
    >>> DT = dt.Frame([None, None, None])
    >>> DT.type
    Type.void
    >>> DT
       |   C0
       | void
    -- + ----
     0 |   NA
     1 |   NA
     2 |   NA
    [3 rows x 1 column]
