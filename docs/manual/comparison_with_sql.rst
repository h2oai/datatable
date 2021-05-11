
Comparison with SQL
===================

This page provides some examples of how various `SQL`_ operations can be
performed in ``datatable``. The ``datatable`` library is still growing; as such,
not all functions in ``SQL`` can be replicated yet. If there is a feature you
would love to have in ``datatable``, please make a feature request on the
`github issues`_ page.

Most of the examples will be based on the famous `iris`_ dataset. `SQLite`_
will be the flavour of SQL used in the comparison.

Let's import ``datatable`` and read in the data using its :func:`fread()`
function::

    >>> from datatable import dt, f, g, by, join, sort, update, fread
    >>>
    >>> iris = fread('https://raw.githubusercontent.com/h2oai/datatable/main/docs/_static/iris.csv')
    >>> iris
        | sepal_length  sepal_width  petal_length  petal_width  species
        |      float64      float64       float64      float64  str32
    --- + ------------  -----------  ------------  -----------  ---------
      0 |          5.1          3.5           1.4          0.2  setosa
      1 |          4.9          3             1.4          0.2  setosa
      2 |          4.7          3.2           1.3          0.2  setosa
      3 |          4.6          3.1           1.5          0.2  setosa
      4 |          5            3.6           1.4          0.2  setosa
      5 |          5.4          3.9           1.7          0.4  setosa
      6 |          4.6          3.4           1.4          0.3  setosa
      7 |          5            3.4           1.5          0.2  setosa
      8 |          4.4          2.9           1.4          0.2  setosa
      9 |          4.9          3.1           1.5          0.1  setosa
     10 |          5.4          3.7           1.5          0.2  setosa
     11 |          4.8          3.4           1.6          0.2  setosa
     12 |          4.8          3             1.4          0.1  setosa
     13 |          4.3          3             1.1          0.1  setosa
     14 |          5.8          4             1.2          0.2  setosa
      … |            …            …             …            …  …
    145 |          6.7          3             5.2          2.3  virginica
    146 |          6.3          2.5           5            1.9  virginica
    147 |          6.5          3             5.2          2    virginica
    148 |          6.2          3.4           5.4          2.3  virginica
    149 |          5.9          3             5.1          1.8  virginica
    [150 rows x 5 columns]

Loading data into an SQL table is a bit more involved, where you need to create
the structure of the table (a schema), before importing the csv file. Have a
look at `SQLite import tutorial`_ for an example on loading data into a
`SQLite`_ datatabase.



SELECT
------

In ``SQL``, you can select a subset of the columns with the ``SELECT`` clause:

.. code-block:: SQL

    SELECT sepal_length,
           sepal_width,
           petal_length
    FROM iris
    LIMIT 5;

In ``datatable``, columns are selected in the ``j`` section::

    >>> iris[:5, ['sepal_length', 'sepal_width', 'petal_length']]
       | sepal_length  sepal_width  petal_length
       |      float64      float64       float64
    -- + ------------  -----------  ------------
     0 |          5.1          3.5           1.4
     1 |          4.9          3             1.4
     2 |          4.7          3.2           1.3
     3 |          4.6          3.1           1.5
     4 |          5            3.6           1.4
    [5 rows x 3 columns]

In ``SQL``, you can select all columns with the ``*`` symbol:

.. code-block:: SQL

    SELECT *
    FROM iris
    LIMIT 5;

In ``datatable``, all columns can be selected with a simple "select-all" slice
``:``, or with :ref:`f-expressions`::

    >>> iris[:5, :]
       | sepal_length  sepal_width  petal_length  petal_width  species
       |      float64      float64       float64      float64  str32
    -- + ------------  -----------  ------------  -----------  -------
     0 |          5.1          3.5           1.4          0.2  setosa
     1 |          4.9          3             1.4          0.2  setosa
     2 |          4.7          3.2           1.3          0.2  setosa
     3 |          4.6          3.1           1.5          0.2  setosa
     4 |          5            3.6           1.4          0.2  setosa
    [5 rows x 5 columns]

If you are selecting a single column, ``datatable`` allows you to access just
the ``j`` section within the square brackets; you do not need to include the
``i`` section: ``DT[j]``

.. code-block:: SQL

    SELECT sepal_length
    FROM iris
    LIMIT 5;

.. code-block:: python

    >>> # datatable
    >>> iris['sepal_length'].head(5)
       | sepal_length
       |      float64
    -- + ------------
     0 |          5.1
     1 |          4.9
     2 |          4.7
     3 |          4.6
     4 |          5
    [5 rows x 1 column]

