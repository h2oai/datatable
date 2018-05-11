Using ``datatable``
===================

This section describes common functionality and commands that you can run in ``datatable``.

Create Frame
------------

You can create a Frame from a variety of sources, including ``numpy`` arrays, ``pandas`` DataFrames, raw python objects, etc:
   
::

   df1 = dt.Frame(np.random.randn(1000000))
   df2 = dt.Frame(pd.DataFrame({"A": range(1000)}))
   df3 = dt.Frame({"n": [1, 3], "s": ["foo", "bar"]})``

Convert a Frame
---------------

Convert an existing Frame into a ``numpy`` array, a ``pandas`` DataFrame, or a pure-python object:

::

   nparr = df1.tonumpy()
   pddfr = df1.topandas()
   pyobj = df1.topython()

Parse Text (csv) Files
----------------------

``datatable`` provides fast and convenient parsing of text (csv) files:

::

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

Write Frame's content into a ``csv`` file (also multi-threaded):

::

   df.to_csv("out.csv")

Save Frame
----------

Save a Frame into a binary format on disk, then open it later instantly, regardless of the data size:

::

   df.save("out.nff")
   df2 = dt.open("out.nff")

Basic Frame Properties
----------------------

Basic Frame properties include:

::

    print(df.shape)   # (nrows, ncols)   
    print(df.names)   # column names   
    print(df.stypes)  # column types

Compute Per-Column Summary Stats
--------------------------------

Compute per-column summary stats using:

::

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

Select subsets of rows and/or columns using:

::

   df["A"]            # select 1 column
   df[:10, :]         # first 10 rows
   df[::-1, "A":"D"]  # reverse rows order, columns from A to D
   df[27, 3]          # single element in row 27, column 3 (0-based)

Delete Rows/Columns
-------------------

Delete rows and or columns using:

::

   del df["D"]        # delete column D
   del df[f.A < 0, :] # delete rows where column A has negative values

Filter Rows
-----------

Filter rows via an expression using the following. In this example, ``mean``, ``sd``, ``f`` are all symbols imported from ``datatable``.

::

   df[(f.x > mean(f.y) + 2.5 * sd(f.y)) | (f.x < -mean(f.y) - sd(f.y)), :]

Compute Columnar Expressions
----------------------------

-  Compute columnar expressions using:

::

   df[:, {"x": f.x, "y": f.y, "x+y": f.x + f.y, "x-y": f.x - f.y}]

Sort Columns
------------

Sort columns using: 

::

    df.sort("A")

Perform Groupby Calculations
----------------------------

Perform groupby calculations using:

::

    df(select=mean(f.x), groupby="y")

Append Rows/Columns
-------------------

Append rows / columns to a Frame using:

::

   df1.cbind(df2, df3)
   df1.rbind(df4, force=True)
