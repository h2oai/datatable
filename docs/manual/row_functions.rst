
Row Functions
=============

Functions :func:`rowall`, :func:`rowany`, :func:`rowcount`, :func:`rowfirst`,
:func:`rowlast`, :func:`rowmax`, :func:`rowmean`, :func:`rowmin`, :func:`rowsd`,
:func:`rowsum` are functions that aggregate across rows instead of columns and
return a single column. These functions are equivalent to `pandas`_ aggregation
functions with parameter ``(axis=1)``.

These functions make it easy to compute rowwise aggregations -- for instance,
you may want the sum of columns ``A``, ``B``, ``C`` and ``D``. You could say:
``f.A + f.B + f.C + f.D``. Rowsum makes it easier -- ``dt.rowsum(f['A':'D'])``.


rowall, rowany
--------------

These work only on boolean expressions -- :func:`rowall` checks if all the
values in the row are ``True``, while :func:`rowany` checks if any value in the
row is ``True``. It is similar to pandas' `all <pd.all_>`_ or `any <pd.any_>`_
with parameter ``(axis=1)``. A single boolean column is returned::

    >>> from datatable import dt, f, by
    >>>
    >>> df = dt.Frame({'A': [True, True], 'B': [True, False]})
    >>> df
       |     A      B
       | bool8  bool8
    -- + -----  -----
     0 |     1      1
     1 |     1      0
    [2 rows x 2 columns]
    >>> # rowall :
    ... df[:, dt.rowall(f[:])]
       |    C0
       | bool8
    -- + -----
     0 |     1
     1 |     0
    [2 rows x 1 column]
    >>> # rowany :
    ... df[:, dt.rowany(f[:])]
       |    C0
       | bool8
    -- + -----
     0 |     1
     1 |     1
    [2 rows x 1 column]


The single boolean column that is returned can be very handy when filtering in the ``i`` section.

