
.. xfunction:: datatable.rowmin
    :src: src/core/expr/fnary/rowminmax.cc FExpr_RowMinMax<MIN>::apply_function
    :tests: tests/ijby/test-rowwise.py
    :cvar: doc_dt_rowmin
    :signature: rowmin(*cols)

    For each row, find the smallest value among the columns from `cols`,
    excluding missing values.


    Parameters
    ----------
    cols: FExpr
        Input columns.

    return: FExpr
        f-expression consisting of one column that has the same number of rows
        as in `cols`. The column stype is the smallest common stype
        for `cols`, but not less than `int32`.

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

        >>> DT[:, dt.rowmin(f[:])]
           |    C0
           | int32
        -- + -----
         0 |     1
         1 |     0
         2 |     0
         3 |     1
         4 |     1
        [5 rows x 1 column]


    See Also
    --------
    - :func:`rowmax()` -- find the largest element row-wise.
