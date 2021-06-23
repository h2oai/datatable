
.. xattr:: datatable.Type.str32
    :src: --

    The type of a column with string data.

    Internally, this column stores data using 2 buffers: a character buffer,
    where all values in the column are stored together as a single large
    concatenated string in UTF8 encoding, and an ``int32`` array of offsets
    into the character buffer. Consequently, this type can only store up to
    2Gb of total character data per column.

    Whenever any operation on a string column exceeds the 2Gb limit, this type
    will be silently replaced with :attr:`dt.Type.str64`.

    A virtual column that produces string data may have either ``str32`` or
    ``str64`` type regardless of how it stores its data.

    This column converts to ``str`` type in Python, ``pa.string()`` in
    pyarrow, and ``dtype('object')`` in numpy and pandas.


    Examples
    --------
    >>> DT = dt.Frame({"to persist": ["one teaspoon", "at a time,",
    ...                               "the rain turns", "mountains", "into valleys"]})
    >>> DT
       | to persist
       | str32
    -- + --------------
     0 | one teaspoon
     1 | at a time,
     2 | the rain turns
     3 | mountains
     4 | into valleys
    [5 rows x 1 column]
