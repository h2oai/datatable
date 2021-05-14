.. _`Grouping with by`:

Grouping with ``by()``
======================

The :func:`by()` modifier splits a dataframe into groups, either via the
provided column(s) or :ref:`f-expressions`, and then applies ``i`` and ``j``
within each group.  This `split-apply-combine`_ strategy allows for a number
of operations:

- Aggregations per group,

- Transformation of a column or columns, where the shape of the dataframe is
  maintained,

- Filtration, where some data are kept and the others discarded, based on a
  condition or conditions.


Aggregation
-----------

The aggregate function is applied in the ``j`` section.

Group by one column::

    >>> from datatable import (dt, f, by, ifelse, update, sort,
    ...                        count, min, max, mean, sum, rowsum)
    >>>
    >>> df =  dt.Frame("""Fruit   Date       Name  Number
    ...                   Apples  10/6/2016  Bob     7
    ...                   Apples  10/6/2016  Bob     8
    ...                   Apples  10/6/2016  Mike    9
    ...                   Apples  10/7/2016  Steve  10
    ...                   Apples  10/7/2016  Bob     1
    ...                   Oranges 10/7/2016  Bob     2
    ...                   Oranges 10/6/2016  Tom    15
    ...                   Oranges 10/6/2016  Mike   57
    ...                   Oranges 10/6/2016  Bob    65
    ...                   Oranges 10/7/2016  Tony    1
    ...                   Grapes  10/7/2016  Bob     1
    ...                   Grapes  10/7/2016  Tom    87
    ...                   Grapes  10/7/2016  Bob    22
    ...                   Grapes  10/7/2016  Bob    12
    ...                   Grapes  10/7/2016  Tony   15""")
    >>>
    >>> df[:, sum(f.Number), by('Fruit')]
       | Fruit    Number
       | str32     int64
    -- + -------  ------
     0 | Apples       35
     1 | Grapes      137
     2 | Oranges     140
    [3 rows x 2 columns]

Group by multiple columns::

    >>> df[:, sum(f.Number), by('Fruit', 'Name')]
       | Fruit    Name   Number
       | str32    str32   int64
    -- + -------  -----  ------
     0 | Apples   Bob        16
     1 | Apples   Mike        9
     2 | Apples   Steve      10
     3 | Grapes   Bob        35
     4 | Grapes   Tom        87
     5 | Grapes   Tony       15
     6 | Oranges  Bob        67
     7 | Oranges  Mike       57
     8 | Oranges  Tom        15
     9 | Oranges  Tony        1
    [10 rows x 3 columns]

By column position::

    >>> df[:, sum(f.Number), by(f[0])]
       | Fruit    Number
       | str32     int64
    -- + -------  ------
     0 | Apples       35
     1 | Grapes      137
     2 | Oranges     140
    [3 rows x 2 columns]

By boolean expression::

    >>> df[:, sum(f.Number), by(f.Fruit == "Apples")]
       |    C0  Number
       | bool8   int64
    -- + -----  ------
     0 |     0     277
     1 |     1      35
    [2 rows x 2 columns]

Combination of column and boolean expression::

    >>> df[:, sum(f.Number), by(f.Name, f.Fruit == "Apples")]
       | Name      C0  Number
       | str32  bool8   int64
    -- + -----  -----  ------
     0 | Bob        0     102
     1 | Bob        1      16
     2 | Mike       0      57
     3 | Mike       1       9
     4 | Steve      1      10
     5 | Tom        0     102
     6 | Tony       0      16
    [7 rows x 3 columns]

The grouping column can be excluded from the final output::

    >>> df[:, sum(f.Number), by('Fruit', add_columns=False)]
       | Number
       |  int64
    -- + ------
     0 |     35
     1 |    137
     2 |    140
    [3 rows x 1 column]

.. note::

    - The resulting dataframe has the grouping column(s) as the first column(s).
    - The grouping columns are excluded from ``j``, unless explicitly included.
    - The grouping columns are sorted in ascending order.