How about adding new columns? In ``SQL``, this is done also in the ``SELECT``
clause:

.. code-block:: SQL

    SELECT *,
           sepal_length*2 AS sepal_length_doubled
    FROM iris
    LIMIT 5;

In ``datatable``, addition of new columns occurs in the ``j`` section::

    >>> iris[:5,
    ...      f[:].extend({"sepal_length_doubled": f.sepal_length * 2})]
       | sepal_length  sepal_width  petal_length  petal_width  species  sepal_length_doubled
       |      float64      float64       float64      float64  str32                 float64
    -- + ------------  -----------  ------------  -----------  -------  --------------------
     0 |          5.1          3.5           1.4          0.2  setosa                   10.2
     1 |          4.9          3             1.4          0.2  setosa                    9.8
     2 |          4.7          3.2           1.3          0.2  setosa                    9.4
     3 |          4.6          3.1           1.5          0.2  setosa                    9.2
     4 |          5            3.6           1.4          0.2  setosa                   10
    [5 rows x 6 columns]

The :func:`update` function can also be used to add new columns. The operation
occurs in-place; reassignment is not required::

    >>> iris[:, update(sepal_length_doubled = f.sepal_length * 2)]
    >>> iris[:5, :]
       | sepal_length  sepal_width  petal_length  petal_width  species  sepal_length_doubled
       |      float64      float64       float64      float64  str32                 float64
    -- + ------------  -----------  ------------  -----------  -------  --------------------
     0 |          5.1          3.5           1.4          0.2  setosa                   10.2
     1 |          4.9          3             1.4          0.2  setosa                    9.8
     2 |          4.7          3.2           1.3          0.2  setosa                    9.4
     3 |          4.6          3.1           1.5          0.2  setosa                    9.2
     4 |          5            3.6           1.4          0.2  setosa                   10
    [5 rows x 6 columns]



WHERE
-----

Filtering in ``SQL`` is done via the ``WHERE`` clause.

.. code-block:: SQL

    SELECT *
    FROM iris
    WHERE species = 'virginica'
    LIMIT 5;

In ``datatable``, filtration is done in the ``i`` section::

    >>> iris[f.species=="virginica", :].head(5)
       | sepal_length  sepal_width  petal_length  petal_width  species    sepal_length_doubled
       |      float64      float64       float64      float64  str32                   float64
    -- + ------------  -----------  ------------  -----------  ---------  --------------------
     0 |          6.3          3.3           6            2.5  virginica                  12.6
     1 |          5.8          2.7           5.1          1.9  virginica                  11.6
     2 |          7.1          3             5.9          2.1  virginica                  14.2
     3 |          6.3          2.9           5.6          1.8  virginica                  12.6
     4 |          6.5          3             5.8          2.2  virginica                  13
    [5 rows x 6 columns]

Note that in ``SQL``, equality comparison is done with the ``=`` symbol,
whereas in ``python``, it is with the ``==`` operator. You can filter with
multple conditions too:

.. code-block:: SQL

    SELECT *
    FROM iris
    WHERE species = 'setosa' AND sepal_length = 5;

In ``datatable`` each condition is wrapped in parentheses; the ``&`` operator
is the equivalent of ``AND``, while ``|`` is the equivalent of ``OR``::

    >>> iris[(f.species=="setosa") & (f.sepal_length==5), :]
       | sepal_length  sepal_width  petal_length  petal_width  species  sepal_length_doubled
       |      float64      float64       float64      float64  str32                 float64
    -- + ------------  -----------  ------------  -----------  -------  --------------------
     0 |            5          3.6           1.4          0.2  setosa                     10
     1 |            5          3.4           1.5          0.2  setosa                     10
     2 |            5          3             1.6          0.2  setosa                     10
     3 |            5          3.4           1.6          0.4  setosa                     10
     4 |            5          3.2           1.2          0.2  setosa                     10
     5 |            5          3.5           1.3          0.3  setosa                     10
     6 |            5          3.5           1.6          0.6  setosa                     10
     7 |            5          3.3           1.4          0.2  setosa                     10
    [8 rows x 6 columns]

