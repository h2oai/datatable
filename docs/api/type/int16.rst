
.. xattr:: datatable.Type.int16
    :src: --
    :tests: tests/types/test-int.py

    Integer type, corresponding to ``int16_t`` in C. This type uses 2 bytes per
    data element, and can store values in the range ``-32767 .. 32767``.

    Most arithmetic operations involving this type will produce a result of
    type ``int32``, which follows the convention of the C language.
