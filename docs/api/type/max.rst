
.. xattr:: datatable.Type.max
    :src: src/core/types/py_type.cc get_max
    :cvar: doc_Type_max

    The largest finite value that this type can represent, if applicable.

    Parameters
    ----------
    return: Any
        The type of the returned value corresponds to the Type object: an int
        for integer types, a float for floating-point types, etc. If the type
        has no well-defined max value then `None` is returned.


    Examples
    --------
    >>> dt.Type.int32.max
    2147483647
    >>> dt.Type.float64.max
    1.7976931348623157e+308
    >>> dt.Type.date32.max
    2146764179


    See also
    --------
    :attr:`.min` -- the smallest value for the type.