Apply multiple aggregate functions to a column in the ``j`` section::

    >>> df[:, {"min": min(f.Number),
    ...        "max": max(f.Number)},
    ...    by('Fruit','Date')]
       | Fruit    Date         min    max
       | str32    str32      int32  int32
    -- + -------  ---------  -----  -----
     0 | Apples   10/6/2016      7      9
     1 | Apples   10/7/2016      1     10
     2 | Grapes   10/7/2016      1     87
     3 | Oranges  10/6/2016     15     65
     4 | Oranges  10/7/2016      1      2
    [5 rows x 4 columns]

Functions can be applied across a columnset. Task : Get sum of ``col3`` and
``col4``, grouped by ``col1`` and ``col2``::

    >>> df = dt.Frame(""" col1   col2   col3   col4   col5
    ...                   a      c      1      2      f
    ...                   a      c      1      2      f
    ...                   a      d      1      2      f
    ...                   b      d      1      2      g
    ...                   b      e      1      2      g
    ...                   b      e      1      2      g""")
    >>>
    >>> df[:, sum(f["col3":"col4"]), by('col1', 'col2')]
       | col1   col2    col3   col4
       | str32  str32  int64  int64
    -- + -----  -----  -----  -----
     0 | a      c          2      4
     1 | a      d          1      2
     2 | b      d          1      2
     3 | b      e          2      4
    [4 rows x 4 columns]

Apply different aggregate functions to different columns::

    >>> df[:, [max(f.col3), min(f.col4)], by('col1', 'col2')]
       | col1   col2   col3   col4
       | str32  str32  int8  int32
    -- + -----  -----  ----  -----
     0 | a      c         1      2
     1 | a      d         1      2
     2 | b      d         1      2
     3 | b      e         1      2
    [4 rows x 4 columns]

Nested aggregations in ``j``. Task : Group by column ``idx`` and get the row
sum of ``A`` and ``B``, ``C`` and ``D``::

    >>> df = dt.Frame(""" idx  A   B   C   D   cat
    ...                    J   1   2   3   1   x
    ...                    K   4   5   6   2   x
    ...                    L   7   8   9   3   y
    ...                    M   1   2   3   4   y
    ...                    N   4   5   6   5   z
    ...                    O   7   8   9   6   z""")
    >>>
    >>> df[:,
    ...     {"AB" : sum(rowsum(f['A':'B'])),
    ...      "CD" : sum(rowsum(f['C':'D']))},
    ...    by('cat')
    ...    ]
       | cat       AB     CD
       | str32  int64  int64
    -- + -----  -----  -----
     0 | x         12     12
     1 | y         18     19
     2 | z         24     26
    [3 rows x 3 columns]

Computation between aggregated columns. Task: get the difference between the
largest and smallest value within each group::

    >>> df = dt.Frame("""GROUP VALUE
    ...                   1     5
    ...                   2     2
    ...                   1     10
    ...                   2     20
    ...                   1     7""")
    >>>
    >>> df[:, max(f.VALUE) - min(f.VALUE), by('GROUP')]
       | GROUP     C0
       | int32  int32
    -- + -----  -----
     0 |     1      5
     1 |     2     18
    [2 rows x 2 columns]

Null values are not excluded from the grouping column::

    >>> df = dt.Frame("""  a    b    c
    ...                    1    2.0  3
    ...                    1    NaN  4
    ...                    2    1.0  3
    ...                    1    2.0  2""")
    >>>
    >>> df[:, sum(f[:]), by('b')]
       |       b      a      c
       | float64  int64  int64
    -- + -------  -----  -----
     0 |      NA      1      4
     1 |       1      2      3
     2 |       2      2      5
    [3 rows x 3 columns]

If you wish to ignore null values, first filter them out::

    >>> df[f.b != None, :][:, sum(f[:]), by('b')]
       |       b      a      c
       | float64  int64  int64
    -- + -------  -----  -----
     0 |       1      2      3
     1 |       2      2      5
    [2 rows x 3 columns]


Filtration
----------

