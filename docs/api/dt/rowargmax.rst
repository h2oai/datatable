
.. xfunction:: datatable.rowargmax
    :src: src/core/expr/fnary/rowminmax.cc FExpr_RowMinMax<MIN,ARG>::apply_function
    :tests: tests/ijby/test-rowwise.py
    :cvar: doc_dt_rowargmax
    :signature: rowargmax(*cols)

    .. x-version-added:: 1.1.0

    For each row, find the index of the largest value among the columns from `cols`.
    When the largest value occurs more than once, the smallest column index
    is returned.


    Parameters
    ----------
    cols: FExpr
        Input columns.

    return: FExpr
        f-expression consisting of one column that has the same number of rows
        as in `cols`. The column type is `int64`.

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

        >>> DT[:, dt.rowargmax(f[:])]
           |    C0
           | int64
        -- + -----
         0 |     0
         1 |     1
         2 |     1
         3 |     1
         4 |     0
        [5 rows x 1 column]


    See Also
    --------
    - :func:`rowargmin()` -- find the index of the smallest element row-wise.
