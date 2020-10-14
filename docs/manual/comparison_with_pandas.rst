.. py:currentmodule:: datatable


.. raw:: html

    <style>
    .wy-nav-content { max-width: 100%; }
    </style>


Comparison with Pandas
=======================
A lot of potential ``datatable`` users are likely to have some familiarity with `pandas <https://pandas.pydata.org/pandas-docs/stable/index.html>`__; as such, this page provides some examples of how various ``pandas`` operations can be performed within ``datatable``. ``datatable`` emphasizes speed and big data support (an area that ``pandas`` struggles with); it also has an expressive and concise syntax, which makes ``datatable`` also useful for small datasets.

.. note:: In ``pandas``, there are two fundamental data structures, `Series <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.Series.html#pandas.Series>`__ and `DataFrame <https://pandas.pydata.org/pandas-docs/stable/user_guide/dsintro.html#dataframe>`__. In ``datatable``, there is only one fundamental data structure - the `Frame <https://datatable.readthedocs.io/en/latest/api/frame.html#datatable-frame>`__. Most of the comparisons will be between ``pandas`` DataFrame and ``datatable`` Frame.

.. code-block:: python

    import pandas as pd
    import numpy as np
    from datatable import dt, f, by, g, join, sort, update, ifelse

    data = {"A": [1, 2, 3, 4, 5],
            "B": [4, 5, 6, 7, 8],
            "C": [7, 8, 9, 10, 11],
            "D": [5, 7, 2, 9, -1]}

    # datatable
    DT = dt.Frame(data)

    # pandas
    df = pd.DataFrame(data)


Row and Column Selection
------------------------

=================================================  ============================================
        Pandas                                              datatable
=================================================  ============================================
Select a single row
``df.loc[2]``                                        ``DT[2, :]``

Select several rows by their indices
``df.iloc[[2,3,4]]``                                  ``DT[[2,3,4], :]``

Select a slice of rows by position
``df.iloc[2:5]``                                      ``DT[2:5, :]``


Same as above
``df.iloc[range(2,5)]``                               ``DT[range(2,5), :]``


Select every second row
``df.iloc[::2]``                                       ``DT[::2, :]``

Select rows using a Boolean mask
``df.iloc[[True, True, False, False, True]]``            ``DT[[True, True, False, False, True], :]``

Select rows on a condition
``df.loc[df['A']>3]``                                   ``DT[f.A>3, :]``

Select rows on multiple conditions, using ``OR``
``df.loc[(df['A'] > 3) | (df['B']<5)]``                   ``DT[(f.A>3) | (f.B<5), :]``

Select rows on multiple conditions, using ``AND``
``df.loc[(df['A'] > 3) & (df['B']<8)]``                  ``DT[(f.A>3) & (f.B<8), :]``

Select a single column by column name
       ``df['A']``                                     ``DT['A']``

Same as above
       ``df.loc[:, 'A']``                              ``DT[:, 'A']``

Select a single column by position
``df.iloc[:, 1]``                                       ``DT[1]`` or ``DT[:, 1]``

Select multiple columns by column names
``df.loc[:, ["A", "B"]]``                              ``DT[:, ["A", "B"]]``

Select multiple columns by position
``df.iloc[:, [0, 1]]``                                ``DT[:, [0, 1]]``

Select multiple columns by slicing
``df.loc[:, "A":"B"]``                                 ``DT[:, "A":"B"]``

Same as above, by position
``df.iloc[:, 1:3]``                                      ``DT[:, 1:3]``

Select columns by Boolean mask
``df.loc[:, [True,False,False,True]]``                ``DT[:, [True,False,False,True]]``

Select multiple rows and columns
``df.loc[2:5, "A":"B"]``                              ``DT[2:5, "A":"B"]``

Same as above, by position
``df.iloc[2:5, :2]``                                    ``DT[2:5, :2]``

Select a single value (returns a scalar)
``df.at[2, 'A']`` or ``df.loc[2, 'A']``                 ``DT[2,"A"]``

Same as above, by position
``df.iat[2, 0]``  or  ``df.iloc[2, 0]``                 ``DT[2, 0]``

Select a single value, return as Series                Returns a Frame
``df.loc[2, ["A"]]``                                  ``DT[2, ["A"]]``

Same as above, by position
``df.iloc[2, [0]]``                                  ``DT[2, [0]]``
=================================================  ============================================


In ``pandas`` every frame has a row index, and if a filtration is executed, the row number is returned :

.. code-block:: python

    # pandas
    df.loc[df['A']>3]

    	A	B	C	D
    3	4	7	10	9
    4	5	8	11     -1

``datatable`` has no notion of a row index; the row numbers displayed are just for convenience:

