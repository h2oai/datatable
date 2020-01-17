
.. ref-context datatable

===============
Using datatable
===============

This section describes common functionality and commands that you can run in ``datatable``.

Create Frame
------------

You can create a Frame from a variety of sources, including ``numpy`` arrays,
``pandas`` DataFrames, raw Python objects, etc::

    import datatable as dt
    import numpy as np
    np.random.seed(1)
    dt.Frame(np.random.randn(1000000))

.. dtframe::
    :names: C0
    :types: float64
    :shape: 1000000, 1

    0,1.62435
    1,-0.611756
    2,-0.528172
    3,-1.07297
    4,0.865408
    5,-2.30154
    6,1.74481
    7,-0.761207
    8,0.319039
    9,-0.24937
    ...
    999995,0.0595784
    999996,0.140349
    999997,-0.596161
    999998,1.18604
    999999,0.313398

::

  import pandas as pd
  pf = pd.DataFrame({"A": range(1000)})
  dt.Frame(pf)

.. dtframe::
    :names: A
    :types: int64
    :shape: 1000, 1

    0,0
    1,1
    2,2
    3,3
    4,4
    5,5
    6,6
    7,7
    8,8
    9,9
    ...
    995,995
    996,996
    997,997
    998,998
    999,999

::

    dt.Frame({"n": [1, 3], "s": ["foo", "bar"]})

.. dtframe::
    :names: n, s
    :types: int8, str32
    :shape: 2, 2

    0,1,"foo"
    1,3,"bar"



Convert a Frame
---------------

Convert an existing Frame into a ``numpy`` array, a ``pandas`` DataFrame,
or a pure Python object::

   nparr = df1.to_numpy()
   pddfr = df1.to_pandas()
   pyobj = df1.to_list()

Parse Text (csv) Files
----------------------

``datatable`` provides fast and convenient parsing of text (csv) files::

   df = dt.fread("train.csv")

The ``datatable`` parser

-  Automatically detects separators, headers, column types, quoting rules,
   etc.
-  Reads from file, URL, shell, raw text, archives, glob
-  Provides multi-threaded file reading for maximum speed
-  Includes a progress indicator when reading large files
-  Reads both RFC4180-compliant and non-compliant files


Write the Frame
---------------

Write the Frame's content into a ``csv`` file (also multi-threaded)::

   df.to_csv("out.csv")

Save a Frame
------------

Save a Frame into a binary format on disk, then open it later instantly,
regardless of the data size::

   df.to_jay("out.jay")
   df2 = dt.open("out.jay")

Basic Frame Properties
----------------------

Basic Frame properties include::

    print(df.shape)   # (nrows, ncols)
    print(df.names)   # column names
    print(df.stypes)  # column types

Compute Per-Column Summary Stats
--------------------------------

Compute per-column summary stats using::

   df.sum()
   df.max()
   df.min()
   df.mean()
   df.sd()
   df.mode()
   df.nmodal()
   df.nunique()

Select Subsets of Rows/Columns
------------------------------

Select subsets of rows and/or columns using::

   df[:, "A"]         # select 1 column
   df[:10, :]         # first 10 rows
   df[::-1, "A":"D"]  # reverse rows order, columns from A to D
   df[27, 3]          # single element in row 27, column 3 (0-based)

Delete Rows/Columns
-------------------

Delete rows and or columns using::

   del df[:, "D"]     # delete column D
   del df[f.A < 0, :] # delete rows where column A has negative values

Filter Rows
-----------

Filter rows via an expression using the following. In this example, ``mean``,
``sd``, ``f`` are all symbols imported from ``datatable``::

   df[(f.x > mean(f.y) + 2.5 * sd(f.y)) | (f.x < -mean(f.y) - sd(f.y)), :]

Compute Columnar Expressions
----------------------------

Compute columnar expressions using::

   df[:, {"x": f.x, "y": f.y, "x+y": f.x + f.y, "x-y": f.x - f.y}]

Sort Columns
------------

Sort columns using::

    df.sort("A")
    df[:, :, sort(f.A)]


Perform Groupby Calculations
----------------------------

Perform groupby calculations using::

    df[:, mean(f.x), by("y")]


Append Rows/Columns
-------------------

Append rows / columns to a Frame using :meth:`Frame.cbind() <datatable.Frame.cbind>`::

    df1.cbind(df2, df3)
    df1.rbind(df4, force=True)
