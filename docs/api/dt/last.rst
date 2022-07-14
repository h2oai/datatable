
.. xfunction:: datatable.last
    :src: src/core/expr/head_reduce_unary.cc compute_firstlast
    :cvar: doc_dt_last
    :tests: tests/test-reduce.py
    :signature: last(cols)

    Return the last row for an ``Expr``. If `cols` is an iterable,
    simply return the last element.


    Parameters
    ----------
    cols: Expr | iterable
        Input columns or an iterable.

    return: Expr | ...
        One-row f-expression that has the same names, stypes and
        number of columns as `cols`. For an iterable the last
        element is returned.


    Examples
    --------
    Function :func:`last()` called on a frame, that is an iterable of columns,
    returns the last column::

        >>> from datatable import dt, last, f, by
        >>> DT = dt.Frame({"A": [1, 1, 2, 1, 2],
        ...                "B": [None, 5, 3, 4, None]})
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1     NA
         1 |     1      5
         2 |     2      3
         3 |     1      4
         4 |     2     NA

        [5 rows x 2 columns]
        >>> last(DT)
           |     B
           | int32
        -- + -----
         0 |    NA
         1 |     5
         2 |     3
         3 |     4
         4 |    NA

        [5 rows x 1 column]

    Called on a set of columns, :func:`last()` returns the last row::

        >>> DT[:, last(f[:])]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     2     NA

        [1 row x 2 columns]

    The same could also be achieved by passing ``-1`` to the row selector::

        >>> DT[-1, :]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     2     NA

        [1 row x 2 columns]

    To get the last non-missing value in a column, one should additionally employ
    a corresponding i-filter::

        >>> DT[f.B != None, last(f.B)]
           |     B
           | int32
        -- + -----
         0 |     4

        [1 row x 1 column]

    Function :func:`last()` is group-aware, meaning that it returns the last row
    per group in a :func:`by()` context::

        >>> DT[:, last(f.B), by("A")]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1      4
         1 |     2     NA

        [2 rows x 2 columns]

    To get the last non-missing value per group,
    one should first filter out all the missing values from the column in question::

        >>> DT[f.B != None, :][:, last(f.B), by("A")]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1      4
         1 |     2      3

        [2 rows x 2 columns]

    .. note::

        Filtering out missing values in the row selector will not work in
        a general case, e.g. when one needs to find the last non-missing values
        in several columns.


    See Also
    --------
    - :func:`first()` -- function that returns the first row.
