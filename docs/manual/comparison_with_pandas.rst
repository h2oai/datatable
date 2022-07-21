
Comparison with pandas
======================
A lot of potential :mod:`datatable` users are likely to have some familiarity
with `pandas`_; as such, this page provides some examples of how various
pandas operations can be performed within datatable. The datatable module
emphasizes speed and big data support (an area that pandas struggles with);
it also has an expressive and concise syntax, which makes datatable also
useful for small datasets.

Note: in pandas, there are two fundamental data structures: `Series`_ and
`DataFrame`_. In datatable, there is only one fundamental data structure —
the :class:`Frame`. Most of the comparisons will be between pandas ``DataFrame``
and datatable ``Frame``.

.. code-block:: python

    >>> import pandas as pd
    >>> import numpy as np
    >>> from datatable import dt, f, by, g, join, sort, update, ifelse
    >>>
    >>> data = {"A": [1, 2, 3, 4, 5],
    ...         "B": [4, 5, 6, 7, 8],
    ...         "C": [7, 8, 9, 10, 11],
    ...         "D": [5, 7, 2, 9, -1]}
    >>>
    >>> # datatable
    >>> DT = dt.Frame(data)
    >>>
    >>> # pandas
    >>> df = pd.DataFrame(data)


Row and Column Selection
------------------------

.. x-comparison-table::
    :header1: pandas
    :header2: datatable

    Select a single row
    ----
    .. code-block::

        df.loc[2]

    ----
    .. code-block::

        DT[2, :]


    ====
    Select several rows by their indices
    ----
    .. code-block::

        df.iloc[[2, 3, 4]]

    ----
    .. code-block::

        DT[[2, 3, 4], :]


    ====
    Select a slice of rows by position
    ----
    .. code-block::

        df.iloc[2:5]
        # or
        df.iloc[range(2, 5)]

    ----
    .. code-block::

        DT[2:5, :]
        # or
        DT[range(2, 5), :]


    ====
    Select every second row
    ----
    .. code-block::

        df.iloc[::2]

    ----
    .. code-block::

        DT[::2, :]


    ====
    Select rows using a boolean mask
    ----
    .. code-block::

        df.iloc[[True, True, False, False, True]]

    ----
    .. code-block::

        DT[[True, True, False, False, True], :]


    ====
    Select rows on a condition
    ----
    .. code-block::

        df.loc[df['A']>3]

    ----
    .. code-block::

        DT[f.A>3, :]


    ====
    Select rows on multiple conditions, using OR
    ----
    .. code-block::

        df.loc[(df['A'] > 3) | (df['B']<5)]

    ----
    .. code-block::

        DT[(f.A>3) | (f.B<5), :]


    ====
    Select rows on multiple conditions, using AND
    ----
    .. code-block::

        df.loc[(df['A'] > 3) & (df['B']<8)]

    ----
    .. code-block::

        DT[(f.A>3) & (f.B<8), :]


    ====
    Select a single column by column name
    ----
    .. code-block::

        df['A']
        df.loc[:, 'A']

    ----
    .. code-block::

        DT['A']
        DT[:, 'A']


    ====
    Select a single column by position
    ----
    .. code-block::

        df.iloc[:, 1]

    ----
    .. code-block::

        DT[1]
        DT[:, 1]


    ====
    Select multiple columns by column names
    ----
    .. code-block::

        df.loc[:, ["A", "B"]]

    ----
    .. code-block::

        DT[:, ["A", "B"]]


    ====
    Select multiple columns by position
    ----
    .. code-block::

        df.iloc[:, [0, 1]]

    ----
    .. code-block::

        DT[:, [0, 1]]


    ====
    Select multiple columns by slicing
    ----
    .. code-block::

        df.loc[:, "A":"B"]

    ----
    .. code-block::

        DT[:, "A":"B"]


    ====
    Select multiple columns by position
    ----
    .. code-block::

        df.iloc[:, 1:3]

    ----
    .. code-block::

        DT[:, 1:3]


    ====
    Select columns by Boolean mask
    ----
    .. code-block::

        df.loc[:, [True,False,False,True]]

    ----
    .. code-block::

        DT[:, [True,False,False,True]]


    ====
    Select multiple rows and columns
    ----
    .. code-block::

        df.loc[2:5, "A":"B"]

    ----
    .. code-block::

        DT[2:5, "A":"B"]


    ====
    Select multiple rows and columns by position
    ----
    .. code-block::

        df.iloc[2:5, :2]

    ----
    .. code-block::

        DT[2:5, :2]


    ====
    Select a single value (returns a scalar)
    ----
    .. code-block::

        df.at[2, 'A']
        df.loc[2, 'A']

    ----
    .. code-block::

        DT[2, "A"]


    ====
    Select a single value by position
    ----
    .. code-block::

        df.iat[2, 0]
        df.iloc[2, 0]

    ----
    .. code-block::

        DT[2, 0]


    ====
    Select a single value, return as Series
    ----
    .. code-block::

        df.loc[2, ["A"]]

    ----
    .. code-block::

        DT[2, ["A"]]


    ====
    Select a single value (return as Series/Frame) by position
    ----
    .. code-block::

        df.iloc[2, [0]]

    ----
    .. code-block::

        DT[2, [0]]


    ====
    In pandas every frame has a row index, and if a filtration is executed,
    the row numbers are kept::

        >>> df.loc[df['A'] > 3]
            A   B   C    D
        3   4   7   10   9
        4   5   8   11  -1

    ----
    Datatable has no notion of a row index; the row numbers displayed are just
    for convenience::

        >>> DT[f.A > 3, :]
           |     A      B      C      D
           | int32  int32  int32  int32
        -- + -----  -----  -----  -----
         0 |     4      7     10      9
         1 |     5      8     11     -1
        [2 rows x 4 columns]


    ====
    In pandas, the index can be numbers, or characters, or intervals, or even
    ``MultiIndex``es; you can subset rows on these labels::

        >>> df1 = df.set_index(pd.Index(['a','b','c','d','e']))
            A   B   C    D
        a   1   4   7    5
        b   2   5   8    7
        c   3   6   9    2
        d   4   7   10   9
        e   5   8   11  -1

        >>> df1.loc["a":"c"]
            A   B   C   D
        a   1   4   7   5
        b   2   5   8   7
        c   3   6   9   2

    ----
    Datatable has the :attr:`key <dt.Frame.key>` property, which is meant as
    an equivalent of pandas indices, but its purpose at the moment is for joins,
    not for subsetting data::

        >>> data = {"A": [1, 2, 3, 4, 5],
        ...         "B": [4, 5, 6, 7, 8],
        ...         "C": [7, 8, 9, 10, 11],
        ...         "D": [5, 7, 2, 9, -1],
        ...         "E": ['a','b','c','d','e']}
        >>> DT1 = dt.Frame(data)
        >>> DT1.key = 'E'
        >>> DT1
        E     |     A      B      C      D
        str32 | int32  int32  int32  int32
        ----- + -----  -----  -----  -----
        a     |     1      4      7      5
        b     |     2      5      8      7
        c     |     3      6      9      2
        d     |     4      7     10      9
        e     |     5      8     11     -1
        [5 rows x 5 columns]

        >>> DT1["a":"c", :]  # this will fail
        TypeError: A string slice cannot be used as a row selector


    ====
    Pandas' ``.loc`` notation works on labels, while ``.iloc`` works on actual
    positions. This is noticeable during row selection. Datatable, however, works
    only on positions::

        >>> df1 = df.set_index('C')
            A   B   D
        C
        7   1   4   5
        8   2   5   7
        9   3   6   2
        10  4   7   9
        11  5   8  -1

    Selecting with ``.loc`` for the row with number 7 returns no error::

        >>> df1.loc[7]
        A    1
        B    4
        D    5
        Name: 7, dtype: int64

    However, selecting with ``iloc`` for the row with number 7 returns an error,
    because positionally, there is no row 7::

        >>> df.iloc[7]
        IndexError: single positional indexer is out-of-bounds

    ----
    Datatable has the :attr:`dt.Frame.key` property, which is
    used for joins, not row subsetting, and as such selection similar to ``loc``
    with the row label is not possible::

        >>> DT.key = 'C'
        >>> DT
            C |     A      B      D
        int32 | int32  int32  int32
        ----- + -----  -----  -----
            7 |     1      4      5
            8 |     2      5      7
            9 |     3      6      2
           10 |     4      7      9
           11 |     5      8     -1
        [5 rows x 4 columns]

        >>> DT[7, :]   # this will fail
        ValueError: Row 7 is invalid for a frame with 5 rows



