
.. xfunction:: datatable.last
    :src: src/core/expr/head_reduce_unary.cc compute_firstlast
    :cvar: doc_dt_last
    :tests: tests/test-reduce.py
    :signature: last(cols)

    Return the last row for each column from `cols`.


    Parameters
    ----------
    cols: Expr
        Input columns.

    return: Expr
        f-expression having one row, and the same names, stypes and
        number of columns as in `cols`.


    Examples
    --------

    ``last()`` returns the last column in a frame::

        >>> from datatable import dt, f, by, sort, last
        >>>
        >>> df = dt.Frame({"A": [1, 1, 2, 1, 2],
        ...                "B": [None, 2, 3, 4, None]})
        >>>
        >>> df
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1     NA
         1 |     1      2
         2 |     2      3
         3 |     1      4
         4 |     2     NA
        [5 rows x 2 columns]

        >>> dt.last(df)
           |     B
           | int32
        -- + -----
         0 |    NA
         1 |     2
         2 |     3
         3 |     4
         4 |    NA
        [5 rows x 1 column]

    Within a frame, it returns the last row::

        >>> df[:, last(f[:])]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     2     NA
        [1 row x 2 columns]

    The above code can be replicated by passing -1 to the ``i`` section instead::

        >>> df[-1, :]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     2     NA
        [1 row x 2 columns]

    Like ``first()``, ``last()`` can be handy if you wish to get the last
    non null value in a column::

        >>> df[f.B != None, dt.last(f.B)]
           |     B
           | int32
        -- + -----
         0 |     4
        [1 row x 1 column]

    ``last()`` returns the last row per group in a :func:`by()` operation::

        >>> df[:, last(f[:]), by("A")]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1      4
         1 |     2     NA
        [2 rows x 2 columns]

    To get the last non-null value per row in a :func:`by()` operation, you can
    use the :func:`sort()` function, and set the ``na_position`` argument as
    ``first`` (this will move the ``NAs`` to the top of the column)::

        >>> df[:, last(f[:]), by("A"), sort("B", na_position="first")]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1      4
         1 |     2      3
        [2 rows x 2 columns]


    See Also
    --------
    - :func:`first()` -- function that returns the first row.