Filter for rows where at least one cell is greater than 0::

    >>> df = dt.Frame({'a': [0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0],
    ...                'b': [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    ...                'c': [0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0],
    ...                'd': [0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0],
    ...                'e': [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    ...                'f': [0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0]})
    >>> df
       |    a     b      c     d     e     f
       | int8  int8  int32  int8  int8  int8
    -- + ----  ----  -----  ----  ----  ----
     0 |    0     0      0     0     0     0
     1 |    0     0      0     0     0     1
     2 |    0     0      0     0     0     0
     3 |    0     0      0     0     0     0
     4 |    0     0      0     0     0     0
     5 |    0     0      5     0     0     0
     6 |    1     0      0     0     0     0
     7 |    0     0      0     0     0     0
     8 |    0     0      0     1     0     0
     9 |    1     0      0     0     0     0
    10 |    0     0      0     0     0     0
    [11 rows x 6 columns]

    >>> df[dt.rowany(f[:] > 0), :]
       |    a     b      c     d     e     f
       | int8  int8  int32  int8  int8  int8
    -- + ----  ----  -----  ----  ----  ----
     0 |    0     0      0     0     0     1
     1 |    0     0      5     0     0     0
     2 |    1     0      0     0     0     0
     3 |    0     0      0     1     0     0
     4 |    1     0      0     0     0     0
    [5 rows x 6 columns]


Filter for rows where all the cells are 0::

    >>> df[dt.rowall(f[:] == 0), :]
       |    a     b      c     d     e     f
       | int8  int8  int32  int8  int8  int8
    -- + ----  ----  -----  ----  ----  ----
     0 |    0     0      0     0     0     0
     1 |    0     0      0     0     0     0
     2 |    0     0      0     0     0     0
     3 |    0     0      0     0     0     0
     4 |    0     0      0     0     0     0
     5 |    0     0      0     0     0     0
    [6 rows x 6 columns]


Filter for rows where all the columns' values are the same::

    >>> df = dt.Frame("""Name  A1   A2  A3  A4
    ...                  deff  0    0   0   0
    ...                  def1  0    1   0   0
    ...                  def2  0    0   0   0
    ...                  def3  1    0   0   0
    ...                  def4  0    0   0   0""")
    >>>
    >>> # compare the first integer column with the rest,
    ... # use rowall to find rows where all is True
    ... # and filter with the resulting boolean
    ... df[dt.rowall(f[1]==f[1:]), :]
       | Name      A1     A2     A3     A4
       | str32  bool8  bool8  bool8  bool8
    -- + -----  -----  -----  -----  -----
     0 | deff       0      0      0      0
     1 | def2       0      0      0      0
     2 | def4       0      0      0      0
    [3 rows x 5 columns]


Filter for rows where the values are increasing::

    >>> df = dt.Frame({"A": [1, 2, 6, 4],
    ...                "B": [2, 4, 5, 6],
    ...                "C": [3, 5, 4, 7],
    ...                "D": [4, -3, 3, 8],
    ...                "E": [5, 1, 2, 9]})
    >>> df
       |     A      B      C      D      E
       | int32  int32  int32  int32  int32
    -- + -----  -----  -----  -----  -----
     0 |     1      2      3      4      5
     1 |     2      4      5     -3      1
     2 |     6      5      4      3      2
     3 |     4      6      7      8      9
    [4 rows x 5 columns]
    >>> df[dt.rowall(f[1:] >= f[:-1]), :]
       |     A      B      C      D      E
       | int32  int32  int32  int32  int32
    -- + -----  -----  -----  -----  -----
     0 |     1      2      3      4      5
     1 |     4      6      7      8      9
    [2 rows x 5 columns]



rowfirst, rowlast
-----------------
These look for the first and last non-missing value in a row respectively::

    >>> df = dt.Frame({'A':[1, None, None, None],
    ...                'B':[None, 3, 4, None],
    ...                'C':[2, None, 5, None]})
    >>> df
       |    A      B      C
       | int8  int32  int32
    -- + ----  -----  -----
     0 |    1     NA      2
     1 |   NA      3     NA
     2 |   NA      4      5
     3 |   NA     NA     NA
    [4 rows x 3 columns]
    >>> # rowfirst :
    >>> df[:, dt.rowfirst(f[:])]
       |    C0
       | int32
    -- + -----
     0 |     1
     1 |     3
     2 |     4
     3 |    NA
    [4 rows x 1 column]
    >>> # rowlast :
    ... df[:, dt.rowlast(f[:])]
       |    C0
       | int32
    -- + -----
     0 |     2
     1 |     3
     2 |     5
     3 |    NA
    [4 rows x 1 column]


Get rows where the last value in the row is greater than the first value
in the row::

    >>> df = dt.Frame({'a': [50, 40, 30, 20, 10],
    ...                'b': [60, 10, 40, 0, 5],
    ...                'c': [40, 30, 20, 30, 40]})
    >>> df
       |     a      b      c
       | int32  int32  int32
    -- + -----  -----  -----
     0 |    50     60     40
     1 |    40     10     30
     2 |    30     40     20
     3 |    20      0     30
     4 |    10      5     40
    [5 rows x 3 columns]
    >>> df[dt.rowlast(f[:]) > dt.rowfirst(f[:]), :]
       |     a      b      c
       | int32  int32  int32
    -- + -----  -----  -----
     0 |    20      0     30
     1 |    10      5     40
    [2 rows x 3 columns]


rowmax, rowmin
--------------
These get the maximum and minimum values per row, respectively::

    >>> df = dt.Frame({"C": [2, 5, 30, 20, 10],
    ...                "D": [10, 8, 20, 20, 1]})
    >>> df
       |     C      D
       | int32  int32
    -- + -----  -----
     0 |     2     10
     1 |     5      8
     2 |    30     20
     3 |    20     20
     4 |    10      1
    [5 rows x 2 columns]
    >>> # rowmax
    ... df[:, dt.rowmax(f[:])]
       |    C0
       | int32
    -- + -----
     0 |    10
     1 |     8
     2 |    30
     3 |    20
     4 |    10
    [5 rows x 1 column]
    >>> # rowmin
    ... df[:, dt.rowmin(f[:])]
       |    C0
       | int32
    -- + -----
     0 |     2
     1 |     5
     2 |    20
     3 |    20
     4 |     1
    [5 rows x 1 column]


Find the difference between the maximum and minimum of each row::

    >>> df = dt.Frame("""Value1  Value2  Value3  Value4
    ...                     5       4      3        2
    ...                     4       3      2        1
    ...                     3       3      5        1""")
    >>>
    >>> df[:, dt.update(max_min = dt.rowmax(f[:]) - dt.rowmin(f[:]))]
    >>> df
       | Value1  Value2  Value3  Value4  max_min
       |  int32   int32   int32   int32    int32
    -- + ------  ------  ------  ------  -------
     0 |      5       4       3       2        3
     1 |      4       3       2       1        3
     2 |      3       3       5       1        4
    [3 rows x 5 columns]



rowsum, rowmean, rowcount, rowsd
--------------------------------
:func:`rowsum` and :func:`rowmean` get the sum and mean of rows respectively;
:func:`rowcount` counts the number of non-missing values in a row, while
:func:`rowsd` aggregates a row to get the standard deviation.

Get the count, sum, mean and standard deviation for each row::

    >>> df = dt.Frame("""ORD  A   B   C    D
    ...                 198  23  45  NaN  12
    ...                 138  25  NaN NaN  62
    ...                 625  52  36  49   35
    ...                 457  NaN NaN NaN  82
    ...                 626  52  32  39   45""")
    >>>
    >>> df[:, dt.update(rowcount = dt.rowcount(f[:]),
    ...                 rowsum = dt.rowsum(f[:]),
    ...                 rowmean = dt.rowmean(f[:]),
    ...                 rowsd = dt.rowsd(f[:])
    ...                 )]
    >>> df
       |   ORD        A        B        C      D  rowcount   rowsum  rowmean     rowsd
       | int32  float64  float64  float64  int32     int32  float64  float64   float64
    -- + -----  -------  -------  -------  -----  --------  -------  -------  --------
     0 |   198       23       45       NA     12         4      278     69.5   86.7583
     1 |   138       25       NA       NA     62         3      225     75     57.6108
     2 |   625       52       36       49     35         5      797    159.4  260.389
     3 |   457       NA       NA       NA     82         2      539    269.5  265.165
     4 |   626       52       32       39     45         5      794    158.8  261.277
    [5 rows x 9 columns]


Find rows where the number of nulls is greater than 3::

    >>> df = dt.Frame({'city': ["city1", "city2", "city3", "city4"],
    ...                'state': ["state1", "state2", "state3", "state4"],
    ...                '2005': [144, 205, 123, None],
    ...                '2006': [173, 211, 123, 124],
    ...                '2007': [None, None, None, None],
    ...                '2008': [None, 206, None, None],
    ...                '2009': [None, None, 124, 123],
    ...                '2010': [128, 273, None, None]})
    >>> df
       | city   state    2005   2006  2007   2008   2009   2010
       | str32  str32   int32  int32  void  int32  int32  int32
    -- + -----  ------  -----  -----  ----  -----  -----  -----
     0 | city1  state1    144    173    NA     NA     NA    128
     1 | city2  state2    205    211    NA    206     NA    273
     2 | city3  state3    123    123    NA     NA    124     NA
     3 | city4  state4     NA    124    NA     NA    123     NA
    [4 rows x 8 columns]
    >>> # get columns that are null, then sum on the rows
    ... # and finally filter where the sum is greater than 3
    ... df[dt.rowsum(dt.isna(f[:])) > 3, :]
       | city   state    2005   2006  2007   2008   2009   2010
       | str32  str32   int32  int32  void  int32  int32  int32
    -- + -----  ------  -----  -----  ----  -----  -----  -----
     0 | city4  state4     NA    124    NA     NA    123     NA
    [1 row x 8 columns]


Rowwise sum of the float columns::

    >>> df = dt.Frame("""ID   W_1       W_2     W_3
    ...                  1    0.1       0.2     0.3
    ...                  1    0.2       0.4     0.5
    ...                  2    0.3       0.3     0.2
    ...                  2    0.1       0.3     0.4
    ...                  2    0.2       0.0     0.5
    ...                  1    0.5       0.3     0.2
    ...                  1    0.4       0.2     0.1""")
    >>>
    >>> df[:, dt.update(sum_floats = dt.rowsum(f[float]))]
    >>> df
       |    ID      W_1      W_2      W_3  sum_floats
       | int32  float64  float64  float64     float64
    -- + -----  -------  -------  -------  ----------
     0 |     1      0.1      0.2      0.3         0.6
     1 |     1      0.2      0.4      0.5         1.1
     2 |     2      0.3      0.3      0.2         0.8
     3 |     2      0.1      0.3      0.4         0.8
     4 |     2      0.2      0        0.5         0.7
     5 |     1      0.5      0.3      0.2         1
     6 |     1      0.4      0.2      0.1         0.7
    [7 rows x 5 columns]



More Examples
-------------

Divide columns ``A``, ``B``, ``C``, ``D`` by the ``total`` column, square it
and sum rowwise::

    >>> df = dt.Frame({'A': [2, 3],
    ...                'B': [1, 2],
    ...                'C': [0, 1],
    ...                'D': [1, 0],
    ...                'total': [4, 6]})
    >>> df
       |     A      B     C     D  total
       | int32  int32  int8  int8  int32
    -- + -----  -----  ----  ----  -----
     0 |     2      1     0     1      4
     1 |     3      2     1     0      6
    [2 rows x 5 columns]
    >>> df[:, update(result = dt.rowsum((f[:-1]/f[-1])**2))]
    >>> df
       |     A      B     C     D  total    result
       | int32  int32  int8  int8  int32   float64
    -- + -----  -----  ----  ----  -----  --------
     0 |     2      1     0     1      4  0.375
     1 |     3      2     1     0      6  0.388889
    [2 rows x 6 columns]


Get the row sum of the ``COUNT`` columns::

    >>> df = dt.Frame("""USER OBSERVATION COUNT.1 COUNT.2 COUNT.3
    ...                     A    1           0       1       1
    ...                     A    2           1       1       2
    ...                     A    3           3       0       0""")
    >>>
    >>> columns = [f[column] for column in df.names if column.startswith("COUNT")]
    >>> df[:, update(total = dt.rowsum(columns))]
    >>> df
       | USER   OBSERVATION  COUNT.1  COUNT.2  COUNT.3  total
       | str32        int32    int32    bool8    int32  int32
    -- + -----  -----------  -------  -------  -------  -----
     0 | A                1        0        1        1      2
     1 | A                2        1        1        2      4
     2 | A                3        3        0        0      3
    [3 rows x 6 columns]


Sum selected columns rowwise::

    >>> df = dt.Frame({'location' : ("a","b","c","d"),
    ...                'v1' : (3,4,3,3),
    ...                'v2' : (4,56,3,88),
    ...                'v3' : (7,6,2,9),
    ...                'v4':  (7,6,1,9),
    ...                'v5' : (4,4,7,9),
    ...                'v6' : (2,8,4,6)})
    >>>
    >>> df
       | location     v1     v2     v3     v4     v5     v6
       | str32     int32  int32  int32  int32  int32  int32
    -- + --------  -----  -----  -----  -----  -----  -----
     0 | a             3      4      7      7      4      2
     1 | b             4     56      6      6      4      8
     2 | c             3      3      2      1      7      4
     3 | d             3     88      9      9      9      6
    [4 rows x 7 columns]
    >>> df[:, {"x1": dt.rowsum(f[1:4]), "x2": dt.rowsum(f[4:])}]
       |    x1     x2
       | int32  int32
    -- + -----  -----
     0 |    14     13
     1 |    66     18
     2 |     8     12
     3 |   100     24
    [4 rows x 2 columns]


.. _`pandas`: https://pandas.pydata.org/pandas-docs/stable/index.html
.. _`pd.all`: https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.all.html
.. _`pd.any`: https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.any.html