This occurs in the ``i`` section of the groupby, where only a subset of the
data per group is needed; selection is limited to integers or slicing.

.. note::

    - ``i`` is applied after the grouping, not before.

    - :ref:`f-expressions` in the ``i`` section is not yet implemented for
      groupby.


Select the first row per group::

    >>> df = dt.Frame("""A   B
    ...                  1  10
    ...                  1  20
    ...                  2  30
    ...                  2  40
    ...                  3  10""")
    >>>
    >>> # passing 0 as index gets the first row after the grouping
    ... # note that python's index starts from 0, not 1
    ...
    >>> df[0, :, by('A')]
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     1     10
     1 |     2     30
     2 |     3     10
    [3 rows x 2 columns]

Select the last row per group::

    >>> df[-1, :, by('A')]
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     1     20
     1 |     2     40
     2 |     3     10
    [3 rows x 2 columns]

Select the nth row per group. Task : select the second row per group::

    >>> df[1, :, by('A')]
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     1     20
     1 |     2     40
    [2 rows x 2 columns]

.. note::

    Filtering this way can be used to drop duplicates; you can decide to keep
    the first or last non-duplicate.

Select the latest entry per group::

    >>> df =  dt.Frame("""   id    product  date
    ...                     220    6647     2014-09-01
    ...                     220    6647     2014-09-03
    ...                     220    6647     2014-10-16
    ...                     826    3380     2014-11-11
    ...                     826    3380     2014-12-09
    ...                     826    3380     2015-05-19
    ...                     901    4555     2014-09-01
    ...                     901    4555     2014-10-05
    ...                     901    4555     2014-11-01""")
    >>>
    >>> df[-1, :, by('id'), sort('date')]
       |    id  product  date
       | int32    int32  str32
    -- + -----  -------  ----------
     0 |   220     6647  2014-10-16
     1 |   826     3380  2015-05-19
     2 |   901     4555  2014-11-01
    [3 rows x 3 columns]

.. note::

    If ``sort`` and ``by`` modifiers are present, the sorting occurs after
    the grouping, and occurs within each group.

Replicate ``SQL``'s ``HAVING`` clause. Task: Filter for groups where the
length/count is greater than 1::

    >>> df = dt.Frame([[1, 1, 5], [2, 3, 6]], names=['A', 'B'])
    >>> df
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     1      2
     1 |     1      3
     2 |     5      6
    [3 rows x 2 columns]

    >>> # Get the count of each group,
    ... # and assign to a new column, using the update method
    ... # note that the update operation is in-place;
    ... # there is no need to assign back to the dataframe
    >>> df[:, update(filter_col = count()), by('A')]
    >>>
    >>> # The new column will be added to the end
    ... # We use an f-expression to return rows
    ... # in each group where the count is greater than 1
    >>> df[f.filter_col > 1, f[:-1]]
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     1      2
     1 |     1      3
    [2 rows x 2 columns]

Keep only rows per group where ``diff`` is the minimum::

    >>> df = dt.Frame(""" item    diff   otherstuff
    ...                     1       2            1
    ...                     1       1            2
    ...                     1       3            7
    ...                     2      -1            0
    ...                     2       1            3
    ...                     2       4            9
    ...                     2      -6            2
    ...                     3       0            0
    ...                     3       2            9""")
    >>>
    >>> df[:,
    ...    #get boolean for rows where diff column is minimum for each group
    ...    update(filter_col = f.diff == min(f.diff)),
    ...    by('item')]
    >>>
    >>> df[f.filter_col == 1, :-1]
       |  item   diff  otherstuff
       | int32  int32       int32
    -- + -----  -----  ----------
     0 |     1      1           2
     1 |     2     -6           2
     2 |     3      0           0
    [3 rows x 3 columns]

