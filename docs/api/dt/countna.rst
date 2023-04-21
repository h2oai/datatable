
.. xfunction:: datatable.countna
    :src: src/core/expr/fexpr_count.cc pyfn_countna
    :tests: tests/test-reduce.py
    :cvar: doc_dt_countna
    :signature: countna(cols=None)

    .. x-version-added:: 1.1.0

    Count missing values for each column from `cols`. This function
    is group-aware.


    Parameters
    ----------
    cols: FExpr
        Input columns.

    return: FExpr
        f-expression that counts the number of missing values
        for each column from `cols`. If `cols` is not provided,
        it will return `0` for each of the frame's group.
        The returned f-expression has as many rows as there are groups,
        it also has the same names and number of columns as in `cols`.
        All the resulting column's stypes are `int64`.

    except: TypeError
        The exception is raised when one of the columns from `cols`
        has an obj64 type.


    Examples
    --------

    .. code-block:: python

        >>> from datatable import dt, f
        >>>
        >>> DT = dt.Frame({'A': [None, 1, 2, None, 2],
        ...                'B': [None, 2, 3, 4, 5],
        ...                'C': [1, 2, 1, 1, 2]})
        >>> DT
           |     A      B      C
           | int32  int32  int32
        -- + -----  -----  -----
         0 |    NA     NA      1
         1 |     1      2      2
         2 |     2      3      1
         3 |    NA      4      1
         4 |     2      5      2
        [5 rows x 3 columns]

    Count missing values in all the columns::

        >>> DT[:, dt.countna(f[:])]
           |     A      B      C
           | int64  int64  int64
        -- + -----  -----  -----
         0 |     2      1      0
        [1 row x 3 columns]

    Count missing values in the column `B` only::

        >>> DT[:, dt.countna(f.B)]
           |     B
           | int64
        -- + -----
         0 |     1
        [1 row x 1 column]

    When no `cols` is passed, this function will always return zero::

        >>> DT[:, dt.countna()]
           |    C0
           | int64
        -- + -----
         0 |     0
        [1 row x 1 column]




    See Also
    --------

    - :func:`count()` -- function to count the number of non-missing values.
