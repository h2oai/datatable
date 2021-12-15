
.. xfunction:: datatable.rowmax
    :src: src/core/expr/fnary/rowminmax.cc FExpr_RowMinMax<MIN,ARG>::apply_function
    :tests: tests/ijby/test-rowwise.py
    :cvar: doc_dt_rowmax
    :signature: rowmax(*cols)

    For each row, find the largest value among the columns from `cols`.


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

        >>> DT[:, dt.rowmax(f[:])]
           |    C0
           | int32
        -- + -----
         0 |     1
         1 |     2
         2 |     3
         3 |     4
         4 |     2
        [5 rows x 1 column]


    See Also
    --------
    - :func:`rowmin()` -- find the smallest element row-wise.
