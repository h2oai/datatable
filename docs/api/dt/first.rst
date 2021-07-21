
.. xfunction:: datatable.first
    :src: src/core/expr/head_reduce_unary.cc compute_firstlast
    :cvar: doc_dt_first
    :tests: tests/test-reduce.py
    :signature: first(cols)

    Return the first row for each column from `cols`.


    Parameters
    ----------
    cols: Expr
        Input columns.

    return: Expr
        f-expression having one row, and the same names, stypes and
        number of columns as in `cols`.


    Examples
    --------
    :func:`first()` returns the first column in a frame::

        >>> from datatable import dt, f, by, sort, first
        >>> df = dt.Frame({"A": [1, 1, 2, 1, 2],
        ...                "B": [None, 2, 3, 4, 5]})
        >>> df
           |  A   B
        -- + --  --
         0 |  1  NA
         1 |  1   2
         2 |  2   3
         3 |  1   4
         4 |  2   5

        [5 rows x 2 columns]
        >>> dt.first(df)
           |  A
        -- + --
         0 |  1
         1 |  1
         2 |  2
         3 |  1
         4 |  2

        [5 rows x 1 column]

    Within a frame, it returns the first row::

        >>> df[:, first(f[:])]
           |  A   B
        -- + --  --
         0 |  1  NA

        [1 row x 2 columns]

    Of course, you can replicate this by passing 0 to the ``i`` section instead::

        >>> df[0, :]
           |  A   B
        -- + --  --
         0 |  1  NA

        [1 row x 2 columns]

    :func:`first()` comes in handy if you wish to get the first non null value in a
    column::

        >>> df[f.B != None, first(f.B)]
           |  B
        -- + --
         0 |  2

        [1 row x 1 column]

    :func:`first()` returns the first row per group in a :func:`by()` operation::

        >>> df[:, first(f[:]), by("A")]
           |  A   B
        -- + --  --
         0 |  1  NA
         1 |  2   3

        [2 rows x 2 columns]

    To get the first non-null value per row in a :func:`by()` operation, you can
    use the :func:`sort()` function, and set the ``na_position`` argument as
    ``last``::

        >>> df[:, first(f[:]), by("A"), sort("B", na_position="last")]
           |  A   B
        -- + --  --
         0 |  1   2
         1 |  2   3

        [2 rows x 2 columns]


    See Also
    --------
    - :func:`last()` -- function that returns the last row.
