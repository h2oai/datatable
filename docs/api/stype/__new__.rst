
.. xmethod:: datatable.stype.__new__
    :src: src/datatable/types.py ___new___

    __new__(self, value)
    --

    Find an stype corresponding to `value`.

    This method is called when you attempt to construct a new
    :class:`dt.stype` object, for example ``dt.stype(int)``. Instead of
    actually creating any new stypes, we return one of the existing
    stype values.


    Parameters
    ----------
    value: str | type | np.dtype
        An object that will be converted into an stype. This could be
        a string such as `"integer"` or `"int"` or `"int8"`, a python
        type such as `bool` or `float`, or a numpy dtype.

    return: stype
        An :class:`dt.stype` that corresponds to the input `value`.

    except: ValueError
        Raised if `value` does not correspond to any stype.


    Examples
    --------
    >>> dt.stype(str)
    stype.str64

    >>> dt.stype("double")
    stype.float64

    >>> dt.stype(numpy.dtype("object"))
    stype.obj64

    >>> dt.stype("int64")
    stype.int64