Now suppose you have a frame where some values are missing (NA)::

    >>> null_data = dt.Frame(""" a    b    c
    ...                          1    2    3
    ...                          1    NaN  4
    ...                          2    1    3
    ...                          1    2    2""")
    >>> null_data
       |     a        b      c
       | int32  float64  int32
    -- + -----  -------  -----
     0 |     1        2      3
     1 |     1       NA      4
     2 |     2        1      3
     3 |     1        2      2
    [4 rows x 3 columns]

In SQL you could filter out those values like this:

.. code-block:: SQL

    SELECT *
    FROM null_data
    WHERE b is NOT NULL;

In ``datatable``, the ``NOT`` operator is replicated with the ``!=`` symbol::

    >>> null_data[f.b!=None, :]
       |     a        b      c
       | int32  float64  int32
    -- + -----  -------  -----
     0 |     1        2      3
     1 |     2        1      3
     2 |     1        2      2
    [3 rows x 3 columns]

You could also use :func:`isna <dt.math.isna>` function with the ``~`` operator
which inverts boolean expressions::

    >>> null_data[~dt.math.isna(f.b), :]
       |     a        b      c
       | int32  float64  int32
    -- + -----  -------  -----
     0 |     1        2      3
     1 |     2        1      3
     2 |     1        2      2
    [3 rows x 3 columns]

Keeping the null rows is easily achievable; it is simply the inverse of the above code:

.. code-block:: SQL

    SELECT *
    FROM null_data
    WHERE b is NULL;

.. code-block:: python

    >>> null_data[dt.isna(f.b), :]
       |     a        b      c
       | int32  float64  int32
    -- + -----  -------  -----
     0 |     1       NA      4
    [1 row x 3 columns]

    null_data[dt.isna(f.b), :]

.. note::

    ``SQL`` has the ``IN`` operator, which does not have an equivalent in
    ``datatable`` yet.



ORDER BY
--------

In SQL, sorting is executed with the ``ORDER BY`` clause, while in ``datatable``
it is handled by the :func:`sort()` function.

.. code-block:: SQL

    SELECT *
    FROM iris
    ORDER BY sepal_length ASC
    limit 5;

.. code-block:: python

    >>> iris[:5, :, sort('sepal_length')]
       | sepal_length  sepal_width  petal_length  petal_width  species  sepal_length_doubled
       |      float64      float64       float64      float64  str32                 float64
    -- + ------------  -----------  ------------  -----------  -------  --------------------
     0 |          4.3          3             1.1          0.1  setosa                    8.6
     1 |          4.4          2.9           1.4          0.2  setosa                    8.8
     2 |          4.4          3             1.3          0.2  setosa                    8.8
     3 |          4.4          3.2           1.3          0.2  setosa                    8.8
     4 |          4.5          2.3           1.3          0.3  setosa                    9
    [5 rows x 6 columns]

Sorting in descending order in SQL is with the ``DESC``.

.. code-block:: SQL

    SELECT *
    FROM iris
    ORDER BY sepal_length DESC
    limit 5;

In datatable, this can be achieved in two ways::

    >>> iris[:5, :, sort('sepal_length', reverse=True)]
       | sepal_length  sepal_width  petal_length  petal_width  species    sepal_length_doubled
       |      float64      float64       float64      float64  str32                   float64
    -- + ------------  -----------  ------------  -----------  ---------  --------------------
     0 |          7.9          3.8           6.4          2    virginica                  15.8
     1 |          7.7          3.8           6.7          2.2  virginica                  15.4
     2 |          7.7          2.6           6.9          2.3  virginica                  15.4
     3 |          7.7          2.8           6.7          2    virginica                  15.4
     4 |          7.7          3             6.1          2.3  virginica                  15.4
    [5 rows x 6 columns]

or, you could negate the sorting column; datatable will correctly interprete the
negation(``-``) as descending order::

    >>> iris[:5, :, sort(-f.sepal_length)]
       | sepal_length  sepal_width  petal_length  petal_width  species    sepal_length_doubled
       |      float64      float64       float64      float64  str32                   float64
    -- + ------------  -----------  ------------  -----------  ---------  --------------------
     0 |          7.9          3.8           6.4          2    virginica                  15.8
     1 |          7.7          3.8           6.7          2.2  virginica                  15.4
     2 |          7.7          2.6           6.9          2.3  virginica                  15.4
     3 |          7.7          2.8           6.7          2    virginica                  15.4
     4 |          7.7          3             6.1          2.3  virginica                  15.4
    [5 rows x 6 columns]



GROUP BY
--------

SQL's ``GROUP BY`` operations can be performed in ``datatable`` with the
:func:`by()` function.  Have a look at the :func:`by()` API, as well as the
:ref:`Grouping with by` user guide.

