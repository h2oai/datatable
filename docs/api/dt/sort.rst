
.. xfunction:: datatable.sort
    :src: src/core/expr/py_sort.cc osort::osort_pyobject::m__init__
    :cvar: doc_dt_sort
    :signature: sort(*cols, reverse=False, na_position="first")

    Sort clause for use in Frame's square-bracket selector.

    When a ``sort()`` object is present inside a ``DT[i, j, ...]``
    expression, it will sort the rows of the resulting Frame according
    to the columns ``cols`` passed as the arguments to ``sort()``.

    When used together with ``by()``, the sort clause applies after the
    group-by, i.e. we sort elements within each group. Note, however,
    that because we use stable sorting, the operations of grouping and
    sorting are commutative: the result of applying groupby and then sort
    is the same as the result of sorting first and then doing groupby.

    When used together with ``i`` (row filter), the ``i`` filter is
    applied after the sorting. For example::

        >>> DT[:10, :, sort(f.Highscore, reverse=True)]

    will select the first 10 records from the frame ``DT`` ordered by
    the Highscore column.


    Parameters
    ----------
    cols: FExpr
        Columns to sort the frame by.

    reverse: bool
        If ``False``, sorting is performed in the ascending order. If ``True``,
        sorting is descending.

    na_position: None | "first" | "last" | "remove"
        If "first", the default behaviour, missing values are placed
        first in sorted output. If "last", they are placed last.
        If "remove", rows that contain missing values in `cols`
        are excluded from the output.

    return: object
        ``datatable.sort`` object for use in square-bracket selector.


    Examples
    --------
    .. code-block:: python

        >>> from datatable import dt, f, by
        >>>
        >>> DT = dt.Frame({"col1": ["A", "A", "B", None, "D", "C"],
        >>>                "col2": [2, 1, 9, 8, 7, 4],
        >>>                "col3": [0, 1, 9, 4, 2, 3],
        >>>                "col4": [1, 2, 3, 3, 2, 1]})
        >>>
        >>> DT
           | col1    col2   col3   col4
           | str32  int32  int32  int32
        -- + -----  -----  -----  -----
         0 | A          2      0      1
         1 | A          1      1      2
         2 | B          9      9      3
         3 | NA         8      4      3
         4 | D          7      2      2
         5 | C          4      3      1
        [6 rows x 4 columns]

    Sort by a single column::

        >>> DT[:, :, dt.sort("col1")]
           | col1    col2   col3   col4
           | str32  int32  int32  int32
        -- + -----  -----  -----  -----
         0 | NA         8      4      3
         1 | A          2      0      1
         2 | A          1      1      2
         3 | B          9      9      3
         4 | C          4      3      1
         5 | D          7      2      2
        [6 rows x 4 columns]


    Sort by multiple columns::

        >>> DT[:, :, dt.sort("col2", "col3")]
           | col1    col2   col3   col4
           | str32  int32  int32  int32
        -- + -----  -----  -----  -----
         0 | A          1      1      2
         1 | A          2      0      1
         2 | C          4      3      1
         3 | D          7      2      2
         4 | NA         8      4      3
         5 | B          9      9      3
        [6 rows x 4 columns]

    Sort in descending order::

        >>> DT[:, :, dt.sort(-f.col1)]
           | col1    col2   col3   col4
           | str32  int32  int32  int32
        -- + -----  -----  -----  -----
         0 | NA         8      4      3
         1 | D          7      2      2
         2 | C          4      3      1
         3 | B          9      9      3
         4 | A          2      0      1
         5 | A          1      1      2
        [6 rows x 4 columns]

    The frame can also be sorted in descending order by setting the ``reverse`` parameter to ``True``::

        >>> DT[:, :, dt.sort("col1", reverse=True)]
           | col1    col2   col3   col4
           | str32  int32  int32  int32
        -- + -----  -----  -----  -----
         0 | NA         8      4      3
         1 | D          7      2      2
         2 | C          4      3      1
         3 | B          9      9      3
         4 | A          2      0      1
         5 | A          1      1      2
        [6 rows x 4 columns]

    By default, when sorting, null values are placed at the top; to relocate null values to the bottom, pass ``last`` to the ``na_position`` parameter::

        >>> DT[:, :, dt.sort("col1", na_position="last")]
           | col1    col2   col3   col4
           | str32  int32  int32  int32
        -- + -----  -----  -----  -----
         0 | A          2      0      1
         1 | A          1      1      2
         2 | B          9      9      3
         3 | C          4      3      1
         4 | D          7      2      2
         5 | NA         8      4      3
        [6 rows x 4 columns]

    Passing ``remove`` to ``na_position`` completely excludes any row with null values from the sorted output:

        >>> DT[:, :, dt.sort("col1", na_position="remove")]
           | col1    col2   col3   col4
           | str32  int32  int32  int32
        -- + -----  -----  -----  -----
         0 | A          2      0      1
         1 | A          1      1      2
         2 | B          9      9      3
         3 | C          4      3      1
         4 | D          7      2      2
        [5 rows x 4 columns]

    Sort by multiple columns, descending and ascending order::

        >>> DT[:, :, dt.sort(-f.col2, f.col3)]
           | col1    col2   col3   col4
           | str32  int32  int32  int32
        -- + -----  -----  -----  -----
         0 | B          9      9      3
         1 | NA         8      4      3
         2 | D          7      2      2
         3 | C          4      3      1
         4 | A          2      0      1
         5 | A          1      1      2
        [6 rows x 4 columns]

    The same code above can be replicated by passing a list of booleans to ``reverse``. 
    The length of the reverse flag list should match the number of columns to be sorted::

        >>> DT[:, :, dt.sort("col2", "col3", reverse=[True, False])]
           | col1    col2   col3   col4
           | str32  int32  int32  int32
        -- + -----  -----  -----  -----
         0 | B          9      9      3
         1 | NA         8      4      3
         2 | D          7      2      2
         3 | C          4      3      1
         4 | A          2      0      1
         5 | A          1      1      2
        [6 rows x 4 columns]

    In the presence of :func:`by()`, :func:`sort()` sorts within each group::

        >>> DT[:, :, by("col4"), dt.sort(f.col2)]
           |  col4  col1    col2   col3
           | int32  str32  int32  int32
        -- + -----  -----  -----  -----
         0 |     1  A          2      0
         1 |     1  C          4      3
         2 |     2  A          1      1
         3 |     2  D          7      2
         4 |     3  NA         8      4
         5 |     3  B          9      9
        [6 rows x 4 columns]