.. code-block:: python

    DT[f.A>3, :]

        A	B	C	D
    0	4	7	10	9
    1	5	8	11     −1



In ``pandas``, the index can be numbers, or characters, or intervals, or even MultiIndexes; you can subset rows on these labels.

.. code-block:: python

    # pandas
    df1 = df.set_index(pd.Index(['a','b','c','d','e']))

        A	B	C	D
    a	1	4	7	5
    b	2	5	8	7
    c	3	6	9	2
    d	4	7	10	9
    e	5	8	11     -1

    df1.loc["a":"c"]


        A	B	C	D
    a	1	4	7	5
    b	2	5	8	7
    c	3	6	9	2

``datatable`` has the `key <https://datatable.readthedocs.io/en/latest/api/frame/key.html#datatable-frame-key>`__ property, which is meant as an equivalent of pandas indices, but its purpose at the moment is for joins, not for subsetting data :

.. code-block:: python

    # datatable
    data = {"A": [1, 2, 3, 4, 5],
            "B": [4, 5, 6, 7, 8],
            "C": [7, 8, 9, 10, 11],
            "D": [5, 7, 2, 9, -1],
            "E": ['a','b','c','d','e']}

    DT1 = dt.Frame(data)

    DT1.key = 'E'

    DT1

    E	A	B	C	D
    a	1	4	7	5
    b	2	5	8	7
    c	3	6	9	2
    d	4	7	10	9
    e	5	8	11     −1

    # this will fail
    DT1["a":"c", :]

    ---------------------------------------------------------------------------
    TypeError                                 Traceback (most recent call last)
    <ipython-input-98-73c453287f07> in <module>
    ----> 1 DT["a":"c", :]

    TypeError: A string slice cannot be used as a row selector

Pandas' ``loc`` notation works on labels, while ``iloc`` works on actual position. This is noticeable during row selection.  ``datatable``, however, works only on position.

.. code-block:: python

    df1 = df.set_index('C')

        A	B	D
    C
    7	1	4	5
    8	2	5	7
    9	3	6	2
    10	4	7	9
    11	5	8      -1

Selecting with loc for the row with number 7 returns no error :

.. code-block:: python

    df1.loc[7]

    A    1
    B    4
    D    5
    Name: 7, dtype: int64

However, selecting with ``iloc`` for the row with number 7 returns an error, because positionally, there is no row 7 :

.. code-block:: python

    df.iloc[7]

    ---------------------------------------------------------------------------
   # Lots of code here related to the error message
   .....

    IndexError: single positional indexer is out-of-bounds

As stated earlier, ``datatable`` has the `key <https://datatable.readthedocs.io/en/latest/api/frame/key.html#datatable-frame-key>`__ property, which is used for joins, not row subsetting, and as such selection similar to ``loc`` with the row label is not possible.

.. code-block:: python

    # datatable

    DT.key = 'C'

    DT

    C	A	B	D
    7	1	4	5
    8	2	5	7
    9	3	6	2
    10	4	7	9
    11	5	8      −1

    # this will fail
    DT[7, :]

    ---------------------------------------------------------------------------
    ValueError                                Traceback (most recent call last)
    <ipython-input-107-e5be0baed765> in <module>
    ----> 1 DT[7, :]

    ValueError: Row 7 is invalid for a frame with 5 rows


Add New/Update Existing Columns
-------------------------------

=======================================================  ===============================================================
        Pandas                                              datatable
=======================================================  ===============================================================
Add a new column with a scalar value
``df['new_col'] = 2``                                        ``DT['new_col'] = 2``

Same as above
``df = df.assign(new_col = 2)``                              ``DT[:, update(new_col=2)]``

Add a new column with a list of values
``df['new_col'] = range(len(df))``                           ``DT['new_col_1'] = range(DT.nrows)``

