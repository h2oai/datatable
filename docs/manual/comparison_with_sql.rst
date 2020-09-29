.. py:currentmodule:: datatable

Comparison with SQL
====================

This page provides some examples of how various `SQL <https://en.wikipedia.org/wiki/SQL>`__ operations can be performed in ``datatable``. The ``datatable`` library is still growing; as such, not all functions in ``SQL`` can be replicated yet. If there is a feature you would love to have in ``datatable``, please make a feature request on the `github issues <https://github.com/h2oai/datatable/issues>`__ page.

Most of the examples will be based on the famous `iris <https://en.wikipedia.org/wiki/Iris_flower_data_set>`__ dataset. `SQLite <https://www.sqlite.org/>`__ will be the flavour of SQL used in the comparison.

Let's import ``datatable`` and read in the data:

.. code-block:: python

    from datatable import dt, f, g, by, join, sort, update, fread

    url = 'https://gist.github.com/curran/a08a1080b88344b0c8a7/raw/639388c2cbc2120a14dcf466e85730eb8be498bb/iris.csv'
    iris = fread(url)

    iris.head(5)


.. csv-table:: iris dataset
    :header: "sepal\\_length", "sepal\\_width", "petal\\_length", "petal\\_width",	"species"
    :widths: 10,10,10,10,20

    5.1,	3.5,	1.4,	0.2,	setosa
    	4.9,	3,	1.4,	0.2,	setosa
    	4.7,	3.2,	1.3,	0.2,	setosa
    	4.6,	3.1,	1.5,	0.2,	setosa
    	5,	3.6,	1.4,	0.2,	setosa

Loading data into an SQL table is a bit more involved, where you need to create the structure of the table (a schema), before importing the csv file. Have a look `here <https://www.sqlitetutorial.net/sqlite-import-csv/>`__ for an example on loading data into an `SQLite <https://www.sqlite.org/>`__ datatabase. The :func:`fread()` function makes it quite easy to read delimited files.

We'll assume that the data has been loaded into a database.



SELECT
-------

In ``SQL``, you can select a subset of the columns with the ``SELECT`` clause:

.. code-block:: SQL

    SELECT sepal_length,
           sepal_width,
           petal_length
    FROM iris
    LIMIT 5;

In ``datatable``, columns are selected in the ``j`` section :

.. code-block:: python

    iris[:5, ('sepal_length', 'sepal_width', 'petal_length')]


.. csv-table:: select columns
    :header: "sepal\\_length", "sepal\\_width", "petal\\_length"
    :widths: 10,10,10

    5.1,	3.5,	1.4
    4.9,	3,	1.4
	4.7,	3.2,	1.3
	4.6,	3.1,	1.5
	5,	3.6,	1.4

In ``SQL``, you can select all columns with the ``*`` symbol :

.. code-block:: SQL

    SELECT *
    FROM iris
    LIMIT 5;

In ``datatable``, all columns can be selected with the :ref:`f-expressions` :

.. code-block:: python

    iris[:5, f[:]]

.. csv-table:: select all columns
    :header: "sepal\\_length", "sepal\\_width", "petal\\_length", "petal\\_width",	"species"
    :widths: 10,10,10,10,10

    5.1,	3.5,	1.4,	0.2,	setosa
    	4.9,	3,	1.4,	0.2,	setosa
    	4.7,	3.2,	1.3,	0.2,	setosa
    	4.6,	3.1,	1.5,	0.2,	setosa
    	5,	3.6,	1.4,	0.2,	setosa

If you are selecting a single column, ``datatable`` allows you to access just the ``j`` section within the square brackets; you do not need to include the ``i`` section --> ``DT[j]``

.. code-block:: SQL

    SELECT sepal_length
    FROM iris
    LIMIT 5;

.. code-block:: python

    # datatable
    iris['species'].head(5)

.. csv-table:: 
    :header: sepal\\_length
    :widths: 20

	5.1
	4.9
	4.7
	4.6
	5




How about adding new columns? In ``SQL``, this is done also in the ``SELECT`` clause :

