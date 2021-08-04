
.. xfunction:: datatable.rowmean
    :src: src/core/expr/fnary/rowmean.cc FExpr_RowMean::apply_function
    :tests: tests/ijby/test-rowwise.py
    :cvar: doc_dt_rowmean
    :signature: rowmean(*cols)

    For each row, find the mean values among the columns from `cols`
    skipping missing values. If a row contains only the missing values,
    this function will produce a missing value too.


    Parameters
    ----------
    cols: FExpr
        Input columns.

    return: FExpr
        f-expression consisting of one column that has the same number of rows
        as in `cols`. The column stype is `float32` when all the `cols`
        are `float32`, and `float64` in all the other cases.

    except: TypeError
        The exception is raised when `cols` has non-numeric columns.

    Examples
    --------
    ::

        >>> from datatable import dt, f, rowmean
        >>> DT = dt.Frame({'a': [None, True, True, True],
        ...                'b': [2, 2, 1, 0],
        ...                'c': [3, 3, 1, 0],
        ...                'd': [0, 4, 6, 0],
        ...                'q': [5, 5, 1, 0]}

        >>> DT
           |     a      b      c      d      q
           | bool8  int32  int32  int32  int32
        -- + -----  -----  -----  -----  -----
         0 |    NA      2      3      0      5
         1 |     1      2      3      4      5
         2 |     1      1      1      6      1
         3 |     1      0      0      0      0
        [4 rows x 5 columns]


    Get the row mean of all columns::

        >>> DT[:, rowmean(f[:])]
           |      C0
           | float64
        -- + -------
         0 |     2.5
         1 |     3
         2 |     2
         3 |     0.2
        [4 rows x 1 column]

    Get the row mean of specific columns::

        >>> DT[:, rowmean(f['a', 'b', 'd'])]
           |       C0
           |  float64
        -- + --------
         0 | 1
         1 | 2.33333
         2 | 2.66667
         3 | 0.333333
        [4 rows x 1 column]



    See Also
    --------
    - :func:`rowsd()` -- calculate the standard deviation row-wise.