Let's look at some common grouping operations in ``SQL``, and their equivalents
in ``datatable``.

Single aggregation per group
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: SQL

    SELECT species,
           COUNT() AS N
    FROM iris
    GROUP BY species;


.. code-block:: python

    >>> iris[:, dt.count(), by('species')]
       | species     count
       | str32       int64
    -- + ----------  -----
     0 | setosa         50
     1 | versicolor     50
     2 | virginica      50
    [3 rows x 2 columns]


Multiple aggregations per group
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: SQL

    SELECT species,
           COUNT() AS N,
           AVG(sepal_length) AS mean_sepal_length
    FROM iris
    GROUP BY species;


.. code-block:: python

    >>> iris[:,
    ...     {"mean_sepal_length": dt.mean(f.sepal_length),
    ...     "N": dt.count()},
    ...     by('species')]
       | species     mean_sepal_length      N
       | str32                 float64  int64
    -- + ----------  -----------------  -----
     0 | setosa                  5.006     50
     1 | versicolor              5.936     50
     2 | virginica               6.588     50
    [3 rows x 3 columns]


Grouping on multiple columns
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

    >>> fruits_data
       | Fruit    Date       Name   Number
       | str32    str32      str32   int32
    -- + -------  ---------  -----  ------
     0 | Apples   10/6/2016  Bob         7
     1 | Apples   10/6/2016  Bob         8
     2 | Apples   10/6/2016  Mike        9
     3 | Apples   10/7/2016  Steve      10
     4 | Apples   10/7/2016  Bob         1
     5 | Oranges  10/7/2016  Bob         2
     6 | Oranges  10/6/2016  Tom        15
     7 | Oranges  10/6/2016  Mike       57
     8 | Oranges  10/6/2016  Bob        65
     9 | Oranges  10/7/2016  Tony        1
    10 | Grapes   10/7/2016  Bob         1
    11 | Grapes   10/7/2016  Tom        87
    12 | Grapes   10/7/2016  Bob        22
    13 | Grapes   10/7/2016  Bob        12
    14 | Grapes   10/7/2016  Tony       15
    [15 rows x 4 columns]

.. code-block:: SQL

    SELECT fruit,
           name,
           SUM(number) AS sum_num
    FROM fruits_data
    GROUP BY fruit, name;

.. code-block:: python

    >>> fruits_data[:,
    ...             {"sum_num": dt.sum(f.Number)},
    ...             by('Fruit', 'Name')]
       | Fruit    Name   sum_num
       | str32    str32    int64
    -- + -------  -----  -------
     0 | Apples   Bob         16
     1 | Apples   Mike         9
     2 | Apples   Steve       10
     3 | Grapes   Bob         35
     4 | Grapes   Tom         87
     5 | Grapes   Tony        15
     6 | Oranges  Bob         67
     7 | Oranges  Mike        57
     8 | Oranges  Tom         15
     9 | Oranges  Tony         1
    [10 rows x 3 columns]


WHERE with GROUP BY
~~~~~~~~~~~~~~~~~~~

.. code-block:: SQL

    SELECT species,
           AVG(sepal_length) AS avg_sepal_length
    FROM iris
    WHERE sepal_width > 3
    GROUP BY species;

.. code-block:: python

    >>> iris[f.sepal_width >=3, :][:,
    ...                           {"avg_sepal_length": dt.mean(f.sepal_length)},
    ...                           by('species')]
       | species     avg_sepal_length
       | str32                float64
    -- + ----------  ----------------
     0 | setosa               5.02917
     1 | versicolor           6.21875
     2 | virginica            6.76897
    [3 rows x 2 columns]


HAVING with GROUP BY
~~~~~~~~~~~~~~~~~~~~

.. code-block:: SQL

    SELECT fruit,
           name,
           SUM(number) AS sum_num
    FROM fruits_data
    GROUP BY fruit, name
    HAVING sum_num > 50;


.. code-block:: python

    >>> fruits_data[:,
    ...            {'sum_num': dt.sum(f.Number)},
    ...            by('Fruit','Name')][f.sum_num > 50, :]
       | Fruit    Name   sum_num
       | str32    str32    int64
    -- + -------  -----  -------
     0 | Grapes   Tom         87
     1 | Oranges  Bob         67
     2 | Oranges  Mike        57
    [3 rows x 3 columns]


