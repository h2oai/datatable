
.. xattr:: datatable.Type.int8
    :src: --
    :tests: tests/types/test-int.py

    Integer type that uses 1 byte per data element and can store values in the
    range ``-127 .. 127``.

    This type corresponds to ``int8_t`` in C/C++, ``int`` in python,
    ``np.dtype('int8')`` in numpy, and ``pa.int8()`` in pyarrow.

    Most arithmetic operations involving this type will produce a result of
    type ``int32``, which follows the convention of the C language.


    Examples
    --------
    >>> dt.Type.int8
    Type.int8
    >>> dt.Type('int8')
    Type.int8
    >>> dt.Frame([1, 0, 1, 1, 0]).types
    [Type.int8]
