
.. xfunction:: datatable.max
    :src: src/core/expr/head_reduce_unary.cc max_reducer
    :tests: tests/test-reduce.py
    :cvar: doc_dt_max
    :signature: max(cols)

    Calculate the maximum value for each column from `cols`. It is recommended
    to use it as `dt.max()` to prevent conflict with the Python built-in
    `max()` function.

    Parameters
    ----------
    cols: Expr
        Input columns.

    return: Expr
        f-expression having one row and the same names, stypes and number
        of columns as in `cols`.

    except: TypeError
        The exception is raised when one of the columns from `cols`
        has a non-numeric type.


    Examples
    --------
    .. code-block:: python

        >>> from datatable import dt, f, by
        >>>
        >>> df = dt.Frame({'A': [1, 1, 1, 2, 2, 2, 3, 3, 3],
        ...                'B': [3, 2, 20, 1, 6, 2, 3, 22, 1]})
        >>> df
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1      3
         1 |     1      2
         2 |     1     20
         3 |     2      1
         4 |     2      6
         5 |     2      2
         6 |     3      3
         7 |     3     22
         8 |     3      1
        [9 rows x 2 columns]

    Get the maximum from column B::

        >>> df[:, dt.max(f.B)]
           |     B
           | int32
        -- + -----
         0 |    22
        [1 row x 1 column]

    Get the maximum of all columns::

        >>> df[:, [dt.max(f.A), dt.max(f.B)]]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     3     22
        [1 row x 2 columns]

    Same as above, but more convenient::

        >>> df[:, dt.max(f[:])]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     3     22
        [1 row x 2 columns]

    You can pass in a dictionary with new column names::

        >>> df[:, dt.max({"A_max": f.A, "B_max": f.B})]
           |   A_max    B_max
           |   int32    int32
        -- + -------  -------
         0 |       3       22
        [1 row x 2 columns]

    In the presence of :func:`by()`, it returns the row with the maximum
    value per group::

        >>> df[:, dt.max(f.B), by("A")]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1     20
         1 |     2      6
         2 |     3     22
        [3 rows x 2 columns]