Add new/update existing columns
-------------------------------

.. x-comparison-table::
    :header1: pandas
    :header2: datatable

    Add a new column with a scalar value
    ----
    .. code-block::

        df['new_col'] = 2
        df = df.assign(new_col = 2)

    ----
    .. code-block::

        DT['new_col'] = 2
        DT[:, update(new_col=2)]

    ====
    Add a new column with a list of values
    ----
    .. code-block::

        df['new_col'] = range(len(df))
        df = df.assign(new_col = range(len(df))

    ----
    .. code-block::

        DT['new_col_1'] = range(DT.nrows)
        DT[:, update(new_col=range(DT.nrows)]

    ====
    Update a single value
    ----
    .. code-block::

        df.at[2, 'new_col'] = 200

    ----
    .. code-block::

        DT[2, 'new_col'] = 200

    ====
    Update an entire column
    ----
    .. code-block::

        df.loc[:, "A"] = 5  # or
        df["A"] = 5
        df = df.assign(A = 5)

    ----
    .. code-block::

        DT["A"] = 5
        DT[:, update(A = 5)]

    ====
    Update multiple columns
    ----
    .. code-block::

        df.loc[:, "A":"C"] = np.arange(15).reshape(-1,3)

    ----
    .. code-block::

        DT[:, "A":"C"] = np.arange(15).reshape(-1,3)


.. note::

    In datatable, the :func:`update()` method is in-place; reassigment to
    the Frame ``DT`` is not required.



Rename columns
--------------

.. x-comparison-table::
    :header1: pandas
    :header2: datatable

    Rename a column
    ----
    .. code-block::

        df = df.rename(columns={"A": "col_A"})

    ----
    .. code-block::

        DT.names = {"A": "col_A"}

    ====
    Rename multiple columns
    ----
    .. code-block::

        df = df.rename(columns={"A": "col_A", "B": "col_B"})

    ----
    .. code-block::

        DT.names = {"A": "col_A", "B": "col_B"}


In datatable, you can select and rename columns at the same time, by passing
a dictionary of :ref:`f-expressions` into the ``j`` section::

    >>> # datatable
    >>> DT[:, {"A": f.A, "box": f.B, "C": f.C, "D": f.D * 2}]
       |     A    box      C      D
       | int32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 |     1      4      7     10
     1 |     2      5      8     14
     2 |     3      6      9      4
     3 |     4      7     10     18
     4 |     5      8     11     -2
    [5 rows x 4 columns]



Delete Columns
--------------

.. x-comparison-table::
    :header1: pandas
    :header2: datatable

    ====
    Delete a column
    ----
    .. code-block::

        del df['B']

    ----
    .. code-block::

        del DT['B']

    ====
    Same as above
    ----
    .. code-block::

        df = df.drop('B', axis=1)

    ----
    .. code-block::

        DT = DT[:, f[:].remove(f.B)]

    ====
    Remove multiple columns
    ----
    .. code-block::

        df = df.drop(['B', 'C'], axis=1)

    ----
    .. code-block::

        del DT[: , ['B', 'C']]   # or
        DT = DT[:, f[:].remove([f.B, f.C])]




Sorting
-------

.. x-comparison-table::
    :header1: pandas
    :header2: datatable

    Sort by a column -- default ascending
    ----
    .. code-block::

        df.sort_values('A')

    ----
    .. code-block::

        DT.sort('A')                       # or
        DT[:, : , sort('A')]

    ====
    Sort by a column -- descending
    ----
    .. code-block::

        df.sort_values('A',ascending=False)

    ----
    .. code-block::

        DT.sort(-f.A)                      # or
        DT[:, :, sort(-f.A)]               # or
        DT[:, :, sort('A', reverse=True)]

    ====
    Sort by multiple columns -- default ascending
    ----
    .. code-block::

        df.sort_values(['A', 'C'])

    ----
    .. code-block::

        DT.sort('A', 'C')                  # or
        DT[:, :, sort('A', 'C')]

    ====
    Sort by multiple columns -- both descending
    ----
    .. code-block::

        df.sort_values(['A','C'],ascending=[False,False])

    ----
    .. code-block::

        DT.sort(-f.A, -f.C)                # or
        DT[:, :, sort(-f.A, -f.C)]         # or
        DT[:, :, sort('A', 'C', reverse=[True, True])]

    ====
    Sort by multiple columns -- different sort directions
    ----
    .. code-block::

        df.sort_values(['A', 'C'], ascending=[True, False])

    ----
    .. code-block::

        DT.sort(f.A, -f.C)                 # or
        DT[:, :, sort(f.A, -f.C)]          # or
        DT[:, :, sort('A', 'C', reverse=[False, True])]


.. note::

    By default, pandas puts NAs last in the sorted data, while datatable
    puts them first.

.. note::

    In pandas, there is an option to sort with a Callable; this option is not
    supported in datatable.

.. note::

    In pandas, you can sort on the rows or columns; in datatable sorting is
    column-wise only.



Grouping and Aggregation
------------------------

.. code-block:: python

    >>> data = {"a": [1, 1, 2, 1, 2],
    ...         "b": [2, 20, 30, 2, 4],
    ...         "c": [3, 30, 50, 33, 50]}
    >>>
    >>> # pandas
    >>> df = pd.DataFrame(data)
    >>>
    >>> # datatable
    >>> DT = dt.Frame(data)
    >>> DT
       |     a      b      c
       | int32  int32  int32
    -- + -----  -----  -----
     0 |     1      2      3
     1 |     1     20     30
     2 |     2     30     50
     3 |     1      2     33
     4 |     2      4     50
    [5 rows x 3 columns]


.. x-comparison-table::
    :header1: pandas
    :header2: datatable

    ====
    Group by column ``a`` and sum the other columns
    ----
    .. code-block::

        df.groupby("a").agg("sum")

    ----
    .. code-block::

        DT[:, dt.sum(f[:]), by("a")]

    ====
    Group by ``a`` and ``b`` and sum ``c``
    ----
    .. code-block::

        df.groupby(["a", "b"]).agg("sum")

    ----
    .. code-block::

        DT[:, dt.sum(f.c), by("a", "b")]


    ====
    Get size per group
    ----
    .. code-block::

        df.groupby("a").size()

    ----
    .. code-block::

        DT[:, dt.count(), by("a")]


    ====
    Grouping with multiple aggregation functions
    ----
    .. code-block::

        df.groupby("a").agg({"b": "sum", "c": "mean"})

    ----
    .. code-block::

        DT[:, {"b": dt.sum(f.b), "c": dt.mean(f.c)}, by("a")]

    ====
    Get the first row per group
    ----
    .. code-block::

        df.groupby("a").first()

    ----
    .. code-block::

        DT[0, :, by("a")]

    ====
    Get the last row per group
    ----
    .. code-block::

        df.groupby('a').last()

    ----
    .. code-block::

        DT[-1, :, by("a")]

    ====
    Get the first two rows per group
    ----
    .. code-block::

        df.groupby("a").head(2)

    ----
    .. code-block::

        DT[:2, :, by("a")]

    ====
    Get the last two rows per group
    ----
    .. code-block::

        df.groupby("a").tail(2)

    ----
    .. code-block::

        DT[-2:, :, by("a")]


Transformations within groups in pandas is done using the `pd.transform`_
function::

    >>> # pandas
    >>> grouping = df.groupby("a")["b"].transform("min")
    >>> df.assign(min_b=grouping)
        a   b   c   min_b
    0   1   2   3   2
    1   1   20  30  2
    2   2   30  50  4
    3   1   2   33  2
    4   2   4   50  4

In datatable, transformations occur within the ``j`` section; in the presence
of :func:`by()`, the computations within ``j`` are per group::

    >>> # datatable
    >>> DT[:, f[:].extend({"min_b": dt.min(f.b)}), by("a")]
       |     a      b      c  min_b
       | int32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 |     1      2      3      2
     1 |     1     20     30      2
     2 |     1      2     33      2
     3 |     2     30     50      4
     4 |     2      4     50      4
    [5 rows x 4 columns]

Note that the result above is sorted by the grouping column. If you want the
data to maintain the same shape as the source data, then :func:`update()` is
a better option (and usually faster)::

    >>> # datatable
    >>> DT[:, update(min_b = dt.min(f.b)), by("a")]
    >>> DT
       |     a      b      c  min_b
       | int32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 |     1      2      3      2
     1 |     1     20     30      2
     2 |     2     30     50      4
     3 |     1      2     33      2
     4 |     2      4     50      4
    [5 rows x 4 columns]

In pandas, some computations might require creating the column first before
aggregation within a groupby. Take the example below, where we need to
calculate the revenue per group::

    >>> data = {'shop': ['A', 'B', 'A'],
    ...         'item_price': [123, 921, 28],
    ...         'item_sold': [1, 2, 4]}
    >>> df1 = pd.DataFrame(data)  # pandas
    >>> DT1 = dt.Frame(data)      # datatable
    >>> DT1
       | shop   item_price  item_sold
       | str32       int32      int32
    -- + -----  ----------  ---------
     0 | A             123          1
     1 | B             921          2
     2 | A              28          4
    [3 rows x 3 columns]

To get the total revenue, we first need to create a revenue column, then sum it
in the groupby::

    >>> # pandas
    >>> df1['revenue'] = df1['item_price'] * df1['item_sold']
    >>> df1.groupby("shop")['revenue'].sum().reset_index()
        shop    revenue
    0   A       235
    1   B       1842

In datatable, there is no need to create a temporary column; you can easily
nest your computations in the ``j`` section; the computations will be
executed per group::

    >>> # datatable
    >>> DT1[:, {"revenue": dt.sum(f.item_price * f.item_sold)}, by("shop")]
       | shop   revenue
       | str32    int64
    -- + -----  -------
     0 | A          235
     1 | B         1842
    [2 rows x 2 columns]

You can learn more about the :func:`by()` function at the
:ref:`Grouping with by` documentation.



Concatenate
-----------

In pandas you can combine multiple dataframes using the ``concatenate()``
method; the concatenation is based on the indices::

    >>> # pandas
    >>> df1 = pd.DataFrame({"A": ["a", "a", "a"], "B": range(3)})
    >>>
    >>> df2 = pd.DataFrame({"A": ["b", "b", "b"], "B": range(4, 7)})

By default, pandas concatenates the rows, with one dataframe on top of the other::

    >>> pd.concat([df1, df2], axis = 0)
        A   B
    0   a   0
    1   a   1
    2   a   2
    0   b   4
    1   b   5
    2   b   6

The same functionality can be replicated in datatable using the
:meth:`dt.Frame.rbind` method::

    >>> # datatable
    >>> DT1 = dt.Frame(df1)
    >>> DT2 = dt.Frame(df2)
    >>> dt.rbind(DT1, DT2)
       | A          B
       | str32  int64
    -- + -----  -----
     0 | a          0
     1 | a          1
     2 | a          2
     3 | b          4
     4 | b          5
     5 | b          6
    [6 rows x 2 columns]

Notice how in pandas the indices are preserved (you can get rid of the indices
with the ``ignore_index`` argument), whereas in datatable the indices are not
referenced.

To combine data across the columns, in pandas, you set the axis argument to
``columns``::

    >>> # pandas
    >>> df1 = pd.DataFrame({"A": ["a", "a", "a"], "B": range(3)})
    >>> df2 = pd.DataFrame({"C": ["b", "b", "b"], "D": range(4, 7)})
    >>> df3 = pd.DataFrame({"E": ["c", "c", "c"], "F": range(7, 10)})
    >>> pd.concat([df1, df2, df3], axis = 1)
        A   B   C   D   E   F
    0   a   0   b   4   c   7
    1   a   1   b   5   c   8
    2   a   2   b   6   c   9

In datatable, you combine frames along the columns using the
:meth:`dt.Frame.cbind` method::

    >>> DT1 = dt.Frame(df1)
    >>> DT2 = dt.Frame(df2)
    >>> DT3 = dt.Frame(df3)
    >>> dt.cbind([DT1, DT2, DT3])
       | A          B  C          D  E          F
       | str32  int64  str32  int64  str32  int64
    -- + -----  -----  -----  -----  -----  -----
     0 | a          0  b          4  c          7
     1 | a          1  b          5  c          8
     2 | a          2  b          6  c          9
    [3 rows x 6 columns]

In pandas, if you concatenate dataframes along the rows, and the columns do not
match, a dataframe of all the columns is returned, with null values for the
missing rows::

    >>> # pandas
    >>> pd.concat([df1, df2, df3], axis = 0)
        A    B    C    D    E    F
    0   a    0.0  NaN  NaN  NaN  NaN
    1   a    1.0  NaN  NaN  NaN  NaN
    2   a    2.0  NaN  NaN  NaN  NaN
    0   NaN  NaN  b    4.0  NaN  NaN
    1   NaN  NaN  b    5.0  NaN  NaN
    2   NaN  NaN  b    6.0  NaN  NaN
    0   NaN  NaN  NaN  NaN  c    7.0
    1   NaN  NaN  NaN  NaN  c    8.0
    2   NaN  NaN  NaN  NaN  c    9.0

In datatable, if you concatenate along the rows and the columns in the frames
do not match, you get an error message; you can however force the row
combinations, by passing ``force=True``::

    >>> # datatable
    >>> dt.rbind([DT1, DT2, DT3], force=True)
       | A          B  C          D  E          F
       | str32  int64  str32  int64  str32  int64
    -- + -----  -----  -----  -----  -----  -----
     0 | a          0  NA        NA  NA        NA
     1 | a          1  NA        NA  NA        NA
     2 | a          2  NA        NA  NA        NA
     3 | NA        NA  b          4  NA        NA
     4 | NA        NA  b          5  NA        NA
     5 | NA        NA  b          6  NA        NA
     6 | NA        NA  NA        NA  c          7
     7 | NA        NA  NA        NA  c          8
     8 | NA        NA  NA        NA  c          9
    [9 rows x 6 columns]

.. note::

    :func:`rbind()` and :func:`cbind()` methods exist for the frames, and
    operate in-place.



Join/merge
----------

pandas has a variety of options for joining dataframes, using the ``join``
or ``merge`` method; in datatable, only the left join is possible, and there
are certain limitations. You have to set keys on the dataframe to be joined,
and for that, the keyed columns must be unique. The main function in datatable
for joining dataframes based on column values is the :func:`join()` function.
As such, our comparison will be limited to left-joins only.

In pandas, you can join dataframes easily with the ``merge`` method::

    >>> df1 = pd.DataFrame({"x" : ["b"]*3 + ["a"]*3 + ["c"]*3,
    ...                     "y" : [1, 3, 6] * 3,
    ...                     "v" : range(1, 10)})
    >>> df2 = pd.DataFrame({"x": ('c','b'),
    ...                     "v": (8,7),
    ...                     "foo": (4,2)})
    >>> df1.merge(df2, on="x", how="left")
        x   y   v_x v_y  foo
    0   b   1   1   7.0  2.0
    1   b   3   2   7.0  2.0
    2   b   6   3   7.0  2.0
    3   a   1   4   NaN  NaN
    4   a   3   5   NaN  NaN
    5   a   6   6   NaN  NaN
    6   c   1   7   8.0  4.0
    7   c   3   8   8.0  4.0
    8   c   6   9   8.0  4.0

In datatable, there are limitations currently. First, the joining dataframe must
be keyed. Second, the values in the column(s) used as the joining key(s) must be
unique, otherwise the keying operation will fail. Third, the join columns must
have the same name.

.. code-block:: python

    >>> DT1 = dt.Frame(df1)
    >>> DT2 = dt.Frame(df2)
    >>>
    >>> # set key on DT2
    >>> DT2.key = 'x'
    >>>
    >>> DT1[:, :, join(DT2)]
       | x          y      v    v.0    foo
       | str32  int64  int64  int64  int64
    -- + -----  -----  -----  -----  -----
     0 | b          1      1      7      2
     1 | b          3      2      7      2
     2 | b          6      3      7      2
     3 | a          1      4     NA     NA
     4 | a          3      5     NA     NA
     5 | a          6      6     NA     NA
     6 | c          1      7      8      4
     7 | c          3      8      8      4
     8 | c          6      9      8      4
    [9 rows x 5 columns]

More details about joins in datatable can be found at the :func:`join()` API
and have a look at the :ref:`Tutorial on the join operator <join tutorial>`.



More examples
-------------

This section shows how some solutions in pandas can be translated to datatable;
the examples used here, as well as the pandas solutions, are from the
`pandas cookbook`_.

Feel free to submit a pull request on `github`_ for examples you would like to
share with the community.


if-then-else
~~~~~~~~~~~~

.. code-block:: python

    >>> # Initial data frame:
    >>> df = pd.DataFrame({"AAA": [4, 5, 6, 7],
    ...                    "BBB": [10, 20, 30, 40],
    ...                    "CCC": [100, 50, -30, -50]})
    >>> df
        AAA BBB CCC
    0   4   10  100
    1   5   20  50
    2   6   30  -30
    3   7   40  -50

In pandas this can be achieved using numpy’s `where() <np.where_>`_::

    >>> df['logic'] = np.where(df['AAA'] > 5, 'high', 'low')
        AAA  BBB  CCC  logic
    0    4   10   100    low
    1    5   20    50    low
    2    6   30   -30   high
    3    7   40   -50   high

In datatable, this can be solved using the :func:`ifelse()` function::

    >>> # datatable
    >>> DT = dt.Frame(df)
    >>> DT["logic"] = ifelse(f.AAA > 5, "high", "low")
    >>> DT
       |   AAA    BBB    CCC  logic
       | int64  int64  int64  str32
    -- + -----  -----  -----  -----
     0 |     4     10    100  low
     1 |     5     20     50  low
     2 |     6     30    -30  high
     3 |     7     40    -50  high
    [4 rows x 4 columns]


Select rows with data closest to certain value
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

    >>> # pandas
    >>> df = pd.DataFrame({"AAA": [4, 5, 6, 7],
    ...                    "BBB": [10, 20, 30, 40],
    ...                    "CCC": [100, 50, -30, -50]})
    >>> aValue = 43.0

Solution in pandas, using argsort::

    >>> df.loc[(df.CCC - aValue).abs().argsort()]
         AAA  BBB  CCC
    1    5    20    50
    0    4    10   100
    2    6    30   -30
    3    7    40   -50

In datatable, the :func:`sort` function can be used to rearrange rows in the
desired order::

    >>> DT = dt.Frame(df)
    >>> DT[:, :, sort(dt.math.abs(f.CCC - aValue))]
       |   AAA    BBB    CCC
       | int64  int64  int64
    -- + -----  -----  -----
     0 |     5     20     50
     1 |     4     10    100
     2 |     6     30    -30
     3 |     7     40    -50
    [4 rows x 3 columns]



Efficiently and dynamically creating new columns using applymap
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

    >>> # pandas
    >>> df = pd.DataFrame({"AAA": [1, 2, 1, 3],
    ...                    "BBB": [1, 1, 2, 2],
    ...                    "CCC": [2, 1, 3, 1]})
       AAA  BBB CCC
    0   1   1   2
    1   2   1   1
    2   1   2   3
    3   3   2   1

    >>> source_cols = df.columns
    >>> new_cols = [str(x) + "_cat" for x in source_cols]
    >>> categories = {1: 'Alpha', 2: 'Beta', 3: 'Charlie'}
    >>> df[new_cols] = df[source_cols].applymap(categories.get)
    >>> df
        AAA  BBB  CCC  AAA_cat  BBB_cat  CCC_cat
    0    1    1    2   Alpha    Alpha    Beta
    1    2    1    1   Beta     Alpha    Alpha
    2    1    2    3   Alpha    Beta     Charlie
    3    3    2    1   Charlie  Beta     Alpha


We can replicate the solution above in datatable::

    >>> # datatable
    >>> import itertools as it
    >>>
    >>> DT = dt.Frame(df)
    >>> mixer = it.product(DT.names, categories)
    >>> conditions = [(name, f[name] == value, categories[value])
    ...               for name, value in mixer]
    >>> for name, cond, value in conditions:
    ...    DT[cond, f"{name}_cat"] = value
       |   AAA    BBB    CCC  AAA_cat  BBB_cat  CCC_cat
       | int64  int64  int64  str32    str32    str32
    -- + -----  -----  -----  -------  -------  -------
     0 |     1      1      2  Alpha    Alpha    Beta
     1 |     2      1      1  Beta     Alpha    Alpha
     2 |     1      2      3  Alpha    Beta     Charlie
     3 |     3      2      1  Charlie  Beta     Alpha
    [4 rows x 6 columns]


Keep other columns when using ``min()`` with groupby
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

    >>> # pandas
    >>> df = pd.DataFrame({'AAA': [1, 1, 1, 2, 2, 2, 3, 3],
    ...                    'BBB': [2, 1, 3, 4, 5, 1, 2, 3]})
    >>> df
        AAA  BBB
    0    1    2
    1    1    1
    2    1    3
    3    2    4
    4    2    5
    5    2    1
    6    3    2
    7    3    3

Solution in pandas::

    >>> df.loc[df.groupby("AAA")["BBB"].idxmin()]
        AAA  BBB
    1    1    1
    5    2    1
    6    3    2

In datatable, you can :func:`sort()` within a group, to achieve the same result above::

    >>> # datatable
    >>> DT = dt.Frame(df)
    >>> DT[0, :, by("AAA"), sort(f.BBB)]
       |   AAA    BBB
       | int64  int64
    -- + -----  -----
     0 |     1      1
     1 |     2      1
     2 |     3      2
    [3 rows x 2 columns]


Apply to different items in a group
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

    >>> # pandas
    >>> df = pd.DataFrame({'animal': 'cat dog cat fish dog cat cat'.split(),
    ...                    'size': list('SSMMMLL'),
    ...                    'weight': [8, 10, 11, 1, 20, 12, 12],
    ...                    'adult': [False] * 5 + [True] * 2})
    >>> df
      animal size  weight  adult
    0    cat    S       8  False
    1    dog    S      10  False
    2    cat    M      11  False
    3   fish    M       1  False
    4    dog    M      20  False
    5    cat    L      12   True
    6    cat    L      12   True


Solution in pandas::

    >>> def GrowUp(x):
    ...     avg_weight = sum(x[x['size'] == 'S'].weight * 1.5)
    ...     avg_weight += sum(x[x['size'] == 'M'].weight * 1.25)
    ...     avg_weight += sum(x[x['size'] == 'L'].weight)
    ...     avg_weight /= len(x)
    ...     return pd.Series(['L', avg_weight, True],
    ...                      index=['size', 'weight', 'adult'])
    >>>
    >>> expected_df = gb.apply(GrowUp)
            size   weight  adult
    animal
    cat       L  12.4375   True
    dog       L  20.0000   True
    fish      L   1.2500   True

In datatable, we can use the :func:`ifelse()` function to replicate
the solution above, since it is based on a series of conditions::

    >>> DT = dt.Frame(df)
    >>>
    >>> conditions = ifelse(f.size == "S", f.weight * 1.5,
    ...                     f.size == "M", f.weight * 1.25,
    ...                     f.size == "L", f.weight,
    ...                     None)
    >>>
    >>> DT[:, {"size": "L",
    ...        "avg_wt": dt.sum(conditions) / dt.count(),
    ...        "adult": True},
    ...    by("animal")]
       | animal  size    avg_wt  adult
       | str32   str32  float64  bool8
    -- + ------  -----  -------  -----
     0 | cat     L      12.4375      1
     1 | dog     L      20           1
     2 | fish    L       1.25        1
    [3 rows x 4 columns]


.. note::

    :func:`ifelse()` can take multiple conditions, along with a default
    return value.

.. note::

    Custom functions are not supported in datatable yet.


Sort groups by aggregated data
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

    >>> # pandas
    >>> df = pd.DataFrame({'code': ['foo', 'bar', 'baz'] * 2,
    ...                    'data': [0.16, -0.21, 0.33, 0.45, -0.59, 0.62],
    ...                    'flag': [False, True] * 3})
        code    data    flag
    0    foo    0.16    False
    1    bar   -0.21    True
    2    baz    0.33    False
    3    foo    0.45    True
    4    bar   -0.59    False
    5    baz    0.62    True

Solution in pandas::

    >>> code_groups = df.groupby('code')
    >>> agg_n_sort_order = code_groups[['data']].transform(sum).sort_values(by='data')
    >>> sorted_df = df.loc[agg_n_sort_order.index]
    >>> sorted_df
        code  data   flag
    1   bar  -0.21   True
    4   bar  -0.59  False
    0   foo   0.16  False
    3   foo   0.45   True
    2   baz   0.33  False
    5   baz   0.62   True

The solution above sorts the data based on the sum of the ``data`` column per
group in the ``code`` column.

We can replicate this in datatable::

    >>> DT = dt.Frame(df)
    >>> DT[:, update(sum_data = dt.sum(f.data)), by("code")]
    >>> DT[:, :-1, sort(f.sum_data)]
       | code      data   flag
       | str32  float64  bool8
    -- + -----  -------  -----
     0 | bar      -0.21      1
     1 | bar      -0.59      0
     2 | foo       0.16      0
     3 | foo       0.45      1
     4 | baz       0.33      0
     5 | baz       0.62      1
    [6 rows x 3 columns]


Create a value counts column and reassign back to the DataFrame
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

    >>> # pandas
    >>> df = pd.DataFrame({'Color': 'Red Red Red Blue'.split(),
    ...                    'Value': [100, 150, 50, 50]})
    >>> df
       Color  Value
    0    Red    100
    1    Red    150
    2    Red     50
    3   Blue     50

Solution in pandas::

    >>> df['Counts'] = df.groupby(['Color']).transform(len)
    >>> df
        Color  Value  Counts
    0   Red      100       3
    1   Red      150       3
    2   Red       50       3
    3   Blue      50       1

In datatable, you can replicate the solution above with the :func:`count()`
function::

    >>> DT = dt.Frame(df)
    >>> DT[:, update(Counts=dt.count()), by("Color")]
    >>> DT
       | Color  Value  Counts
       | str32  int64   int64
    -- + -----  -----  ------
     0 | Red      100       3
     1 | Red      150       3
     2 | Red       50       3
     3 | Blue      50       1
    [4 rows x 3 columns]


Shift groups of the values in a column based on the index
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

    >>> # pandas
    >>> df = pd.DataFrame({'line_race': [10, 10, 8, 10, 10, 8],
    ...                    'beyer': [99, 102, 103, 103, 88, 100]},
    ...                    index=['Last Gunfighter', 'Last Gunfighter',
    ...                           'Last Gunfighter', 'Paynter', 'Paynter',
    ...                           'Paynter'])
    >>> df
                        line_race  beyer
    Last Gunfighter         10     99
    Last Gunfighter         10    102
    Last Gunfighter          8    103
    Paynter                 10    103
    Paynter                 10     88
    Paynter                  8    100


Solution in pandas::

    >>> df['beyer_shifted'] = df.groupby(level=0)['beyer'].shift(1)
    >>> df
                        line_race  beyer  beyer_shifted
    Last Gunfighter         10     99            NaN
    Last Gunfighter         10    102           99.0
    Last Gunfighter          8    103          102.0
    Paynter                 10    103            NaN
    Paynter                 10     88          103.0
    Paynter                  8    100           88.0

Datatable has an equivalent :func:`shift()` function::

    >>> DT = dt.Frame(df.reset_index())
    >>> DT[:, update(beyer_shifted = dt.shift(f.beyer)), by("index")]
    >>> DT
       | index            line_race  beyer  beyer_shifted
       | str32                int64  int64          int64
    -- + ---------------  ---------  -----  -------------
     0 | Last Gunfighter         10     99             NA
     1 | Last Gunfighter         10    102             99
     2 | Last Gunfighter          8    103            102
     3 | Paynter                 10    103             NA
     4 | Paynter                 10     88            103
     5 | Paynter                  8    100             88
    [6 rows x 4 columns]


Frequency table like `plyr`_ in R
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

    >>> grades = [48, 99, 75, 80, 42, 80, 72, 68, 36, 78]
    >>> df = pd.DataFrame({'ID': ["x%d" % r for r in range(10)],
    ...                    'Gender': ['F', 'M', 'F', 'M', 'F',
    ...                               'M', 'F', 'M', 'M', 'M'],
    ...                    'ExamYear': ['2007', '2007', '2007', '2008', '2008',
    ...                                 '2008', '2008', '2009', '2009', '2009'],
    ...                    'Class': ['algebra', 'stats', 'bio', 'algebra',
    ...                              'algebra', 'stats', 'stats', 'algebra',
    ...                              'bio', 'bio'],
    ...                    'Participated': ['yes', 'yes', 'yes', 'yes', 'no',
    ...                                     'yes', 'yes', 'yes', 'yes', 'yes'],
    ...                    'Passed': ['yes' if x > 50 else 'no' for x in grades],
    ...                    'Employed': [True, True, True, False,
    ...                                 False, False, False, True, True, False],
    ...                    'Grade': grades})
    >>> df
        ID  Gender  ExamYear    Class     Participated  Passed  Employed  Grade
    0   x0  F           2007    algebra   yes           no      True         48
    1   x1  M           2007    stats     yes           yes     True         99
    2   x2  F           2007    bio       yes           yes     True         75
    3   x3  M           2008    algebra   yes           yes     False        80
    4   x4  F           2008    algebra   no            no      False        42
    5   x5  M           2008    stats     yes           yes     False        80
    6   x6  F           2008    stats     yes           yes     False        72
    7   x7  M           2009    algebra   yes           yes     True         68
    8   x8  M           2009    bio       yes           no      True         36
    9   x9  M           2009    bio       yes           yes     False        78


Solution in pandas::

    >>> df.groupby('ExamYear').agg({'Participated': lambda x: x.value_counts()['yes'],
    ...                             'Passed': lambda x: sum(x == 'yes'),
    ...                             'Employed': lambda x: sum(x),
    ...                             'Grade': lambda x: sum(x) / len(x)})
                Participated  Passed  Employed      Grade
    ExamYear
    2007                 3       2         3        74.000000
    2008                 3       3         0        68.500000
    2009                 3       2         2        60.666667


In datatable you can nest conditions within aggregations::

    >>> DT = dt.Frame(df)

    >>> DT[:, {"Participated": dt.sum(f.Participated == "yes"),
    ...        "Passed": dt.sum(f.Passed == "yes"),
    ...        "Employed": dt.sum(f.Employed),
    ...        "Grade": dt.mean(f.Grade)},
    ...    by("ExamYear")]
       | ExamYear  Participated  Passed  Employed    Grade
       | str32            int64   int64     int64  float64
    -- + --------  ------------  ------  --------  -------
     0 | 2007                 3       2         3  74
     1 | 2008                 3       3         0  68.5
     2 | 2009                 3       2         2  60.6667
    [3 rows x 5 columns]



Missing functionality
---------------------

Listed below are some functions in pandas that do not have an equivalent in datatable yet, and are likely to be implemented:

- Reshaping functions

  - `melt <https://pandas.pydata.org/docs/reference/api/pandas.melt.html>`__
  - `wide_to_long <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.wide_to_long.html>`__
  - `pivot_table <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.pivot_table.html>`__

- Convenience function for filtering and subsetting

  - `isin <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.isin.html>`__

- `Datetime functions <https://pandas.pydata.org/pandas-docs/stable/user_guide/timeseries.html>`__

- Missing values

  - `forward fill <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.ffill.html>`__
  - `backward fill <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.bfill.html>`__

- Aggregation functions, such as

  - `expanding <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.expanding.html>`__
  - `rolling <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.rolling.html>`__

- String functions, such as

  - `string split <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.Series.str.split.html>`__
  - `string extract <https://pandas.pydata.org/pandas-docs/stable/generated/pandas.Series.str.extract.html>`__
  - `string replace <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.Series.str.replace.html>`__

- Custom function application, via `pd.apply`_ and `pd.pipe`_.


If there are any functions that you would like to see in datatable, please
head over to `github`_ and raise a feature request.



.. _`pandas`:            https://pandas.pydata.org/pandas-docs/stable/index.html
.. _`pandas cookbook`:   https://pandas.pydata.org/pandas-docs/stable/user_guide/cookbook.html
.. _`DataFrame`:         https://pandas.pydata.org/pandas-docs/stable/user_guide/dsintro.html#dataframe
.. _`Series`:            https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.Series.html#pandas.Series
.. _`pd.Series.argsort`: https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.Series.argsort.html
.. _`pd.transform`:      https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.core.groupby.DataFrameGroupBy.transform.html
.. _`pd.apply`:          https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.apply.html
.. _`pd.pipe`:           https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.pipe.html
.. _`np.where`:          https://numpy.org/doc/stable/reference/generated/numpy.where.html
.. _`np.argsort`:        https://numpy.org/doc/stable/reference/generated/numpy.argsort.html
.. _`plyr`:              https://www.rdocumentation.org/packages/plyr/versions/1.8.6
.. _`github`:            https://github.com/h2oai/datatable/issues