Grouping on a condition
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: SQL

    SELECT sepal_width >=3 AS width_larger_than_3,
           AVG(sepal_length) AS avg_sepal_length
    FROM iris
    GROUP BY sepal_width>=3;

.. code-block:: python

    >>> iris[:,
    ...      {"avg_sepal_length": dt.mean(f.sepal_length)},
    ...      by(f.sepal_width >= 3)]
       |    C0  avg_sepal_length
       | bool8           float64
    -- + -----  ----------------
     0 |     0           5.95263
     1 |     1           5.77634
    [2 rows x 2 columns]

At the moment, names cannot be assigned in the ``by`` section.


LEFT OUTER JOIN
----------------

We will compare the left outer join, as that is the only join currently
implemented in ``datatable``. Another aspect is that the frame being joined
must be keyed, the column or columns to be keyed must not have duplicates,
and the joining column has to have the same name in both frames. You can read
more about the :func:`join()` API and have a look at the :ref:`join tutorial`.

Example data::

    >>> DT = dt.Frame(x = ["b"]*3 + ["a"]*3 + ["c"]*3,
    ...               y = [1, 3, 6] * 3,
    ...               v = range(1, 10))
    >>>
    >>> X = dt.Frame({"x":('c','b'),
    ...               "v":(8,7),
    ...               "foo":(4,2)})

A left outer join in SQL:

.. code-block:: SQL

    SELECT DT.x,
           DT.y,
           DT.v,
           X.foo
    FROM DT
    left JOIN X
    ON DT.x = X.x

A left outer join in ``datatable``::

    >>> X.key = 'x'
    >>> DT[:, [f.x, f.y, f.v, g.foo], join(X)]
       | x          y      v    foo
       | str32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 | b          1      1      2
     1 | b          3      2      2
     2 | b          6      3      2
     3 | a          1      4     NA
     4 | a          3      5     NA
     5 | a          6      6     NA
     6 | c          1      7      4
     7 | c          3      8      4
     8 | c          6      9      4
    [9 rows x 4 columns]


UNION
-----

The ``UNION ALL`` clause in SQL can be replicated in ``datatable`` with
:func:`rbind()`.

.. code-block:: SQL

    SELECT x, v
    FROM DT
    UNION ALL
    SELECT x, v
    FROM x

In ``datatable``, :func:`rbind()` takes a list/tuple of frames and lumps into one::

    >>> dt.rbind([DT[:, ('x','v')], X[:, ('x', 'v')]])
       | x          v
       | str32  int32
    -- + -----  -----
     0 | b          1
     1 | b          2
     2 | b          3
     3 | a          4
     4 | a          5
     5 | a          6
     6 | c          7
     7 | c          8
     8 | c          9
     9 | b          7
    10 | c          8
    [11 rows x 2 columns]

SQL's ``UNION`` removes duplicate rows after combining the results of the
individual queries; there is no built-in function in ``datatable`` yet that
handles duplicates.



SQL's WINDOW functions
----------------------

Some SQL window functions can be replicated in ``datatable`` (`rank` is one of the windows function not currently implemented in datatable) :

- TOP n rows per group

.. code-block:: SQL

    SELECT * from
    (SELECT *,
           ROW_NUMBER() OVER(PARTITION BY species ORDER BY sepal_length DESC) AS row_num
     FROM iris)
    WHERE row_num < 3;

.. code-block:: python

    >>> iris[:3, :, by('species'), sort(-f.sepal_length)]
       | species     sepal_length  sepal_width  petal_length  petal_width
       | str32            float64      float64       float64      float64
    -- + ----------  ------------  -----------  ------------  -----------
     0 | setosa               5.8          4             1.2          0.2
     1 | setosa               5.7          4.4           1.5          0.4
     2 | setosa               5.7          3.8           1.7          0.3
     3 | versicolor           7            3.2           4.7          1.4
     4 | versicolor           6.9          3.1           4.9          1.5
     5 | versicolor           6.8          2.8           4.8          1.4
     6 | virginica            7.9          3.8           6.4          2
     7 | virginica            7.7          3.8           6.7          2.2
     8 | virginica            7.7          2.6           6.9          2.3
    [9 rows x 5 columns]

Filter for rows above the mean sepal length:

.. code-block:: SQL

    SELECT sepal_length,
           sepal_width,
           petal_length,
           petal_width,
           species
    FROM
    (SELECT *,
    AVG(sepal_length) OVER (PARTITION BY species) AS avg_sepal_length
    FROM iris)
    WHERE sepal_length > avg_sepal_length
    LIMIT 5;