Keep only entries where ``make`` has both 0 and 1 in ``sales``::

    >>> df  =  dt.Frame(""" make    country  other_columns   sale
    ...                     honda    tokyo       data          1
    ...                     honda    hirosima    data          0
    ...                     toyota   tokyo       data          1
    ...                     toyota   hirosima    data          0
    ...                     suzuki   tokyo       data          0
    ...                     suzuki   hirosima    data          0
    ...                     ferrari  tokyo       data          1
    ...                     ferrari  hirosima    data          0
    ...                     nissan   tokyo       data          1
    ...                     nissan   hirosima    data          0""")
    >>>
    >>> df[:,
    ...    update(filter_col = sum(f.sale)),
    ...    by('make')]
    >>>
    >>> df[f.filter_col == 1, :-1]
       | make     country   other_columns   sale
       | str32    str32     str32          bool8
    -- + -------  --------  -------------  -----
     0 | honda    tokyo     data               1
     1 | honda    hirosima  data               0
     2 | toyota   tokyo     data               1
     3 | toyota   hirosima  data               0
     4 | ferrari  tokyo     data               1
     5 | ferrari  hirosima  data               0
     6 | nissan   tokyo     data               1
     7 | nissan   hirosima  data               0
    [8 rows x 4 columns]


Transformation
--------------

This is when a function is applied to a column after a groupby and the resulting column is appended back to the dataframe.  The number of rows of the dataframe is unchanged. The :func:`update` method makes this possible and easy.

Get the minimum and maximum of column ``c`` per group, and append to dataframe::

    >>> df = dt.Frame(""" c     y
    ...                   9     0
    ...                   8     0
    ...                   3     1
    ...                   6     2
    ...                   1     3
    ...                   2     3
    ...                   5     3
    ...                   4     4
    ...                   0     4
    ...                   7     4""")
    >>>
    >>> # Assign the new columns via the update method
    >>> df[:,
    ...    update(min_col = min(f.c),
    ...           max_col = max(f.c)),
    ...   by('y')]
    >>> df
       |     c      y  min_col  max_col
       | int32  int32    int32    int32
    -- + -----  -----  -------  -------
     0 |     9      0        8        9
     1 |     8      0        8        9
     2 |     3      1        3        3
     3 |     6      2        6        6
     4 |     1      3        1        5
     5 |     2      3        1        5
     6 |     5      3        1        5
     7 |     4      4        0        7
     8 |     0      4        0        7
     9 |     7      4        0        7
    [10 rows x 4 columns]

Fill missing values by group mean::

    >>> df = dt.Frame({'value' : [1, None, None, 2, 3, 1, 3, None, 3],
    ...                'name' : ['A','A', 'B','B','B','B', 'C','C','C']})
    >>>
    >>> df
       |   value  name
       | float64  str32
    -- + -------  -----
     0 |       1  A
     1 |      NA  A
     2 |      NA  B
     3 |       2  B
     4 |       3  B
     5 |       1  B
     6 |       3  C
     7 |      NA  C
     8 |       3  C
    [9 rows x 2 columns]

    >>> # This uses a combination of update and ifelse methods:
    >>> df[:,
    ...    update(value = ifelse(f.value == None,
    ...                          mean(f.value),
    ...                          f.value)),
    ...    by('name')]
    >>>
    >>> df
       |   value  name
       | float64  str32
    -- + -------  -----
     0 |       1  A
     1 |       1  A
     2 |       2  B
     3 |       2  B
     4 |       3  B
     5 |       1  B
     6 |       3  C
     7 |       3  C
     8 |       3  C
    [9 rows x 2 columns]


Transform and Aggregate on multiple columns
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Task: Get the sum of the aggregate of column ``a`` and ``b``, grouped by ``c``
and ``d`` and append to dataframe::

    >>> df = dt.Frame({'a' : [1,2,3,4,5,6],
    ...                'b' : [1,2,3,4,5,6],
    ...                'c' : ['q', 'q', 'q', 'q', 'w', 'w'],
    ...                'd' : ['z','z','z','o','o','o']})
    >>> df
       |     a      b  c      d
       | int32  int32  str32  str32
    -- + -----  -----  -----  -----
     0 |     1      1  q      z
     1 |     2      2  q      z
     2 |     3      3  q      z
     3 |     4      4  q      o
     4 |     5      5  w      o
     5 |     6      6  w      o
    [6 rows x 4 columns]

    >>> df[:,
    ...    update(e = sum(f.a) + sum(f.b)),
    ...    by('c', 'd')
    ...    ]
    >>>
    >>> df
       |     a      b  c      d          e
       | int32  int32  str32  str32  int64
    -- + -----  -----  -----  -----  -----
     0 |     1      1  q      z         12
     1 |     2      2  q      z         12
     2 |     3      3  q      z         12
     3 |     4      4  q      o          8
     4 |     5      5  w      o         22
     5 |     6      6  w      o         22
    [6 rows x 5 columns]


