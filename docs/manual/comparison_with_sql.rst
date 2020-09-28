.. py:currentmodule:: datatable

Comparison with SQL
====================

This page provides some examples of how various `SQL <https://en.wikipedia.org/wiki/SQL>`__ operations can be performed in ``datatable``. The ``datatable`` library is still growing; as such, not all functions in ``SQL`` can be replicated yet. If there is a feature you would love to have in ``datatable``, please make a feature request on the `github issues <https://github.com/h2oai/datatable/issues>`__ page. 

Most of the examples will be based on the famous `iris <https://en.wikipedia.org/wiki/Iris_flower_data_set>`__ dataset. The assumption here is that the data is already loaded into a database; we'll simply show the relevant ``SQL`` queries side by side with ``datatable`` constructs.

Let's import ``datatable`` and read in the data:

.. code-block:: python

    from datatable import dt, f, g, by, join, update, fread

    url = 'https://bit.ly/3kTNxM4'
    iris = fread(url)

    iris.head(5)


.. csv-table:: iris dataset
    :header: "sepal_length", "sepal_width", "petal_length", "petal_width",	"species"
    :widths: 10,10,10,10,20

    5.1,	3.5,	1.4,	0.2,	setosa
    	4.9,	3,	1.4,	0.2,	setosa
    	4.7,	3.2,	1.3,	0.2,	setosa
    	4.6,	3.1,	1.5,	0.2,	setosa
    	5,	3.6,	1.4,	0.2,	setosa


SELECT
-------

In ``SQL``, you can select a subset of the columns with the ``SELECT`` clause:

.. code-block:: SQL

    SELECT sepal_length, sepal_width, petal_length
    FROM iris
    LIMIT 5;

In ``datatable``, columns are selected in the ``j`` section :

.. code-block:: python

    iris[:5, ('sepal_length', 'sepal_width', 'petal_length')]


.. csv-table:: select columns
    :header: "sepal_length", "sepal_width", "petal_length"
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
    :header: "sepal_length", "sepal_width", "petal_length", "petal_width",	"species"
    :widths: 10,10,10,10,20

    5.1,	3.5,	1.4,	0.2,	setosa
    	4.9,	3,	1.4,	0.2,	setosa
    	4.7,	3.2,	1.3,	0.2,	setosa
    	4.6,	3.1,	1.5,	0.2,	setosa
    	5,	3.6,	1.4,	0.2,	setosa

How about adding new columns? In ``SQL``, this is done also in the ``SELECT`` clause :

.. code-block:: SQL

    SELECT *,
    sepal_length *2 as sepal_length_doubled 
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
    :header:    sepal_length,	sepal_width,	petal_length,	petal_width,	species,	sepal_length_doubled
    :widths: 10,10,10,10,20,10

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
    WHERE species = 'viriginica'
    LIMIT 5;

In ``datatable``, filtration is done in the ``i`` section :

.. code-block:: python

    iris[f.species=="virginica", :].head(5)

.. csv-table:: Filtration
    :header:    sepal_length,	sepal_width,	petal_length,	petal_width,	species
    :widths: 10,10,10,10,20

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
    WHERE species = 'setosa' AND sepal_length = 5;

In ``datatable`` each condition is wrapped in parentheses; the ``&`` operator is the equivalent of ``AND``, while ``|`` is the equivalent of ``OR``.

.. code-block:: python

    iris[(f.species=="setosa")&(f.sepal_length==5), :]

.. csv-table:: Filtering on Multiple Conditions
    :header: sepal_length,	sepal_width,	petal_length,	petal_width,	species
    :widths: 10,10,10,10,20
    
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

    dt.Frame(""" a    b    c
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

    # datatable
    null_data[dt.isna(f.b), :]

or :

.. code-block:: python

    null_data[f.b==None, :]

.. csv-table:: Null Rows only
    :header: a, b, c
    :widths: 10,10,10

    	1,	NA,	4

Note : ``SQL`` has the ``IN`` operator, which does not have an equivalent in ``datatable`` yet.

GROUPBY
-------