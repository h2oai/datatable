
.. xfunction:: datatable.count
    :src: src/core/expr/fexpr_count_countna.cc pyfn_count
    :cvar: doc_dt_count
    :tests: tests/test-reduce.py
    :signature: count(cols=None)

    Calculate the number of non-missing values for each column from `cols`, if `cols` is provided, 
    or the total number of rows if `cols` is not provided.

    Parameters
    ----------
    cols: FExpr
        Input columns. If no `cols` is passed, then the count of all rows is returned.

    return: Expr
        f-expression having one row, and the same names and number of columns
        as in `cols`. All the returned column stypes are `int64`.

    except: TypeError
        The exception is raised when one of the columns from `cols`
        has a non-numeric and non-string type.


    Examples
    --------

    .. code-block:: python

        >>> from datatable import dt, f
        >>>
        >>> df = dt.Frame({'A': [1, 1, 2, 1, 2],
        ...                'B': [None, 2, 3, 4, 5],
        ...                'C': [1, 2, 1, 1, 2]})
        >>> df
           |     A      B      C
           | int32  int32  int32
        -- + -----  -----  -----
         0 |     1     NA      1
         1 |     1      2      2
         2 |     2      3      1
         3 |     1      4      1
         4 |     2      5      2
        [5 rows x 3 columns]

    Get the count of all rows::

        >>> df[:, dt.count()]
           | count
           | int32
        -- + -----
         0 |     5
        [1 row x 1 column]

    Get the count of column `B` (note how the null row is excluded from the
    count result)::

        >>> df[:, dt.count(f.B)]
           |     B
           | int64
        -- + -----
         0 |     4
        [1 row x 1 column]


    See Also
    --------

    - :func:`sum()` -- function to calculate the sum of values.