.. code-block:: SQL

    SELECT *,
          sepal_length*2 as sepal_length_doubled
    FROM iris
    LIMIT 5;

In ``datatable``, addition of new columns occurs in the ``j`` column :

.. code-block:: python

    iris[:5,
         f[:].extend({"sepal_length_doubled": f.sepal_length * 2})]

The :func:`update` option can also be used to add new columns. The operation occurs in-place; reassignment is not required.

.. code-block:: python

    iris[:, update(sepal_length_doubled = f.sepal_length * 2)]

    iris[:5, :]

.. csv-table:: Add a new column
    :header:    sepal\\_length,	sepal\\_width,	petal\\_length,	petal\\_width,	species,	sepal\\_length\\_doubled
    :widths: 10,10,10,10,10,10

    	5.1,	3.5,1.4,	0.2,	setosa,	10.2
	    4.9,	3,	1.4,	0.2,	setosa,	9.8
	    4.7	,3.2,	1.3,	0.2,	setosa,	9.4
	    4.6,	3.1,	1.5,	0.2,	setosa,	9.2
	    5,	3.6,	1.4,	0.2,	setosa,	10


FILTER
-------

Filtering in ``SQL`` is done via the ``WHERE`` clause.

.. code-block:: SQL

    SELECT *
    FROM iris
    WHERE species = 'virginica'
    LIMIT 5;

In ``datatable``, filtration is done in the ``i`` section :

.. code-block:: python

    iris[f.species=="virginica", :].head(5)

.. csv-table:: Filtration
    :header:    sepal\\_length,	sepal\\_width,	petal\\_length,	petal\\_width,	species
    :widths: 10,10,10,10,10

    6.3,	3.3,	6,	2.5,	virginica
	5.8,	2.7,	5.1,	1.9,	virginica
	7.1,	3,	5.9,	2.1,	virginica
	6.3,	2.9,	5.6,	1.8,	virginica
	6.5,	3,	5.8,	2.2,	virginica

Note that in ``SQL``, equality comparison is done with the ``=`` symbol, whereas in ``python``, it is with the ``==`` operator.
You can filter with multple conditions :

.. code-block:: SQL

    SELECT *
    FROM iris
    WHERE species = 'setosa'
    AND sepal_length = 5;

In ``datatable`` each condition is wrapped in parentheses; the ``&`` operator is the equivalent of ``AND``, while ``|`` is the equivalent of ``OR``.

.. code-block:: python

    iris[(f.species=="setosa") & (f.sepal_length==5), :]

.. csv-table:: Filtering on Multiple Conditions
    :header: sepal\\_length,	sepal\\_width,	petal\\_length,	petal\\_width,	species
    :widths: 10,10,10,10,10

	5,	3.6,	1.4,	0.2,	setosa
	5,	3.4,	1.5,	0.2,	setosa
	5,	3,	1.6,	0.2,	setosa
	5,	3.4,	1.6,	0.4,	setosa
	5,	3.2,	1.2,	0.2,	setosa
	5,	3.5,	1.3,	0.3,	setosa
	5,	3.5,	1.6,	0.6,	setosa
	5,	3.3,	1.4,	0.2,	setosa

Null rows can be filtered out as well :

.. csv-table:: Null Data
    :header: a, b, c
    :widths: 10,10,10

    1,    2,  3
    1,   NA,  4
    2,    1,  3
    1,    2,  2

The code below is how SQL would filter out the null rows :

.. code-block:: SQL

    SELECT *
    FROM null_data
    WHERE b is NOT NULL;

In ``datatable``, the ``NOT`` operator is replicated with the ``!=`` symbol :

.. code-block:: python

    null_data = dt.Frame(""" a    b    c
                             1    2    3
                             1    NaN  4
                             2    1    3
                             1    2    2""")

    null_data[f.b!=None, :]

You could also use the :func:`isna()` function with the ``~`` (tilde) symbol, which inverts the boolean selection :

.. code-block:: python

    null_data[~dt.isna(f.b), :]


.. csv-table:: Null Data Filtered out
    :header: a, b, c
    :widths: 10,10,10

    	1,	2,	3
    	2,	1,	3
    	1,	2,	2

