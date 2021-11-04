
.. xfunction:: datatable.rowsd
    :src: src/core/expr/fnary/rowsd.cc FExpr_RowSd::apply_function
    :tests: tests/ijby/test-rowwise.py
    :cvar: doc_dt_rowsd
    :signature: rowsd(*cols)

    For each row, find the standard deviation among the columns from `cols`
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

        >>> from datatable import dt, f, rowsd
        >>> DT = dt.Frame({'name': ['A', 'B', 'C', 'D', 'E'],
        ...                'group': ['mn', 'mn', 'kl', 'kl', 'fh'],
        ...                'S1': [1, 4, 5, 6, 7],
        ...                'S2': [2, 3, 8, 5, 1],
        ...                'S3': [8, 5, 2, 5, 3]}

        >>> DT
           | name   group     S1     S2     S3
           | str32  str32  int32  int32  int32
        -- + -----  -----  -----  -----  -----
         0 | A      mn         1      2      8
         1 | B      mn         4      3      5
         2 | C      kl         5      8      2
         3 | D      kl         6      5      5
         4 | E      fh         7      1      3
        [5 rows x 5 columns]

    Get the row standard deviation for all integer columns::

        >>> DT[:, rowsd(f[int])]
           |      C0
           | float64
        -- + -------
         0 | 3.78594
         1 | 1
         2 | 3
         3 | 0.57735
         4 | 3.05505
        [5 rows x 1 column]

    Get the row standard deviation for some columns::

        >>> DT[:, rowsd(f[2, 3])]
           |       C0
           |  float64
        -- + --------
         0 | 0.707107
         1 | 0.707107
         2 | 2.12132
         3 | 0.707107
         4 | 4.24264
        [5 rows x 1 column]


    See Also
    --------
    - :func:`rowmean()` -- calculate the mean value row-wise.
