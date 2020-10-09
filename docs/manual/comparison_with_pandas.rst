.. py:currentmodule:: datatable


.. raw:: html

    <style>
    .wy-nav-content { max-width: 100%; }
    </style>


Comparison with Pandas
=======================
A lot of potential ``datatable`` users are likely to have some familiarity with `pandas <https://pandas.pydata.org/pandas-docs/stable/index.html>`__; as such, this page provides some examples of how various ``pandas`` operations can be performed within ``datatable``. ``datatable`` emphasizes speed and big data support (an area that ``pandas`` struggles with); it also has an expressive and concise syntax, which makes ``datatable`` also useful for small datasets.

.. note:: In ``pandas``, there are two fundamental data structures, `Series <https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.Series.html#pandas.Series>`__ and `DataFrame <https://pandas.pydata.org/pandas-docs/stable/user_guide/dsintro.html#dataframe>`__. In ``datatable``, there is only one fundamental data structure - the `Frame <https://datatable.readthedocs.io/en/latest/api/frame.html#datatable-frame>`__. Most of the comparisons will be between a ``pandas`` DataFrame and ``datatable`` Frame.

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
``df = df.rename(columns={"A":"col_A"})``                    | ``DT[:, update(col_A = f.A)]`` or
                                                             | ``DT.names = {"A" : "col_A"}``

Rename multiple columns
``df = df.rename(columns={"A":"col_A", "B":"col_B"})``      ``DT[:, update(col_A = f.A, col_B = f.B)]``
=======================================================  ===============================================================

Delete Columns
--------------

=======================================================  ===============================================================
        Pandas                                              datatable
=======================================================  ===============================================================
Delete a column
``df = df.drop('B', axis=1)``                                 ``DT[:, f[:].remove(f.B)]``
Same as above
``del df['B']``                                                ``del DT['B']``

Remove multiple columns
``df = df.drop(['B', 'C'], axis=1)``                         | ``DT[:, f[:].remove([f.B, f.C])]`` or
                                                             | ``del DT[: , ['B', 'C']]``
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

In ``datatable``, transformations occur within the ``j`` section :

.. code-block:: python

    DT[:, f[:].extend({"min_b": dt.min(f.b)}), by("a")]

    	a	b	c	min_b
    0	1	2	3	2
    1	1	20	30	2
    2	1	2	33	2
    3	2	30	50	4
    4	2	4	50	4

Note the results above is sorted by the grouping column. If you want the data to maintain the same shape as the source data, then :func:`update()` is a better option :


.. code-block:: python

    DT[:, update(min_b=dt.min(f.b)), by("a")]

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

In ``datatable``, there is no need to create a temporary column; you can easily nest your computations in the ``j`` section :

.. code-block:: python

    # datatable

    DT1[:, {"revenue": dt.sum(f.item_price * f.item_sold)}, "shop"]

        shop	revenue
    0	A	235
    1	B	1842

You can learn more about the :func:`by()` function  at the `Grouping with by <https://datatable.readthedocs.io/en/latest/manual/groupby_examples.html>`__ documentation, as well as the API : :func:`by()`.


.. note:: Pandas allows custom functions via the apply method. ``datatable`` does not yet support custom functions.

.. note:: Also missing in ``datatable`` but available in Pandas are cumulative functions (cumsum, cumprod, ...), as well as windows functions (rolling, expanding, ...)

