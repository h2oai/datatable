
.. xfunction:: datatable.count
    :src: src/core/expr/fexpr_count.cc pyfn_count
    :cvar: doc_dt_count
    :tests: tests/test-reduce.py
    :signature: count(cols=None)

    Count non-missing values for each column from `cols`. When called
    with no arguments, simply count the number of rows. This function
    is group-aware.

    Parameters
    ----------
    cols: FExpr
        Input columns if any.

    return: FExpr
        f-expression that counts the number of non-missing values
        for each column from `cols`. If `cols` is not provided,
        it calculates the number of rows. The returned f-expression
        has as many rows as there are groups, it also has the same names and
        number of columns as in `cols`. All the resulting column's stypes
        are `int64`.


    Examples
    --------

    .. code-block:: python

        >>> from datatable import dt, f
        >>>
        >>> DT = dt.Frame({'A': [None, 1, 2, None, 2],
        ...                'B': [None, 2, 3, 4, 5],
        ...                'C': [1, 2, 1, 1, 2]})
        >>> DT
           |     A      B      C
           | int32  int32  int32
        -- + -----  -----  -----
         0 |    NA     NA      1
         1 |     1      2      2
         2 |     2      3      1
         3 |    NA      4      1
         4 |     2      5      2
        [5 rows x 3 columns]

    Count non-missing values in all the columns::

        >>> DT[:, dt.count(f[:])]
           |     A      B      C
           | int64  int64  int64
        -- + -----  -----  -----
         0 |     3      4      5
        [1 row x 3 columns]

    Count non-missing values in the column `B` only::

        >>> DT[:, dt.count(f.B)]
           |     B
           | int64
        -- + -----
         0 |     4
        [1 row x 1 column]

    Get the number of rows in a frame::

        >>> DT[:, dt.count()]
           | count
           | int64
        -- + -----
         0 |     5
        [1 row x 1 column]



    See Also
    --------

    - :func:`countna()` -- function to count the number of missing values.
