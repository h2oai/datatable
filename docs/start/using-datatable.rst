
===============
Using datatable
===============

This section describes common functionality and commands supported by
``datatable``.


Create a Frame
--------------

:class:`Frame <dt.Frame>` can be :meth:`created <dt.Frame.__init__>`
from a variety of sources. For instance, from a ``numpy`` array::

    >>> import datatable as dt
    >>> import numpy as np
    >>> np.random.seed(1)
    >>> NP = np.random.randn(1000000)
    >>> dt.Frame(NP)
           |         C0
           |    float64
    ------ + ----------
         0 |  1.62435
         1 | -0.611756
         2 | -0.528172
         3 | -1.07297
         4 |  0.865408
         5 | -2.30154
         6 |  1.74481
         7 | -0.761207
         8 |  0.319039
         9 | -0.24937
        10 |  1.46211
        11 | -2.06014
        12 | -0.322417
        13 | -0.384054
        14 |  1.13377
         … |          …
    999995 |  0.0595784
    999996 |  0.140349
    999997 | -0.596161
    999998 |  1.18604
    999999 |  0.313398

    [1000000 rows x 1 column]

From ``pandas`` DataFrame::

    >>> import pandas as pd
    >>> PD = pd.DataFrame({"A": range(1000)})
    >>> dt.Frame(PD)
        |     A
        | int64
    --- + -----
      0 |   0
      1 |   1
      2 |   2
      3 |   3
      4 |   4
      5 |   5
      6 |   6
      7 |   7
      8 |   8
      9 |   9
     10 |  10
     11 |  11
     12 |  12
     13 |  13
     14 |  14
      … |   …
    995 | 995
    996 | 996
    997 | 997
    998 | 998
    999 | 999

    [1000 rows x 1 column]

Or from a raw Python object::

    >>> dt.Frame({"n": [1, 3], "s": ["foo", "bar"]})
       |     n s
       | int32 str32
    -- + ----- -----
     0 |     1 foo
     1 |     3 bar

    [2 rows x 2 columns]


Convert a Frame
---------------

An existing frame ``DT`` can be
:ref:`converted <convert-into-other-formats>` to other formats,
including ``numpy`` arrays, ``pandas`` DataFrames, Python objects,
and CSV files::

    >>> NP = DT.to_numpy()
    >>> PD = DT.to_pandas()
    >>> PY = DT.to_list()
    >>> DT.to_csv("out.csv")


Parse CSV Files
---------------

``datatable`` provides fast and convenient way to parse CSV files
via :func:`dt.fread()` function::

    >>> DT = dt.fread("in.csv")

The ``datatable`` parser

-  Automatically detects separators, headers, column types, quoting rules,
   etc.
-  Reads from file, URL, shell, raw text, archives, glob
-  Provides multi-threaded file reading for maximum speed
-  Includes a progress indicator when reading large files
-  Reads both RFC4180-compliant and non-compliant files


Save a Frame
------------

Save a Frame into a binary JAY format on disk, later open it instantly,
regardless of the data size::

    >>> DT.to_jay("out.jay")
    >>> DT2 = dt.open("out.jay")


Basic Frame Properties
----------------------

Basic Frame properties include::

    >>> DT.shape   # (nrows, ncols)
    >>> DT.names   # column names
    >>> DT.types   # column types


Compute Frame Statistics
------------------------

Compute per-column summary statistics using the following
Frame's methods::

    >>> DT.sum()
    >>> DT.max()
    >>> DT.min()
    >>> DT.mean()
    >>> DT.sd()
    >>> DT.mode()
    >>> DT.nmodal()
    >>> DT.nunique()


Select Subsets of Rows or Columns
---------------------------------

Select subsets of rows or columns by using
:meth:`DT[i,j,...] <dt.Frame.__getitem__>` selector::

    >>> DT[:, "A"]         # select 1 column
    >>> DT[:10, :]         # first 10 rows
    >>> DT[::-1, "A":"D"]  # reverse rows order, columns from A to D
    >>> DT[27, 3]          # single element in row 27, column 3 (0-based)


Delete Rows or Columns
----------------------

Delete rows or columns with ``del``::

    >>> del DT[:, "D"]     # delete column D
    >>> del DT[f.A < 0, :] # delete rows where column A has negative values


Filter Rows
-----------

Filter rows via an :ref:`f-expression <f-expressions>`::

    >>> from datatable import mean, sd, f
    >>> DT[(f.A > mean(f.B) + 2.5 * sd(f.B)) | (f.A < -mean(f.B) - sd(f.B)), :]


Compute Columnar Expressions
----------------------------

:ref:`f-expressions` could also be used to compute columnar expressions::

    >>> DT[:, {"A": f.A, "B": f.B, "A+B": f.A + f.B, "A-B": f.A - f.B}]


Sort Columns
------------

Sort columns via :meth:`Frame.sort() <dt.Frame.sort>` or via :func:`dt.sort()`::

    >>> DT.sort("A")
    >>> DT[:, :, dt.sort(f.A)]


Perform Groupby Calculations
----------------------------

Perform groupby calculations using::

    >>> DT[:, mean(f.A), dt.by("B")]


Append Rows or Columns
----------------------

Append rows to the existing frame by using
:meth:`Frame.rbind() <datatable.Frame.rbind>`::

    >>> DT.rbind(DT2)

Append columns by using :meth:`Frame.cbind() <datatable.Frame.cbind>`::

    >>> DT.cbind(DT2)

