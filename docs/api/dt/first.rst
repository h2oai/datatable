
.. xfunction:: datatable.first
    :src: src/core/expr/head_reduce_unary.cc compute_firstlast
    :cvar: doc_dt_first
    :tests: tests/test-reduce.py
    :signature: first(cols)

    Return the first row for an ``Expr``. If `cols` is an iterable,
    simply return the first element.


    Parameters
    ----------
    cols: Expr | iterable
        Input columns or an iterable.

    return: Expr | ...
        One-row f-expression that has the same names, stypes and
        number of columns as `cols`. For an iterable the first
        element is returned.


    Examples
    --------
    Function :func:`first()` called on a frame, that is an iterable of columns,
    returns the first column::

        >>> from datatable import dt, first, f, by
        >>> DT = dt.Frame({"A": [1, 1, 2, 1, 2],
        ...                "B": [None, 5, 3, 4, 2]})
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1     NA
         1 |     1      5
         2 |     2      3
         3 |     1      4
         4 |     2      2

        [5 rows x 2 columns]
        >>> first(DT)
           |     A
           | int32
        -- + -----
         0 |     1
         1 |     1
         2 |     2
         3 |     1
         4 |     2

        [5 rows x 1 column]

    Called on a set of columns, :func:`first()` returns the first row::

        >>> DT[:, first(f[:])]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1     NA

        [1 row x 2 columns]

    The same could also be achieved by passing ``0`` to the row selector::

        >>> DT[0, :]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1     NA

        [1 row x 2 columns]

    To get the first non-missing value in a column, one should additionally employ
    a corresponding i-filter::

        >>> DT[f.B != None, first(f.B)]
           |     B
           | int32
        -- + -----
         0 |     5

        [1 row x 1 column]

    Function :func:`first()` is group-aware, meaning that it returns the first row
    per group in a :func:`by()` context::

        >>> DT[:, first(f.B), by("A")]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1     NA
         1 |     2      3

        [2 rows x 2 columns]

    To get the first non-missing value per group,
    one should first filter out all the missing values from the column in question::

        >>> DT[f.B != None, :][:, first(f.B), by("A")]
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     1      5
         1 |     2      3

        [2 rows x 2 columns]

    .. note::

        Filtering missing values in the row selector will not work in
        a general case, e.g. when one needs to find the first non-missing values
        in several columns.


    See Also
    --------
    - :func:`last()` -- function that returns the last row.
