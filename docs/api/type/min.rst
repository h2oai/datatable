
.. xattr:: datatable.Type.min
    :src: src/core/types/py_type.cc get_min
    :cvar: doc_Type_min

    The smallest finite value that this type can represent, if applicable.

    Parameters
    ----------
    return: Any
        The type of the returned value corresponds to the Type object: an int
        for integer types, a float for floating-point types, etc. If the type
        has no well-defined min value then `None` is returned.


    Examples
    --------
    >>> dt.Type.int8.min
    -127
    >>> dt.Type.float32.min
    -3.4028234663852886e+38
    >>> dt.Type.date32.min
    -2147483647


    See also
    --------
    :attr:`.max` -- the largest value for the type.
