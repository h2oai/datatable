
.. xfunction:: datatable.mean
    :src: src/core/expr/head_reduce_unary.cc mean_reducer
    :cvar: doc_dt_mean
    :tests: tests/test-reduce.py
    :signature: mean(cols)

    Calculate the mean value for each column from `cols`.


    Parameters
    ----------
    cols: FExpr
        Input columns.

    return: Expr
        f-expression having one row, and the same names and number of columns
        as in `cols`. The column stypes are `float32` for
        `float32` columns, and `float64` for all the other numeric types.

    except: TypeError
        The exception is raised when one of the columns from `cols`
        has a non-numeric type.


    See Also
    --------

    - :func:`median()` -- function to calculate median values.
    - :func:`sd()` -- function to calculate standard deviation.


    Examples
    --------
    .. code-block:: python

        >>> from datatable import dt, f, by
        >>>
        >>> df = dt.Frame({'A': [1, 1, 2, 1, 2],
        ...                'B': [None, 2, 3, 4, 5],
        ...                'C': [1, 2, 1, 1, 2]})
        >>>
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


    Get the mean from column A::

        >>> df[:, dt.mean(f.A)]
           |       A
           | float64
        -- + -------
         0 |     1.4
        [1 row x 1 column]

    Get the mean of multiple columns::

        >>> df[:, dt.mean([f.A, f.B])]
           |       A        B
           | float64  float64
        -- + -------  -------
         0 |     1.4      3.5
        [1 row x 2 columns]

    Same as above, but applying to a column slice::

        >>> df[:, dt.mean(f[:2])]
           |       A        B
           | float64  float64
        -- + -------  -------
         0 |     1.4      3.5
        [1 row x 2 columns]

    You can pass in a dictionary with new column names::

        >>> df[:, dt.mean({"A_mean": f.A, "C_avg": f.C})]
           |  A_mean    C_avg
           | float64  float64
        -- + -------  -------
         0 |     1.4      1.4
        [1 row x 2 columns]

    In the presence of :func:`by()`, it returns the average of each column per group::

        >>> df[:, dt.mean({"A_mean": f.A, "B_mean": f.B}), by("C")]
           |     C   A_mean   B_mean
           | int32  float64  float64
        -- + -----  -------  -------
         0 |     1  1.33333      3.5
         1 |     2  1.5          3.5
        [2 rows x 3 columns]
