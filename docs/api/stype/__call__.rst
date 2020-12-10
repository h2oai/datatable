
.. xmethod:: datatable.stype.__call__
    :src: src/datatable/types.py __call__

    __call__(self, col)
    --

    Cast column `col` into the new stype.

    An stype can be used as a function that converts columns into that
    specific stype. In the same way as you could write `int(3.14)` in
    Python to convert a float value into integer, you can likewise
    write `dt.int32(f.A)` to convert column `A` into stype `int32`.

    Parameters
    ----------
    col: FExpr
        A single- or multi- column expression. All columns will be
        converted into the desired stype.

    return: FExpr
        Expression that converts its inputs into the current stype.


    See Also
    --------
    :func:`dt.as_type()` -- equivalent method of casting a column into
       another stype.


    Examples
    --------

    ::

        from datatable import dt, f

        df = dt.Frame({'A': ['1', '1', '2', '1', '2'],
                       'B': [None, '2', '3', '4', '5'],
                       'C': [1, 2, 1, 1, 2]})

        df

    .. dtframe::
        :names: A,B,C
        :types: str32, str32, int32
        :shape: 5, 2

        0,1,NA,1
        1,1,2,2
        2,2,3,1
        3,1,4,1
        4,2,5,2

    Convert column A from string stype to integer stype::

        df[:, dt.int32(f.A)]

    .. dtframe::
        :names: A
        :types: int32
        :shape: 5, 1

        0,1
        1,1
        2,2
        3,1
        4,2

    Convert multiple columns to different stypes::

        df[:, [dt.int32(f.A), dt.str32(f.C)]]

    .. dtframe::
        :names: A,C
        :types: int32,str32
        :shape: 5, 2

        0,1,1
        1,1,2
        2,2,1
        3,1,1
        4,2,2

