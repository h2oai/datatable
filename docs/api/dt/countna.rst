
.. xfunction:: datatable.countna
    :src: src/core/expr/fexpr_count_countna.cc pyfn_countna
    :tests: tests/test-reduce.py
    :cvar: doc_dt_countna
    :signature: countna(cols)

    .. x-version-added:: 1.1.0

    Count the number of NA values for each column from `cols`. 

    Parameters
    ----------
    cols: FExpr
        Input columns.

    return: Expr
        f-expression having one row, and the same names and number of columns
        as in `cols`. All the returned column stypes are `int64`.
        If `cols` is not provided, 0 is returned per group.

    except: TypeError
        The exception is raised when one of the columns from `cols`
        has an obj64 type.



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

    Get the count of NAs of all rows:

        >>> df[:, dt.countna(f[:])]
           |     A      B      C
           | int64  int64  int64
        -- + -----  -----  -----
         0 |     1      2      0
        [1 row x 3 columns]

    Get the count of NAs of column `B`:

        >>> df[:, dt.countna(f.B)]
           |     B
           | int64
        -- + -----
         0 |     2
        [1 row x 1 column]



    See Also
    --------

    - :func:`count()` -- function to count the number of non-missing values.
