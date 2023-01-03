
.. xfunction:: datatable.nth
    :src: src/core/expr/fexpr_nth.cc pyfn_nth
    :cvar: doc_dt_nth
    :tests: tests/test-reduce.py
    :signature: nth(cols, n)

    Return the ``nth`` row for an ``Expr``.

    Parameters
    ----------
    cols: FExpr | iterable
        Input columns or an iterable.

    return: Expr | ...
        One-row f-expression that has the same names, stypes and
        number of columns as `cols`. 

    Examples
    --------
    .. code-block:: python

        >>> from datatable import dt, f, by
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

    Get the third row of column A::

        >>> df[:, dt.nth(f.A, n=2)]
           |     A
           | int32
        -- + -----
         0 |     2
        [1 row x 1 column]

    Get the third row for multiple columns::

        >>> df[:, dt.nth(f[:], n=2)]
           |     A      B      C
           | int32  int32  int32
        -- + -----  -----  -----
         0 |     2      3      1

    Replicate :func:`first()`::

        >>> df[:, dt.nth(f.A, n=0)]
           |     A
           | int32
        -- + -----
         0 |     1
        [1 row x 1 column]

    Replicate :func:`last()`::

        >>> df[:, dt.nth(f.A, n=-1)]
           |     A
           | int32
        -- + -----
         0 |     2
        [1 row x 1 column]

In the presence of :func:`by()`, it returns the nth row of the specified columns per group::

        >>> df[:, [dt.nth(f.A, n = 2), dt.nth(f.B, n = -1)], by(f.C)]
           |     C      A      B
           | int32  int32  int32
        -- + -----  -----  -----
         0 |     1      1      4
         1 |     2     NA      5
        [2 rows x 3 columns]



    See Also
    --------
    - :func:`first()` -- function that returns the first row.
    - :func:`last()` -- function that returns the last row.
