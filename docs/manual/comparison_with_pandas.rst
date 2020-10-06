.. py:currentmodule:: datatable

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

.. note:: In ``datatable``, the :func:`update()` method is in-place, reassigment to the Frame ``DT`` is not required.


Sorting
-------