Same as above
``df = df.assign(new_col = range(len(df))``                  ``DT[:, update(new_col=range(DT.nrows)]``

Update a single value
``df.at[2, 'new_col'] = 200``                                ``DT[2, 'new_col'] = 200``

Update an entire column
``df.loc[:, "A"] = 5``  or ``df["A"] = 5``                   ``DT["A"] = 5``

Same as above
``df = df.assign(A = 5)``                                    ``DT[:, update(A = 5)]``

Update multiple columns
``df.loc[:, "A":"C"] = np.arange(15).reshape(-1,3)``        ``DT[:, "A":"C"] = np.arange(15).reshape(-1,3)``
=======================================================  ===============================================================

.. note:: In ``datatable``, the :func:`update()` method is in-place; reassigment to the Frame ``DT`` is not required.


Rename Columns
--------------

=======================================================  ===============================================================
        Pandas                                              datatable
=======================================================  ===============================================================
Rename a column
``df = df.rename(columns={"A":"col_A"})``                    ``DT.names = {"A" : "col_A"}``

Rename multiple columns
``df = df.rename(columns={"A":"col_A", "B":"col_B"})``      ``DT.names = {"A" : "col_A", "B": "col_B"}``
=======================================================  ===============================================================

In datatable, you can select and rename columns at the same time, by passing a dictionary of :ref:`f-expressions` into the ``j`` section :

.. code-block:: python

    # datatable
    DT[:, {"A": f.A, "box": f.B, "C": f.C, "D": f.D * 2}]

	A	box	C	D
    0	1	4	7	10
    1	2	5	8	14
    2	3	6	9	4
    3	4	7	10	18
    4	5	8	11	−2


Delete Columns
--------------

=======================================================  ===============================================================
        Pandas                                              datatable
=======================================================  ===============================================================
Delete a column
``del df['B']``                                                ``del DT['B']``

Same as above
``df = df.drop('B', axis=1)``                                 ``DT = DT[:, f[:].remove(f.B)]``

Remove multiple columns
``df = df.drop(['B', 'C'], axis=1)``                         | ``del DT[: , ['B', 'C']]`` or
                                                             | ``DT = DT[:, f[:].remove([f.B, f.C])]``
=======================================================  ===============================================================



Sorting
-------

===========================================================  ===============================================================
        Pandas                                                datatable
===========================================================  ===============================================================
Sort by a column - default ascending
``df.sort_values('A')``                                       ``DT.sort('A')`` or ``DT[:, : , sort('A')]``

Sort by a column - descending
``df.sort_values('A',ascending=False)``                       | ``DT.sort(-f.A)`` or ``DT[:, :, sort(-f.A)]`` or
                                                              | ``DT[:, :, sort('A', reverse=True)]``

Sort by multiple columns - default ascending
``df.sort_values(['A','C'])``                                 ``DT.sort('A','C')`` or ``DT[:, :, sort('A','C')]``

Sort by multiple columns - both descending
``df.sort_values(['A','C'],ascending=[False,False])``         | ``DT.sort(-f.A, -f.C)`` or
                                                              | ``DT[:, :, sort(-f.A, -f.C)]`` or
                                                              | ``DT[:, :, sort('A', 'C', reverse=[True, True])]``

Sort by multiple columns - different sort directions
``df.sort_values(['A', 'C'], ascending=[True, False])``       | ``DT.sort(f.A, -f.C)`` or
                                                              | ``DT[:, :, sort(f.A, -f.C)]`` or
                                                              | ``DT[:, :, sort('A', 'C', reverse=[False, True])]``
===========================================================  ===============================================================

.. note:: By default, ``Pandas`` puts NAs last in the sorted data, while ``datatable`` puts them first.

.. note:: In ``Pandas``, there is an option to sort with a Callable; this option is not supported in ``datatable``.

.. note:: In ``Pandas``, you can sort on the rows or columns; in ``datatable`` sorting is column-wise only.

Grouping and Aggregation
------------------------

.. code-block:: python

    data = {"a": [1, 1, 2, 1, 2],
            "b": [2, 20, 30, 2, 4],
            "c": [3, 30, 50, 33, 50]}

    # datatable
    DT = dt.Frame(data)

    # pandas
    df = pd.DataFrame(data)


===========================================================  ===============================================================
        Pandas                                                datatable
===========================================================  ===============================================================
Group by ``a`` and sum the other columns
``df.groupby("a").agg("sum")``                                  ``DT[:, dt.sum(f[:]), by("a")]``

Group by ``a`` and ``b`` and sum ``c``
``df.groupby(["a", "b"]).agg("sum")``                           ``DT[:, dt.sum(f.c), by("a", "b")]``

Get size per group
``df.groupby("a").size()``                                      ``DT[:, dt.count(), by("a")]``

Grouping with multiple aggregation functions
``df.groupby("a").agg({"b": "sum", "c": "mean"})``              | ``DT[:, {"b": dt.sum(f.b), "c": dt.mean(f.c)}, by("a")]``

Get the first row per group
``df.groupby("a").first()``                                     ``DT[0, :, by("a")]``

Get the last row per group
``df.groupby('a').last()``                                      ``DT[-1, :, by("a")]``

Get the first two rows per group
``df.groupby("a").head(2)``                                     ``DT[:2, :, by("a")]``

Get the last two rows per group
``df.groupby("a").tail(2)``                                     ``DT[-2:, :, by("a")]``
===========================================================  ===============================================================

Transformations within groups in pandas is done using the `transform <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.core.groupby.DataFrameGroupBy.transform.html>`__ function :

.. code-block:: python

    # pandas
    grouping = df.groupby("a")["b"].transform("min")
    df.assign(min_b=grouping)

        a	b	c	min_b
    0	1	2	3	2
    1	1	20	30	2
    2	2	30	50	4
    3	1	2	33	2
    4	2	4	50	4

In ``datatable``, transformations occur within the ``j`` section; in the presence of :func:`by()`, the computations within ``j`` are per group :

.. code-block:: python

    DT[:, f[:].extend({"min_b": dt.min(f.b)}), by("a")]

    	a	b	c	min_b
    0	1	2	3	2
    1	1	20	30	2
    2	1	2	33	2
    3	2	30	50	4
    4	2	4	50	4

Note that the result above is sorted by the grouping column. If you want the data to maintain the same shape as the source data, then :func:`update()` is a better option (and usually faster) :


.. code-block:: python

    DT[:, update(min_b = dt.min(f.b)), by("a")]

    DT

        a	b	c	min_b
    0	1	2	3	2
    1	1	20	30	2
    2	2	30	50	4
    3	1	2	33	2
    4	2	4	50	4

In Pandas, some computations might require creating the column first before aggregation within a groupby. Take the example below, where we need to calculate the revenue per group :

.. code-block:: python

    data = {'shop': ['A', 'B', 'A'],
            'item_price': [123, 921, 28],
            'item_sold': [1, 2, 4]}

    df1 = pd.DataFrame(data) # pandas
    DT1 = dt.Frame(data)  # datatable

To get the total revenue, we first need to create a revenue column, then sum it in the groupby :

.. code-block:: python

    df1['revenue'] = df1['item_price'] * df1['item_sold']
    df1.groupby("shop")['revenue'].sum().reset_index()

    	shop	revenue
    0	A	235
    1	B	1842

In ``datatable``, there is no need to create a temporary column; you can easily nest your computations in the ``j`` section; the computations will be executed per group :

.. code-block:: python

    # datatable

    DT1[:, {"revenue": dt.sum(f.item_price * f.item_sold)}, "shop"]

        shop	revenue
    0	A	235
    1	B	1842

You can learn more about the :func:`by()` function  at the `Grouping with by <https://datatable.readthedocs.io/en/latest/manual/groupby_examples.html>`__ documentation, as well as the API : :func:`by()`.


.. note:: Pandas allows custom functions via the ``apply`` or ``pipe`` method. ``datatable`` does not yet support custom functions.

.. note:: Also missing in ``datatable`` but available in Pandas are cumulative functions (cumsum, cumprod, ...), some aggregate functions like `nunique`, `ngroup`, ..., as well as windows functions (rolling, expanding, ...)

CONCATENATE
------------

In Pandas you can combine multiple dataframes using the ``concatenate`` method; the concatenation is based on the indices :

.. code-block:: python

    # pandas
    df1 = pd.DataFrame({"A": ["a", "a", "a"], "B": range(3)})

    df2 = pd.DataFrame({"A": ["b", "b", "b"], "B": range(4, 7)})

By default, pandas concatenates the rows, with one dataframe on top of the other:

.. code-block:: python

    pd.concat([df1, df2], axis = 0)

        A	B
    0	a	0
    1	a	1
    2	a	2
    0	b	4
    1	b	5
    2	b	6

The same functionality can be replicated in ``datatable`` using the `rbind <file:///home/sam/github_cloned_projects/datatable/docs/_build/html/api/frame/rbind.html>`__ function:

.. code-block:: python

    # datatable
    DT1 = dt.Frame(df1)
    DT2 = dt.Frame(df2)

    dt.rbind([DT1, DT2])

        A	B
    0	a	0
    1	a	1
    2	a	2
    3	b	4
    4	b	5
    5	b	6

Notice how in ``Pandas`` the indices are preserved (you can get rid of the indices with the `ignore_index` argument), whereas in ``datatable`` the indices are not referenced.

To combine data across the columns, in ``Pandas``, you set the axis argument to ``columns`` :

.. code-block:: python

    # pandas
    df1 = pd.DataFrame({"A": ["a", "a", "a"], "B": range(3)})

    df2 = pd.DataFrame({"C": ["b", "b", "b"], "D": range(4, 7)})

    df3 = pd.DataFrame({"E": ["c", "c", "c"], "F": range(7, 10)})

    pd.concat([df1, df2, df3], axis = 1)

    	A	B	C	D	E	F
    0	a	0	b	4	c	7
    1	a	1	b	5	c	8
    2	a	2	b	6	c	9

In ``datatable``, you combine frames along the columns using the `cbind <file:///home/sam/github_cloned_projects/datatable/docs/_build/html/api/frame/cbind.html>`__ function :

.. code-block:: python

    # datatable

    DT1 = dt.Frame(df1)
    DT2 = dt.Frame(df2)
    DT3 = dt.Frame(df3)

    dt.cbind([DT1, DT2, DT3])

        A	B	C	D	E	F
    0	a	0	b	4	c	7
    1	a	1	b	5	c	8
    2	a	2	b	6	c	9

In ``Pandas``, if you concatenate dataframes along the rows, and the columns do not match, a dataframe of all the columns is returned, with null values for the missing rows :

.. code-block:: python

    # pandas
    pd.concat([df1, df2, df3], axis = 0)

        A	B	C	D	E	F
    0	a	0.0	NaN	NaN	NaN	NaN
    1	a	1.0	NaN	NaN	NaN	NaN
    2	a	2.0	NaN	NaN	NaN	NaN
    0	NaN	NaN	b	4.0	NaN	NaN
    1	NaN	NaN	b	5.0	NaN	NaN
    2	NaN	NaN	b	6.0	NaN	NaN
    0	NaN	NaN	NaN	NaN	c	7.0
    1	NaN	NaN	NaN	NaN	c	8.0
    2	NaN	NaN	NaN	NaN	c	9.0

In ``datatable``, if you concatenate along the rows and the columns in the frames do not match, you get an error message; you can however force the row combinations, by passing ``force=True`` :

.. code-block:: python

    # datatable
    dt.rbind([DT1, DT2, DT3], force=True)

        A	B	C	D	E	F
    0	a	0	NA	NA	NA	NA
    1	a	1	NA	NA	NA	NA
    2	a	2	NA	NA	NA	NA
    3	NA	NA	b	4	NA	NA
    4	NA	NA	b	5	NA	NA
    5	NA	NA	b	6	NA	NA
    6	NA	NA	NA	NA	c	7
    7	NA	NA	NA	NA	c	8
    8	NA	NA	NA	NA	c	9

.. note:: :func:`rbind()` and :func:`cbind()` methods exist for the frames, and operate in-place.


JOIN/MERGE
----------

``Pandas`` has a variety of options for joining dataframes, using the ``join`` or ``merge`` method; in ``datatable``, only the left join is possible, and there are certain limitations. You have to set keys on the dataframe to be joined, and the keys must be unique. The main function in ``datatable`` for joining dataframes based on column values is the :func:`join()` function. As such, our comparison will be limited to left-joins only.

In Pandas, you can join dataframes easily with the ``merge`` method :

.. code-block:: python

    df1 = pd.DataFrame({x : ["b"]*3 + ["a"]*3 + ["c"]*3,
              y : [1, 3, 6] * 3,
              v : range(1, 10)})

    df2 = pd.DataFrame({"x":('c','b'),
                  "v":(8,7),
                  "foo":(4,2)})

    df1.merge(df2, on="x", how="left")

    	x	y	v_x	v_y	foo
    0	b	1	1	7.0	2.0
    1	b	3	2	7.0	2.0
    2	b	6	3	7.0	2.0
    3	a	1	4	NaN	NaN
    4	a	3	5	NaN	NaN
    5	a	6	6	NaN	NaN
    6	c	1	7	8.0	4.0
    7	c	3	8	8.0	4.0
    8	c	6	9	8.0	4.0

In datatable, there are limitations currently. First, the joining dataframe must be keyed. Second, the column(s) used as the joining key(s) must be unique. Third, the join columns must have the same name.

.. code-block:: python

    DT1 = dt.Frame(df1)
    DT2 = dt.Frame(df2)

    # set key on DT2
    DT2.key = 'x'

    DT1[:, :, join(DT2)]

        x	y	v	v.0	foo
    0	b	1	1	7	2
    1	b	3	2	7	2
    2	b	6	3	7	2
    3	a	1	4	NA	NA
    4	a	3	5	NA	NA
    5	a	6	6	NA	NA
    6	c	1	7	8	4
    7	c	3	8	8	4
    8	c	6	9	8	4

More details about joins in ``datatable`` can be found at the :func:`join()` API and have a look at the `Tutorial on the join operator <https://datatable.readthedocs.io/en/latest/start/quick-start.html#join>`_

MORE EXAMPLES
-------------

This section shows how some solutions in ``Pandas`` can be translated to ``datatable``; most of the examples used here, as well as the ``Pandas`` solutions,  are from the `Pandas cookbook <https://pandas.pydata.org/pandas-docs/stable/user_guide/cookbook.html>`__.


- if-then-else using numpy’s where() :

.. code-block:: python

    # pandas
    df = pd.DataFrame({"AAA": [4, 5, 6, 7],
                       "BBB": [10, 20, 30, 40],
                       "CCC": [100, 50, -30, -50]})

    df

    	AAA	BBB	CCC
    0	4	10	100
    1	5	20	50
    2	6	30	-30
    3	7	40	-50

Solution in ``Pandas`` ::

    df['logic'] = np.where(df['AAA'] > 5, 'high', 'low')

        AAA  BBB  CCC logic
    0    4   10  100   low
    1    5   20   50   low
    2    6   30  -30  high
    3    7   40  -50  high

In ``datatable``, this can be solved using the :func:`ifelse()` function

.. code-block:: python

    # datatable
    DT = dt.Frame(df)

    DT["logic"] = DT[:, ifelse(f.AAA > 5, "high", "low")]

    DT
        AAA	BBB	CCC	logic
    0	4	10	100	low
    1	5	20	50	low
    2	6	30	−30	high
    3	7	40	−50	high

- Select rows with data closest to certain value using argsort

.. code-block:: python

    # pandas
    df = pd.DataFrame({"AAA": [4, 5, 6, 7],
                       "BBB": [10, 20, 30, 40],
                       "CCC": [100, 50, -30, -50]})

    aValue = 43.0

Solution in ``Pandas`` ::

    df.loc[(df.CCC - aValue).abs().argsort()]

     AAA  BBB  CCC
    1    5   20   50
    0    4   10  100
    2    6   30  -30
    3    7   40  -50

In ``datatable``, the ``newsort`` function is roughly similar to `np.argsort <https://numpy.org/doc/stable/reference/generated/numpy.argsort.html>`__ or `pd.Series.argsort <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.Series.argsort.html>`__ ::

    DT = dt.Frame(df)

    order = DT[:, dt.math.abs(f.CCC - aValue)].newsort()

    order

    	order
    0	1
    1	0
    2	2
    3	3

Now, we can apply the ``order`` variable to the ``i`` section ::

    DT[order, :]

        AAA	BBB	CCC
    0	5	20	50
    1	4	10	100
    2	6	30	−30
    3	7	40	−50

Of course, you can skip creating a temporary variable (at the expense of readability) ::

    DT[DT[:, dt.math.abs(f.CCC - aValue)].newsort(), :]



- Efficiently and dynamically creating new columns using applymap

.. code-block:: python

    # pandas
    df = pd.DataFrame({"AAA": [1, 2, 1, 3],
                       "BBB": [1, 1, 2, 2],
                       "CCC": [2, 1, 3, 1]})

        AAA	BBB	CCC
    0	1	1	2
    1	2	1	1
    2	1	2	3
    3	3	2	1

    source_cols = df.columns

    new_cols = [str(x) + "_cat" for x in source_cols]

    categories = {1: 'Alpha', 2: 'Beta', 3: 'Charlie'}

    df[new_cols] = df[source_cols].applymap(categories.get)

    df

        AAA  BBB  CCC  AAA_cat BBB_cat  CCC_cat
    0    1    1    2    Alpha   Alpha     Beta
    1    2    1    1     Beta   Alpha    Alpha
    2    1    2    3    Alpha    Beta  Charlie
    3    3    2    1  Charlie    Beta    Alpha

We can replicate the solution above in ``datatable`` :

.. code-block:: python

    # datatable

    import itertools as it

    DT = dt.Frame(df)

    mixer = it.product(DT.names, categories)

    conditions = [(name, f[name] == value, categories[value])
                  for name, value in mixer]

    for name, cond, value in conditions:
        DT[cond, f"{name}_cat"] = value

        AAA	BBB	CCC	AAA_cat	BBB_cat	CCC_cat
    0	1	1	2	Alpha	Alpha	Beta
    1	2	1	1	Beta	Alpha	Alpha
    2	1	2	3	Alpha	Beta	Charlie
    3	3	2	1	Charlie	Beta	Alpha

- Keep other columns when using min() with groupby

.. code-block:: python

    # pandas
    df = pd.DataFrame({'AAA': [1, 1, 1, 2, 2, 2, 3, 3],
                       'BBB': [2, 1, 3, 4, 5, 1, 2, 3]})

    df

      AAA  BBB
    0    1    2
    1    1    1
    2    1    3
    3    2    4
    4    2    5
    5    2    1
    6    3    2
    7    3    3

Solution in ``Pandas``::

    df.loc[df.groupby("AAA")["BBB"].idxmin()]

           AAA  BBB
    1    1    1
    5    2    1
    6    3    2

In ``datatable``, you can :func:`sort()` within a group, to achieve the same result above :

.. code-block:: python

    # datatable

    DT = dt.Frame(df)

    DT[0, :, by("AAA"), sort(f.BBB)]

        AAA	BBB
    0	1	1
    1	2	1
    2	3	2

- Apply to different items in a group

.. code-block:: python

    # pandas

    df = pd.DataFrame({'animal': 'cat dog cat fish dog cat cat'.split(),
                       'size': list('SSMMMLL'),
                       'weight': [8, 10, 11, 1, 20, 12, 12],
                       'adult': [False] * 5 + [True] * 2})

    df

      animal size  weight  adult
    0    cat    S       8  False
    1    dog    S      10  False
    2    cat    M      11  False
    3   fish    M       1  False
    4    dog    M      20  False
    5    cat    L      12   True
    6    cat    L      12   True


Solution in ``Pandas`` ::

    def GrowUp(x):
        avg_weight = sum(x[x['size'] == 'S'].weight * 1.5)
        avg_weight += sum(x[x['size'] == 'M'].weight * 1.25)
        avg_weight += sum(x[x['size'] == 'L'].weight)
        avg_weight /= len(x)
        return pd.Series(['L', avg_weight, True],
                         index=['size', 'weight', 'adult'])

    expected_df = gb.apply(GrowUp)

            size   weight  adult
    animal
    cat       L  12.4375   True
    dog       L  20.0000   True
    fish      L   1.2500   True

In ``datatable``, we can use the :func:`ifelse()` function to replicate the solution above, since it is based on a series of conditions::

    DT = dt.Frame(df)

    conditions = ifelse(f.size == "S", f.weight * 1.5,
                        f.size == "M", f.weight * 1.25,
                        f.size == "L", f.weight,
                        None)

    DT[:, {"size": "L",
           "avg_wt": dt.sum(conditions) / dt.count(),
           "adult": True},
       by("animal")]

        animal	size	avg_wt	adult
    0	cat	L	12.4375	1
    1	dog	L	20	1
    2	fish	L	1.25	1

.. note:: :func:`ifelse()` can take multiple conditions, along with a default return value.

.. note:: Custom functions are not supported in ``datatable`` yet.

- Sort groups by aggregated data

.. code-block:: python

    # pandas

    df = pd.DataFrame({'code': ['foo', 'bar', 'baz'] * 2,
                       'data': [0.16, -0.21, 0.33, 0.45, -0.59, 0.62],
                       'flag': [False, True] * 3})

        code	data	flag
    0	foo	0.16	False
    1	bar	-0.21	True
    2	baz	0.33	False
    3	foo	0.45	True
    4	bar	-0.59	False
    5	baz	0.62	True

Solution in ``Pandas`` ::

    code_groups = df.groupby('code')

    agg_n_sort_order = code_groups[['data']].transform(sum).sort_values(by='data')

    sorted_df = df.loc[agg_n_sort_order.index]

    sorted_df

        code  data   flag
    1  bar -0.21   True
    4  bar -0.59  False
    0  foo  0.16  False
    3  foo  0.45   True
    2  baz  0.33  False
    5  baz  0.62   True

The solution above sorts the data based on the sum of the ``data`` column per group in the ``code`` column.

We can replicate this in ``datatable`` ::

    DT = dt.Frame(df)

    DT[:, update(sum_data = dt.sum(f.data)), by("code")]

    DT[:, :-1, sort(f.sum_data)]

        code	data	flag
    0	bar	−0.21	1
    1	bar	−0.59	0
    2	foo	0.16	0
    3	foo	0.45	1
    4	baz	0.33	0
    5	baz	0.62	1

- Create a value counts column and reassign back to the DataFrame

.. code-block:: python

    # pandas
    df = pd.DataFrame({'Color': 'Red Red Red Blue'.split(),
                       'Value': [100, 150, 50, 50]})

    df

      Color  Value
    0   Red    100
    1   Red    150
    2   Red     50
    3  Blue     50

Solution in ``Pandas`` ::

    df['Counts'] = df.groupby(['Color']).transform(len)

    df

        Color  Value  Counts
    0   Red    100       3
    1   Red    150       3
    2   Red     50       3
    3  Blue     50       1

In ``datatable``, you can replicate the solution above with the :func:`count()` function ::

    DT = dt.Frame(df)

    DT[:, update(Counts=dt.count()), by("Color")]

        Color	Value	Counts
    0	Red	100	3
    1	Red	150	3
    2	Red	50	3
    3	Blue	50	1

- Shift groups of the values in a column based on the index

.. code-block:: python

    # pandas

    df = pd.DataFrame({'line_race': [10, 10, 8, 10, 10, 8],
                       'beyer': [99, 102, 103, 103, 88, 100]},
                       index=['Last Gunfighter', 'Last Gunfighter',
                              'Last Gunfighter', 'Paynter', 'Paynter',
                              'Paynter'])

    df

                        line_race  beyer
    Last Gunfighter         10     99
    Last Gunfighter         10    102
    Last Gunfighter          8    103
    Paynter                 10    103
    Paynter                 10     88
    Paynter                  8    100


Solution in ``Pandas`` ::

    df['beyer_shifted'] = df.groupby(level=0)['beyer'].shift(1)

    df

                        line_race  beyer  beyer_shifted
    Last Gunfighter         10     99            NaN
    Last Gunfighter         10    102           99.0
    Last Gunfighter          8    103          102.0
    Paynter                 10    103            NaN
    Paynter                 10     88          103.0
    Paynter                  8    100           88.0

``datatable`` has an equivalent :func:`shift()` function ::

    DT = dt.Frame(df) # the index becomes part of the frame

    DT[:, update(beyer_shifted = dt.shift(f.beyer)), by("index")]

    DT

    line_race	beyer	index	        beyer_shifted
    0	10	99	Last Gunfighter	    NA
    1	10	102	Last Gunfighter	    99
    2	8	103	Last Gunfighter	    102
    3	10	103	Paynter	            NA
    4	10	88	Paynter	            103
    5	8	100	Paynter	            88

- Frequency table like plyr in R

.. code-block:: python

    grades = [48, 99, 75, 80, 42, 80, 72, 68, 36, 78]

    df = pd.DataFrame({'ID': ["x%d" % r for r in range(10)],
                       'Gender': ['F', 'M', 'F', 'M', 'F',
                                  'M', 'F', 'M', 'M', 'M'],
                       'ExamYear': ['2007', '2007', '2007', '2008', '2008',
                                    '2008', '2008', '2009', '2009', '2009'],
                       'Class': ['algebra', 'stats', 'bio', 'algebra',
                                 'algebra', 'stats', 'stats', 'algebra',
                                 'bio', 'bio'],
                       'Participated': ['yes', 'yes', 'yes', 'yes', 'no',
                                        'yes', 'yes', 'yes', 'yes', 'yes'],
                       'Passed': ['yes' if x > 50 else 'no' for x in grades],
                       'Employed': [True, True, True, False,
                                    False, False, False, True, True, False],
                       'Grade': grades})

    df

        ID	Gender	ExamYear	Class	Participated	Passed	    Employed	  Grade
    0	x0	F	2007	        algebra	    yes	        no	    True            48
    1	x1	M	2007	        stats	    yes	        yes	    True	    99
    2	x2	F	2007	        bio	    yes	        yes	    True	    75
    3	x3	M	2008	        algebra	    yes	        yes	    False	    80
    4	x4	F	2008	        algebra	    no	        no	    False	    42
    5	x5	M	2008	        stats	    yes	        yes	    False	    80
    6	x6	F	2008	        stats	    yes	        yes	    False	    72
    7	x7	M	2009	        algebra	    yes	        yes	    True	    68
    8	x8	M	2009	        bio	    yes	        no	    True	    36
    9	x9	M	2009	        bio	    yes	        yes	    False	    78


Solution in ``Pandas`` ::

    df.groupby('ExamYear').agg({'Participated': lambda x: x.value_counts()['yes'],
                                'Passed': lambda x: sum(x == 'yes'),
                                'Employed': lambda x: sum(x),
                                'Grade': lambda x: sum(x) / len(x)})

                Participated  Passed  Employed      Grade
    ExamYear
    2007                 3       2         3        74.000000
    2008                 3       3         0        68.500000
    2009                 3       2         2        60.666667

In ``datatable`` you can nest conditions within aggregations ::

    DT = dt.Frame(df)

    DT[:, {"Participated": dt.sum(f.Participated == "yes"),
           "Passed": dt.sum(f.Passed == "yes"),
           "Employed": dt.sum(f.Employed),
           "Grade": dt.mean(f.Grade)},
       by("ExamYear")]

        ExamYear	Participated	Passed	Employed	Grade
    0	2007	        3	        2	     3	        74
    1	2008	        3	        3	     0	        68.5
    2	2009	        3	        2	     2	        60.6667

Feel free to submit a pull request on `github <https://github.com/h2oai/datatable>`__ for examples you would like to share with the community.

MISSING FUNCTIONS
-----------------

Listed below are some functions in Pandas that do not have an equivalent in datatable yet, and are likely to be implemented : 

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
    - `cumsum <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.cumsum.html>`__
    - `cummax <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.cummax.html>`__
    - `expanding <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.expanding.html>`__
    - `rolling <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.rolling.html>`__

- String functions, such as 
    - `string split <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.Series.str.split.html>`__
    - `string extract <https://pandas.pydata.org/pandas-docs/stable/generated/pandas.Series.str.extract.html>`__
    - `string replace <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.Series.str.replace.html>`__

- Custom function application, via `apply <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.apply.html>`__ and `pipe <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.pipe.html>`__
If there are any functions that you would like to see in ``datatable``, please head over to `github <https://github.com/h2oai/datatable/issues>`__ and raise a feature request.

