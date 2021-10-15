
.. xfunction:: datatable.update
    :src: src/core/expr/py_update.cc oupdate::oupdate_pyobject::m__init__
    :cvar: doc_dt_update
    :signature: update(**kwargs)

    Create new or update existing columns within a frame.

    This expression is intended to be used at "j" place in ``DT[i, j]``
    call. It takes an arbitrary number of key/value pairs each describing
    a column name and the expression for how that column has to be
    created/updated.

    Examples
    --------
    .. code-block:: python

        >>> from datatable import dt, f, by, update
        >>>
        >>> DT = dt.Frame([range(5), [4, 3, 9, 11, -1]], names=("A", "B"))
        >>> DT
           |     A      B
           | int32  int32
        -- + -----  -----
         0 |     0      4
         1 |     1      3
         2 |     2      9
         3 |     3     11
         4 |     4     -1
        [5 rows x 2 columns]

    Create new columns and update existing columns::

        >>> DT[:, update(C = f.A * 2,
        ...              D = f.B // 3,
        ...              A = f.A * 4,
        ...              B = f.B + 1)]
        >>> DT
           |     A      B      C      D
           | int32  int32  int32  int32
        -- + -----  -----  -----  -----
         0 |     0      5      0      1
         1 |     4      4      2      1
         2 |     8     10      4      3
         3 |    12     12      6      3
         4 |    16      0      8     -1
        [5 rows x 4 columns]

    Add new column with `unpacking`_; this can be handy for dynamicallly adding
    columns with dictionary comprehensions, or if the names are not valid python
    keywords::

        >>> DT[:, update(**{"extra column": f.A + f.B + f.C + f.D})]
        >>> DT
           |     A      B      C      D  extra column
           | int32  int32  int32  int32         int32
        -- + -----  -----  -----  -----  ------------
         0 |     0      5      0      1             6
         1 |     4      4      2      1            11
         2 |     8     10      4      3            25
         3 |    12     12      6      3            33
         4 |    16      0      8     -1            23
        [5 rows x 5 columns]

    You can update a subset of data::

        >>> DT[f.A > 10, update(A = f.A * 5)]
        >>> DT
           |     A      B      C      D  extra column
           | int32  int32  int32  int32         int32
        -- + -----  -----  -----  -----  ------------
         0 |     0      5      0      1             6
         1 |     4      4      2      1            11
         2 |     8     10      4      3            25
         3 |    60     12      6      3            33
         4 |    80      0      8     -1            23
        [5 rows x 5 columns]

    You can also add a new column or update an existing column in a groupby
    operation, similar to SQL's `window` operation, or pandas `transform()`::

        >>> df = dt.Frame("""exporter assets   liabilities
        ...                   False      5          1
        ...                   True       10         8
        ...                   False      3          1
        ...                   False      24         20
        ...                   False      40         2
        ...                   True       12         11""")
        >>>
        >>> # Get the ratio for each row per group
        >>> df[:,
        ...    update(ratio = dt.sum(f.liabilities) * 100 / dt.sum(f.assets)),
        ...    by(f.exporter)]
        >>> df
           | exporter  assets  liabilities    ratio
           |    bool8   int32        int32  float64
        -- + --------  ------  -----------  -------
         0 |        0       5            1  33.3333
         1 |        1      10            8  86.3636
         2 |        0       3            1  33.3333
         3 |        0      24           20  33.3333
         4 |        0      40            2  33.3333
         5 |        1      12           11  86.3636
        [6 rows x 4 columns]


    .. _`unpacking` : https://docs.python.org/3/tutorial/controlflow.html#unpacking-argument-lists