.. code-block:: python

    >>> iris[:,
    ...      update(temp = f.sepal_length > dt.mean(f.sepal_length)),
    ...      by('species')]
    >>>
    >>> iris[f.temp == 1, f[:-1]].head(5)
       | sepal_length  sepal_width  petal_length  petal_width  species
       |      float64      float64       float64      float64  str32
    -- + ------------  -----------  ------------  -----------  -------
     0 |          5.1          3.5           1.4          0.2  setosa
     1 |          5.4          3.9           1.7          0.4  setosa
     2 |          5.4          3.7           1.5          0.2  setosa
     3 |          5.8          4             1.2          0.2  setosa
     4 |          5.7          4.4           1.5          0.4  setosa
    [5 rows x 5 columns]

Lead and lag

.. code-block:: SQL

    SELECT name,
           destination,
           dep_date,
           LEAD(dep_date) OVER (ORDER BY dep_date, name) AS lead1,
           LEAD(dep_date, 2) OVER (ORDER BY dep_date, name) AS lead2,
           LAG(dep_date) OVER (ORDER BY dep_date, name) AS lag1,
           LAG(dep_date, 3) OVER (ORDER BY dep_date, name) AS lag3
    FROM source_data;

.. code-block:: python

    >>> source_data = dt.Frame({'name': ['Ann', 'Ann', 'Ann', 'Bob', 'Bob'],
    ...                         'destination': ['Japan', 'Korea', 'Switzerland',
    ...                                         'USA', 'Switzerland'],
    ...                         'dep_date': ['2019-02-02', '2019-01-01',
    ...                                      '2020-01-11', '2019-05-05',
    ...                                      '2020-01-11'],
    ...                         'duration': [7, 21, 14, 10, 14]})
    >>> source_data[:,
    ...             f[:].extend({"lead1": dt.shift(f.dep_date, -1),
    ...                          "lead2": dt.shift(f.dep_date, -2),
    ...                          "lag1": dt.shift(f.dep_date),
    ...                          "lag3": dt.shift(f.dep_date,3)
    ...                          }),
    ...             sort('dep_date','name')]
       | name   destination  dep_date    duration  lead1       lead2       lag1        lag3
       | str32  str32        str32          int32  str32       str32       str32       str32
    -- + -----  -----------  ----------  --------  ----------  ----------  ----------  ----------
     0 | Ann    Korea        2019-01-01        21  2019-02-02  2019-05-05  NA          NA
     1 | Ann    Japan        2019-02-02         7  2019-05-05  2020-01-11  2019-01-01  NA
     2 | Bob    USA          2019-05-05        10  2020-01-11  2020-01-11  2019-02-02  NA
     3 | Ann    Switzerland  2020-01-11        14  2020-01-11  NA          2019-05-05  2019-01-01
     4 | Bob    Switzerland  2020-01-11        14  NA          NA          2020-01-11  2019-02-02
    [5 rows x 8 columns]


The equivalent of SQL's ``LAG`` is :func:`shift()` with a positive number,
while SQL's ``LEAD`` is :func:`shift()` with a negative number.

.. note::

    ``datatable`` does not natively support datetimes yet.

Total sum and the proportions::

    >>> proportions = dt.Frame({"t": [1, 2, 3]})
    >>> proportions
       |     t
       | int32
    -- + -----
     0 |     1
     1 |     2
     2 |     3
    [3 rows x 1 column]

.. code-block:: SQL

    SELECT  t,
            SUM(t) OVER () AS sum,
            CAST(t as FLOAT)/SUM(t) OVER () AS pct
    FROM proportions;

.. code-block:: python

    >>> proportions[:,
    ...             f[:].extend({"sum": dt.sum(f.t),
    ...                         "pct": f.t/dt.sum(f.t)})]
       |     t    sum       pct
       | int32  int64   float64
    -- + -----  -----  --------
     0 |     1      6  0.166667
     1 |     2      6  0.333333
     2 |     3      6  0.5
    [3 rows x 3 columns]



.. _`SQL` : https://en.wikipedia.org/wiki/SQL
.. _`SQLite` : https://www.sqlite.org/
.. _`SQLite import tutorial` : https://www.sqlitetutorial.net/sqlite-import-csv/
.. _`github issues` : https://github.com/h2oai/datatable/issues
.. _`iris` : https://en.wikipedia.org/wiki/Iris_flower_data_set