Keeping the null rows is easily achievable; it is simply the inverse of the above code

``SQL``

.. code-block:: SQL

    SELECT *
    FROM null_data
    WHERE b is NULL;

``datatable``

.. code-block:: python

    null_data[dt.isna(f.b), :]

or :

.. code-block:: python

    null_data[f.b==None, :]

.. csv-table:: Null Rows only
    :header: a, b, c
    :widths: 10,10,10

    	1,	NA,	4

Note : ``SQL`` has the ``IN`` operator, which does not have an equivalent in ``datatable`` yet.

SORTING
-------

In SQL, sorting is executed with the ``ORDER BY`` clause, while in ``datatable`` it is handled by the :func:`sort()` function.

.. code-block:: SQL

    SELECT *
    FROM iris
    ORDER BY sepal_length ASC
    limit 5;

.. code-block:: python

    #datatable
    iris[:5, :, sort('sepal_length')]

.. csv-table:: Sorting in Ascending Order
    :header: sepal\\_length,	sepal\\_width,	petal\\_length,	petal\\_width,	species
    :widths: 10,10,10,10,10

    	4.3,	3,	1.1,	0.1,	setosa
    	4.4,	2.9,	1.4,	0.2,	setosa
    	4.4,	3,	1.3,	0.2,	setosa
    	4.4,	3.2,	1.3,	0.2,	setosa
    	4.5,	2.3,	1.3,	0.3,	setosa

Sorting in descending order in SQL is with the ``DESC``.

.. code-block:: SQL

    SELECT *
    FROM iris
    ORDER BY sepal_length DESC
    limit 5;

In datatable, this can be achieved in two ways :

.. code-block:: python

    #datatable
    iris[:5, :, sort('sepal_length', reverse=True)]

or, you could negate the sorting column; datatable will correctly interprete the negation(``-``) as descending order :

.. code-block:: python

    #datatable
    iris[:5, :, sort(-f.sepal_length)]

.. csv-table:: Sorting in Descending Order
    :header: sepal\\_length,	sepal\\_width,	petal\\_length,	petal\\_width,	species
    :widths: 10,10,10,10,10

    	7.9,	3.8,	6.4,	2,	virginica
    	7.7,	3.8,	6.7,	2.2,	virginica
    	7.7,	2.6,	6.9,	2.3,	virginica
    	7.7,	2.8,	6.7,	2,	virginica
    	7.7,	3,	6.1,	2.3,	virginica



GROUPBY
-------

SQL's ``GROUP BY`` operations can be performed in ``datatable`` with the :func:`by()` function.  Have a look at the :func:`by()` API, as well as the `Grouping with by <https://datatable.readthedocs.io/en/latest/manual/groupby_examples.html>`__ user guide.

Let's look at some common grouping operations in ``SQL``, and its equivalent in ``datatable``.

- Single Aggregation per group

.. code-block:: SQL

    SELECT species,
           COUNT() as N
    FROM iris
    GROUP BY species;


.. code-block:: python

    # datatable
    iris[:, dt.count(), by('species')]

.. csv-table:: Count per Group
    :header: species, count
    :widths: 10,10

    setosa,	50
    versicolor,	50
	virginica,	50


- Multiple Aggregations per group

.. code-block:: SQL

    SELECT species,
           COUNT() as N,
           AVG(sepal_length) as mean_sepal_length
    FROM iris
    GROUP BY species;


.. code-block:: python

    # datatable
    iris[:,
        {"mean_sepal_length": dt.mean(f.sepal_length),
        "N": dt.count()},
        by('species')]

.. csv-table:: Multiple Aggregations
    :header: species, mean\\_sepal\\_length, N
    :widths: 10,10, 10

    setosa,	5.006, 50
    versicolor,	5.936, 50
	virginica,	6.588, 50

- Grouping is also possible on multiple columns