Replicate R's groupby `mutate`_
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Task : Get ratio by dividing column ``c`` by the product of column ``c``
and ``d``, grouped by ``a`` and ``b``::

    >>> df = dt.Frame(dict(a = (1,1,0,1,0),
    ...                    b = (1,0,0,1,0),
    ...                    c = (10,5,1,5,10),
    ...                    d = (3,1,2,1,2))
    ...               )
    >>> df
       |    a     b      c      d
       | int8  int8  int32  int32
    -- + ----  ----  -----  -----
     0 |    1     1     10      3
     1 |    1     0      5      1
     2 |    0     0      1      2
     3 |    1     1      5      1
     4 |    0     0     10      2
    [5 rows x 4 columns]

    >>> df[:,
    ...    update(ratio = f.c / sum(f.c * f.d)),
    ...    by('a', 'b')
    ...    ]
    >>> df
       |    a     b      c      d      ratio
       | int8  int8  int32  int32    float64
    -- + ----  ----  -----  -----  ---------
     0 |    1     1     10      3  0.285714
     1 |    1     0      5      1  1
     2 |    0     0      1      2  0.0454545
     3 |    1     1      5      1  0.142857
     4 |    0     0     10      2  0.454545
    [5 rows x 5 columns]


Groupby on boolean expressions
-------------------------------

Conditional sum with groupby
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Task: sum ``data1`` column, grouped by ``key1`` and rows where
``key2 == "one"``::

    >>> df = dt.Frame("""data1        data2     key1  key2
    ...                  0.361601    0.375297     a    one
    ...                  0.069889    0.809772     a    two
    ...                  1.468194    0.272929     b    one
    ...                 -1.138458    0.865060     b    two
    ...                 -0.268210    1.250340     a    one""")
    >>>
    >>>
    >>> df[:,
    ...    sum(f.data1),
    ...    by(f.key2 == "one", f.key1)][f.C0 == 1, 1:]
       | key1      data1
       | str32   float64
    -- + -----  --------
     0 | a      0.093391
     1 | b      1.46819
    [2 rows x 2 columns]


Conditional sums based on various criteria
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    >>> df = dt.Frame(""" A_id       B       C
    ...                     a1      "up"     100
    ...                     a2     "down"    102
    ...                     a3      "up"     100
    ...                     a3      "up"     250
    ...                     a4     "left"    100
    ...                     a5     "right"   102""")
    >>>
    >>> df[:,
    ...    {"sum_up": sum(f.B == "up"),
    ...     "sum_down" : sum(f.B == "down"),
    ...     "over_200_up" : sum((f.B == "up") & (f.C > 200))
    ...     },
    ...    by('A_id')]
       | A_id   sum_up  sum_down  over_200_up
       | str32   int64     int64        int64
    -- + -----  ------  --------  -----------
     0 | a1          1         0            0
     1 | a2          0         1            0
     2 | a3          2         0            1
     3 | a4          0         0            0
     4 | a5          0         0            0
    [5 rows x 4 columns]


More Examples
-------------

