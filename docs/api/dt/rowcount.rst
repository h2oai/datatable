
.. xfunction:: datatable.rowcount
    :src: src/core/expr/fnary/rowcount.cc FExpr_RowCount::apply_function
    :tests: tests/ijby/test-rowwise.py
    :cvar: doc_dt_rowcount
    :signature: rowcount(*cols)

    For each row, count the number of non-missing values in `cols`.


    Parameters
    ----------
    cols: FExpr
        Input columns.

    return: FExpr
        f-expression consisting of one `int32` column and the same number
        of rows as in `cols`.


    Examples
    --------
    ::

        >>> from datatable import dt, f
        >>> DT = dt.Frame({"A": [1, 1, 2, 1, 2],
        ...                "B": [None, 2, 3, 4, None],
        ...                "C":[True, False, False, True, True]})
        >>> DT
           |     A      B      C
           | int32  int32  bool8
        -- + -----  -----  -----
         0 |     1     NA      1
         1 |     1      2      0
         2 |     2      3      0
         3 |     1      4      1
         4 |     2     NA      1
        [5 rows x 3 columns]



    Note the exclusion of null values in the count::

        >>> DT[:, dt.rowcount(f[:])]
           |    C0
           | int32
        -- + -----
         0 |     2
         1 |     3
         2 |     3
         3 |     3
         4 |     2
        [5 rows x 1 column]


    See Also
    --------
    - :func:`rowsum()` -- sum of all values row-wise.