.. csv-table:: Fruits Data
    :header:  Fruit,   Date,       Name,  Number
    :widths: 10,10,10,10

                  Apples,  10/6/2016,  Bob,     7
                  Apples,  10/6/2016,  Bob,     8
                  Apples,  10/6/2016,  Mike,    9
                  Apples,  10/7/2016,  Steve,  10
                  Apples,  10/7/2016,  Bob,     1
                  Oranges, 10/7/2016,  Bob,     2
                  Oranges, 10/6/2016,  Tom,    15
                  Oranges, 10/6/2016,  Mike,   57
                  Oranges, 10/6/2016,  Bob,    65
                  Oranges, 10/7/2016, Tony,    1
                  Grapes,  10/7/2016,  Bob,     1
                  Grapes,  10/7/2016,  Tom,    87
                  Grapes,  10/7/2016,  Bob,    22
                  Grapes,  10/7/2016, Bob,    12
                  Grapes,  10/7/2016,  Tony,   15

.. code-block:: SQL

    SELECT fruit,
           name,
           SUM(number) as sum_num
    FROM fruits_data
    GROUP BY fruit, name;


.. code-block:: python

    # datatable
    fruits_data[:,
                {"sum_num": dt.sum(f.Number)},
                by('Fruit', 'Name')]

.. csv-table:: Aggregations on Multiple COlumns
    :header: Fruit, Name, sum\\_num
    :widths: 10,10, 10

    Apples,	Bob,	16
	Apples,	Mike,	9
	Apples,	Steve,	10
	Grapes,	Bob,	35
	Grapes,	Tom,	87
	Grapes,	Tony,	15
	Oranges,	Bob,	67
	Oranges,	Mike,	57
	Oranges,	Tom,	15
	Oranges,	Tony,	1

- We can replicate SQL's ``WHERE`` clause in a ``GROUP BY``

.. code-block:: SQL

    SELECT species,
           AVG(sepal_length) as avg_sepal_length
    FROM iris
    WHERE sepal_width > 3
    GROUP BY species;


.. code-block:: python

    # datatable
    iris[f.sepal_width >=3, :][:,
                              {"avg_sepal_length": dt.mean(f.sepal_length)},
                              by('species')]

.. csv-table:: Filtration in a Group By
    :header: species, avg\\_sepal\\_length
    :widths: 10,10

    setosa,	5.02917
    versicolor,	6.21875
	virginica,	6.76897

- We can also replicate SQL's ``HAVING`` clause in a ``GROUP BY``

.. code-block:: SQL

    SELECT fruit,
           name,
           SUM(number) as sum_num
    FROM fruits_data
    GROUP BY fruit, name
    HAVING sum_num > 50;


.. code-block:: python

    # datatable
    fruits_data[:,
               {'sum_num': dt.sum(f.Number)},
               by('Fruit','Name')][f.sum_num > 50, :]

.. csv-table:: Filtration after a Group By
    :header: Fruit, Name, sum\\_num
    :widths: 10,10, 10

    Grapes,	Tom,	87
	Oranges,	Bob,	67
	Oranges,	Mike,	57


- Grouping on a condition

.. code-block:: SQL

    SELECT sepal_width >=3 as width_larger_than_3,
           AVG(sepal_length) as avg_sepal_length
    FROM iris
    GROUP BY sepal_width>=3;


.. code-block:: python

    # datatable
    iris[:,
         {"avg_sepal_length": dt.mean(f.sepal_length)},
         by(f.sepal_width >= 3)]

.. csv-table:: Grouping on a Condition
    :header: CO, avg\\_sepal\\_length
    :widths: 10,10

    	0,	5.95263
    	1,	5.77634

At the moment, names cannot be assigned in the ``by`` section.

LEFT OUTER JOIN
----------------

We will compare the left outer join, as that is the only join currently implemented in ``datatable``. Another aspect is that the frame being joined must be keyed, the column or columns to be keyed must not have duplicates, and the joining column has to have the same name in both frames. You can read more about the :func:`join()` API and have a look at the `Tutorial on the join operator <https://datatable.readthedocs.io/en/latest/start/quick-start.html#join>`_

Example data ::

    DT = dt.Frame(x = ["b"]*3 + ["a"]*3 + ["c"]*3,
                  y = [1, 3, 6] * 3,
                  v = range(1, 10))

    X = dt.Frame({"x":('c','b'),
                  "v":(8,7),
                  "foo":(4,2)})