Aggregation on values in a column
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Task: group by ``Day`` and find minimum ``Data_Value`` for elements of type
``TMIN`` and maximum ``Data_Value`` for elements of type ``TMAX``::

    >>> df = dt.Frame("""  Day    Element  Data_Value
    ...                   01-01   TMAX    112
    ...                   01-01   TMAX    101
    ...                   01-01   TMIN    60
    ...                   01-01   TMIN    0
    ...                   01-01   TMIN    25
    ...                   01-01   TMAX    113
    ...                   01-01   TMAX    115
    ...                   01-01   TMAX    105
    ...                   01-01   TMAX    111
    ...                   01-01   TMIN    44
    ...                   01-01   TMIN    83
    ...                   01-02   TMAX    70
    ...                   01-02   TMAX    79
    ...                   01-02   TMIN    0
    ...                   01-02   TMIN    60
    ...                   01-02   TMAX    73
    ...                   01-02   TMIN    31
    ...                   01-02   TMIN    26
    ...                   01-02   TMAX    71
    ...                   01-02   TMIN    26""")
    >>> df[:,
    ...    {"TMAX": max(ifelse(f.Element=="TMAX", f.Data_Value, None)),
    ...     "TMIN": min(ifelse(f.Element=="TMIN", f.Data_Value, None))},
    ...    by(f.Day)]
       | Day     TMAX   TMIN
       | str32  int32  int32
    -- + -----  -----  -----
     0 | 01-01    115      0
     1 | 01-02     79      0
    [2 rows x 3 columns]


Group-by and conditional sum and add back to data frame
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Task: sum the ``Count`` value for each ``ID``, when ``Num`` is (17 or 12) and
``Letter`` is ``'D'`` and then add the calculation back to the original data
frame as column ``'Total'``::

    >>> df = dt.Frame(""" ID  Num  Letter  Count
    ...                   1   17   D       1
    ...                   1   12   D       2
    ...                   1   13   D       3
    ...                   2   17   D       4
    ...                   2   12   A       5
    ...                   2   16   D       1
    ...                   3   16   D       1""")
    >>> expression = ((f.Num==17) | (f.Num==12)) & (f.Letter == "D")
    >>> df[:, update(Total = sum(expression * f.Count)),
    ...       by(f.ID)]
    >>> df
       |    ID    Num  Letter  Count  Total
       | int32  int32  str32   int32  int64
    -- + -----  -----  ------  -----  -----
     0 |     1     17  D           1      3
     1 |     1     12  D           2      3
     2 |     1     13  D           3      3
     3 |     2     17  D           4      4
     4 |     2     12  A           5      4
     5 |     2     16  D           1      4
     6 |     3     16  D           1      0
    [7 rows x 5 columns]


Indexing with multiple min and max in one aggregate
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Task : find ``col1`` where ``col2`` is max, ``col2`` where ``col3`` is min
and ``col1`` where ``col3`` is max::

    >>> df = dt.Frame({
    ...                "id" : [1, 1, 1, 2, 2, 2, 2, 3, 3, 3],
    ...                "col1" : [1, 3, 5, 2, 5, 3, 6, 3, 67, 7],
    ...                "col2" : [4, 6, 8, 3, 65, 3, 5, 4, 4, 7],
    ...                "col3" : [34, 64, 53, 5, 6, 2, 4, 6, 4, 67],
    ...                })
    >>>
    >>> df
       |    id   col1   col2   col3
       | int32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 |     1      1      4     34
     1 |     1      3      6     64
     2 |     1      5      8     53
     3 |     2      2      3      5
     4 |     2      5     65      6
     5 |     2      3      3      2
     6 |     2      6      5      4
     7 |     3      3      4      6
     8 |     3     67      4      4
     9 |     3      7      7     67
    [10 rows x 4 columns]

    >>> df[:,
    ...    {'col1' : max(ifelse(f.col2 == max(f.col2), f.col1, None)),
    ...     'col2' : max(ifelse(f.col3 == min(f.col3), f.col2, None)),
    ...     'col3' : max(ifelse(f.col3 == max(f.col3), f.col1, None))
    ...     },
    ...    by('id')]
       |    id   col1   col2   col3
       | int32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 |     1      5      4      3
     1 |     2      5      3      5
     2 |     3      7      4      7
    [3 rows x 4 columns]


