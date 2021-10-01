
.. xfunction:: datatable.nunique
    :src: src/core/expr/head_reduce_unary.cc op_nunique
    :tests: tests/test-reduce.py
    :cvar: doc_dt_nunique
    :signature: nunique(cols)


    Count the number of unique values for each column from `cols`.

    Parameters
    ----------
    cols: Expr
        Input columns.

    return: Expr
        f-expression having one row, and the same names and number of columns
        as in `cols`. All the returned column stypes are `int64`.

    except: TypeError
        The exception is raised when one of the columns from `cols`
        has an unsupported type, e.g. `obj64`.



    Examples
    --------

    .. code-block:: python

        >>> from datatable import dt, f
        >>>
        >>> df = dt.Frame({'A': [1, 1, 2, None, 1, 2],
        ...                'B': [None, 2, 3, 4, None, 5],
        ...                'C': [1, 2, 1, 1, 2, 4]})
        >>> df
           |     A      B      C
           | int32  int32  int32
        -- + -----  -----  -----
         0 |     1     NA      1
         1 |     1      2      2
         2 |     2      3      1
         3 |    NA      4      1
         4 |     1     NA      2
         5 |     2      5      4
        [6 rows x 3 columns]

    Count the number of unique values for each column in the frame:

        >>> df[:, dt.nunique(f[:])]
           |     A      B      C
           | int64  int64  int64
        -- + -----  -----  -----
         0 |     2      4      3
        [1 row x 3 columns]

    Count the number of unique values for column `B` only:

        >>> df[:, dt.nunique(f.B)]
           |     B
           | int64
        -- + -----
         0 |     4
        [1 row x 1 column]