A left outer join in SQL :

.. code-block:: SQL

    SELECT DT.x,
           DT.y,
           DT.v,
           X.foo
    FROM DT
    left join X
    on DT.x = X.x

A left outer join in ``datatable`` :

.. code-block:: python

    X.key = 'x'
    DT[:, [f.x, f.y, f.v, g.foo], join(X)]

.. csv-table:: Left Outer Jion
    :header:     x,	y,	v,	foo
    :widths: 10,10,10,10

	b,	1,	1,	2
	b,	3,	2,	2
	b,	6,	3,	2
	a,	1,	4,	NA
	a,	3,	5,	NA
	a,	6,	6,	NA
	c,	1,	7,	4
	c,	3,	8,	4
	c,	6,	9,	4

UNION
------

The ``UNION ALL`` clause in SQL can be replicated in ``datatable`` with :func:`rbind()`.

.. code-block:: SQL

    SELECT x, v
    FROM DT
    UNION ALL
    SELECT x, v
    FROM x

In ``datatable``, :func:`rbind()` takes a list/tuple of frames and lumps into one :

.. code-block:: python

    dt.rbind([DT[:, ('x','v')], X[:, ('x', 'v')]])

.. csv-table:: Union all
    :header: x, v
    :widths: 10,10

    	b,	1
    	b,	2
    	b,	3
    	a,	4
    	a,	5
    	a,	6
    	c,	7
    	c,	8
    	c,	9
    	b,	7
    	c,	8

SQL's ``UNION`` removes duplicate rows after combining the results of the individual queries; there is no built-in function in ``datatable`` yet that handles duplicates.

SQL's WINDOW FUNCTIONS
----------------------

Some SQL window functions can be replicated in ``datatable`` (`rank` is one of the windows function not currently implemented in datatable) :

- TOP n rows per group

.. code-block:: SQL

    SELECT * from
    (SELECT *,
           ROW_NUMBER() OVER(PARTITION BY species ORDER BY sepal_length DESC) as row_num
     FROM iris)
    WHERE row_num < 3;

.. code-block:: python

    #datatable
    iris[:3, :, by('species'), sort(-f.sepal_length)]

.. csv-table:: Top N rows per group
    :header: "sepal\\_length", "sepal\\_width", "petal\\_length", "petal\\_width",	"species"
    :widths: 10,10,10,10,10

    setosa,	5.8,	4,	1.2,	0.2
	setosa,	5.7,	4.4,	1.5,	0.4
	setosa,	5.7,	3.8,	1.7,	0.3
	versicolor,	7,	3.2,	4.7,	1.4
	versicolor,	6.9,	3.1,	4.9,	1.5
	versicolor,	6.8,	2.8,	4.8,	1.4
	virginica,	7.9,	3.8,	6.4,	2
	virginica,	7.7,	3.8,	6.7,	2.2
	virginica,	7.7,	2.6,	6.9,	2.3

- Filter for rows above the mean sepal length

.. code-block:: SQL

    SELECT sepal_length,
           sepal_width,
           petal_length,
           petal_width,
           species
    FROM
    (SELECT *,
    AVG(sepal_length) OVER (PARTITION BY species) as avg_sepal_length
    FROM iris)
    WHERE sepal_length > avg_sepal_length
    LIMIT 5;

.. code-block:: python

    #datatable
    iris[:,
         update(temp = f.sepal_length > dt.mean(f.sepal_length)),
         by('species')]

    iris[f.temp == 1, f[:-1]].head(5)

.. csv-table:: Rows above the mean sepal length
    :header: "sepal\\_length", "sepal\\_width", "petal\\_length", "petal\\_width",	"species"
    :widths: 10,10,10,10,10

        5.1,	3.5,	1.4,	0.2,	setosa
	    5.4,	3.9,	1.7,	0.4,	setosa
	    5.4,	3.7,	1.5,	0.2,	setosa
	    5.8,	4,	1.2,	0.2,	setosa
    	5.7,	4.4,	1.5,	0.4,	setosa

