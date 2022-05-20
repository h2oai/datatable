
.. xfunction:: datatable.prod
    :src: src/core/expr/head_reduce_unary.cc prod_reducer
    :tests: tests/test-reduce.py
    :cvar: doc_dt_prod
    :signature: prod(cols)

    .. x-version-added:: 1.1.0

    Calculate the product of values for each column from `cols`. The product
    of missing values is calculated as one.

    Parameters
    ----------
    cols: FExpr
        Input columns.

    return: FExpr
        f-expression having one row, and the same names and number of columns
        as in `cols`. The column stypes are `int64` for
        boolean and integer columns, `float32` for `float32` columns
        and `float64` for `float64` columns.

    except: TypeError
        The exception is raised when one of the columns from `cols`
        has a non-numeric type.


    Examples
    --------

    .. code-block:: python

        >>> from datatable import dt, f, by, prod
        >>>
        >>> DT = dt.Frame({'A': [1, 1, 2, 1, 2],
        ...                'B': [None, 2, 3, 4, 5],
        ...                'C': [1, 2, 1, 1, 2]})
        >>> DT
           |     A      B      C
           | int32  int32  int32
        -- + -----  -----  -----
         0 |     1     NA      1
         1 |     1      2      2
         2 |     2      3      1
         3 |     1      4      1
         4 |     2      5      2
        [5 rows x 3 columns]

    Get the product for column A::

        >>> DT[:, prod(f.A)]
           |     A
           | int64
        -- + -----
         0 |     4
        [1 row x 1 column]

    Get the product for multiple columns::

        >>> DT[:, prod(f['A', 'B'])]
           |     A      B
           | int64  int64
        -- + -----  -----
         0 |     4    120
        [1 row x 2 columns]


    In the presence of :func:`by()`, it returns the product of the specified columns per group::

        >>> DT[:, prod(f['A', 'B']), by('C')]
           |     C      A      B
           | int32  int64  int64
        -- + -----  -----  -----
         0 |     1      2     12
         1 |     2      2     10
        [2 rows x 3 columns]

