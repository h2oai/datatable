
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
