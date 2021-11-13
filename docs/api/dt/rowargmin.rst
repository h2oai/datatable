
.. xfunction:: datatable.rowargmin
    :src: src/core/expr/fnary/rowminmax.cc FExpr_RowMinMax<MIN,RETARGS>::apply_function
    :tests: tests/ijby/test-rowwise.py
    :cvar: doc_dt_rowargmin
    :signature: rowargmin(*cols)

    For each row, find the smallest value among the columns from `cols`.


    Parameters
    ----------
    cols: FExpr
        Input columns.

    return: FExpr
        f-expression consisting of one column that has the same number of rows
        as in `cols`. The column stype `int64`.

    except: TypeError
        The exception is raised when `cols` has non-numeric columns.


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

    ::

        >>> DT[:, dt.rowargmin(f[:])]
           |    C0
           | int64
        -- + -----
         0 |     0
         1 |     2
         2 |     2
         3 |     0
         4 |     2
        [5 rows x 1 column]


    See Also
    --------
    - :func:`rowargmax()` -- find the index of the largest element row-wise.
