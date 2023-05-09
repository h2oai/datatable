
.. xfunction:: datatable.min
    :src: src/core/expr/fexpr_minmax.cc pyfn_min
    :tests: tests/test-reduce.py
    :cvar: doc_dt_min
    :signature: min(cols)

    Calculate the minimum value for each column from `cols`. It is recommended
    to use it as `dt.min()` to prevent conflict with the Python built-in
    `min()` function.


    Parameters
    ----------
    cols: FExpr
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
        >>>
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

    Get the minimum from column B::

        >>> df[:, dt.min(f.B)]
           |     B
           | int32
        -- + -----
         0 |     1
        [1 row x 1 column]

    Get the minimum of all columns::

        >>> df[:, [dt.min(f.A), dt.min(f.B)]]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1      1
        [1 row x 2 columns]

    Same as above, but using the slice notation::

        >>> df[:, dt.min(f[:])]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1      1
        [1 row x 2 columns]

    You can pass in a dictionary with new column names::

        >>> df[:, dt.min({"A_min": f.A, "B_min": f.B})]
           |   A_min    B_min
           |   int32    int32
        -- + -------  -------
         0 |       1        1
        [1 row x 2 columns]

    In the presence of :func:`by()`, it returns the row with the minimum value
    per group::

        >>> df[:, dt.min(f.B), by("A")]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1      2
         1 |     2      1
         2 |     3      1
        [3 rows x 2 columns]