Filter rows based on aggregate value
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Task: for every ``word`` find the ``tag`` that has the most ``count``::

    >>> df = dt.Frame("""word  tag count
    ...                   a     S    30
    ...                   the   S    20
    ...                   a     T    60
    ...                   an    T    5
    ...                   the   T    10""")
    >>>
    >>> # The solution builds on the knowledge that sorting
    ... # while grouping sorts within each group.
    ... df[0, :, by('word'), sort(-f.count)]
       | word   tag    count
       | str32  str32  int32
    -- + -----  -----  -----
     0 | a      T         60
     1 | an     T          5
     2 | the    S         20
    [3 rows x 3 columns]


Get the rows where the ``value`` column is minimum, and rename columns::

    >>> df = dt.Frame({"category": ["A"]*3 + ["B"]*3,
    ...                "date": ["9/6/2016", "10/6/2016",
    ...                         "11/6/2016", "9/7/2016",
    ...                         "10/7/2016", "11/7/2016"],
    ...                "value": [7,8,9,10,1,2]})
    >>>
    >>> df
       | category  date       value
       | str32     str32      int32
    -- + --------  ---------  -----
     0 | A         9/6/2016       7
     1 | A         10/6/2016      8
     2 | A         11/6/2016      9
     3 | B         9/7/2016      10
     4 | B         10/7/2016      1
     5 | B         11/7/2016      2
    [6 rows x 3 columns]

    >>> df[0,
    ...    {"value_date": f.date,
    ...     "value_min":  f.value},
    ...   by("category"),
    ...   sort('value')]
       | category  value_date  value_min
       | str32     str32           int32
    -- + --------  ----------  ---------
     0 | A         9/6/2016            7
     1 | B         10/7/2016           1
    [2 rows x 3 columns]


Get the rows where the ``value`` column is maximum, and rename columns::

    >>> df[0,
    ...    {"value_date": f.date,
    ...     "value_max":  f.value},
    ...   by("category"),
    ...   sort(-f.value)]
       | category  value_date  value_max
       | str32     str32           int32
    -- + --------  ----------  ---------
     0 | A         11/6/2016           9
     1 | B         9/7/2016           10
    [2 rows x 3 columns]


Get the average of the last three instances per group::

    >>> import random
    >>> random.seed(3)
    >>>
    >>> df = dt.Frame({"Student": ["Bob", "Bill",
    ...                            "Bob", "Bob",
    ...                            "Bill","Joe",
    ...                            "Joe", "Bill",
    ...                            "Bob", "Joe",],
    ...                "Score": random.sample(range(10,30), 10)})
    >>>
    >>> df
       | Student  Score
       | str32    int32
    -- + -------  -----
     0 | Bob         17
     1 | Bill        28
     2 | Bob         27
     3 | Bob         14
     4 | Bill        21
     5 | Joe         24
     6 | Joe         19
     7 | Bill        29
     8 | Bob         20
     9 | Joe         23
    [10 rows x 2 columns]

    >>> df[-3:, mean(f[:]), f.Student]
       | Student    Score
       | str32    float64
    -- + -------  -------
     0 | Bill     26
     1 | Bob      20.3333
     2 | Joe      22
    [3 rows x 2 columns]


Group by on a condition
~~~~~~~~~~~~~~~~~~~~~~~

Get the sum of ``Amount`` for ``Number`` in range (1 to 4) and (5 and above)::

    >>> df = dt.Frame("""Number, Amount
    ...                     1,     5
    ...                     2,     10
    ...                     3,     11
    ...                     4,     3
    ...                     5,     5
    ...                     6,     8
    ...                     7,     9
    ...                     8,     6""")
    >>>
    >>> df[:, sum(f.Amount), by(ifelse(f.Number>=5, "B", "A"))]
       | C0     Amount
       | str32   int64
    -- + -----  ------
     0 | A          29
     1 | B          28
    [2 rows x 2 columns]



.. _`split-apply-combine`: https://www.jstatsoft.org/article/view/v040i01
.. _`mutate`: https://dplyr.tidyverse.org/reference/mutate.html
