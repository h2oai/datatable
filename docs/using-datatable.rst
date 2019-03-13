Using ``datatable``
===================

This section describes common functionality and commands that you can run in ``datatable``.

Create Frame
------------

You can create a Frame from a variety of sources, including ``numpy`` arrays, ``pandas`` DataFrames, raw Python objects, etc:

::

  import datatable as dt
  import numpy as np
  np.random.seed(1)
  dt.Frame(np.random.randn(1000000))

.. raw:: html

  <div class=output-cell><div class='datatable'>
    <table class='frame'>
    <thead>
      <tr class='colnames'><td class='row_index'></td><th>C0</th></tr>
      <tr class='coltypes'><td class='row_index'></td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
    </thead>
    <tbody>
      <tr><td class='row_index'>0</td><td>1.62435</td></tr>
      <tr><td class='row_index'>1</td><td>&minus;0.611756</td></tr>
      <tr><td class='row_index'>2</td><td>&minus;0.528172</td></tr>
      <tr><td class='row_index'>3</td><td>&minus;1.07297</td></tr>
      <tr><td class='row_index'>4</td><td>0.865408</td></tr>
      <tr><td class='row_index'>5</td><td>&minus;2.30154</td></tr>
      <tr><td class='row_index'>6</td><td>1.74481</td></tr>
      <tr><td class='row_index'>7</td><td>&minus;0.761207</td></tr>
      <tr><td class='row_index'>8</td><td>0.319039</td></tr>
      <tr><td class='row_index'>9</td><td>&minus;0.24937</td></tr>
      <tr><td class='row_index'>&#x22EE;</td><td class='hellipsis'>&#x22EE;</td></tr>
      <tr><td class='row_index'>999,995</td><td>0.0595784</td></tr>
      <tr><td class='row_index'>999,996</td><td>0.140349</td></tr>
      <tr><td class='row_index'>999,997</td><td>&minus;0.596161</td></tr>
      <tr><td class='row_index'>999,998</td><td>1.18604</td></tr>
      <tr><td class='row_index'>999,999</td><td>0.313398</td></tr>
    </tbody>
    </table>
    <div class='footer'>
      <div class='frame_dimensions'>1,000,000 rows &times; 1 column</div>
    </div>
  </div></div>

::

  import pandas as pd
  pf = pd.DataFrame({"A": range(1000)})
  dt.Frame(pf)

.. raw:: html

  <div class=output-cell><div class='datatable'>
    <table class='frame'>
    <thead>
      <tr class='colnames'><td class='row_index'></td><th>A</th></tr>
      <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
    </thead>
    <tbody>
      <tr><td class='row_index'>0</td><td>0</td></tr>
      <tr><td class='row_index'>1</td><td>1</td></tr>
      <tr><td class='row_index'>2</td><td>2</td></tr>
      <tr><td class='row_index'>3</td><td>3</td></tr>
      <tr><td class='row_index'>4</td><td>4</td></tr>
      <tr><td class='row_index'>5</td><td>5</td></tr>
      <tr><td class='row_index'>6</td><td>6</td></tr>
      <tr><td class='row_index'>7</td><td>7</td></tr>
      <tr><td class='row_index'>8</td><td>8</td></tr>
      <tr><td class='row_index'>9</td><td>9</td></tr>
      <tr><td class='row_index'>&#x22EE;</td><td class='hellipsis'>&#x22EE;</td></tr>
      <tr><td class='row_index'>995</td><td>995</td></tr>
      <tr><td class='row_index'>996</td><td>996</td></tr>
      <tr><td class='row_index'>997</td><td>997</td></tr>
      <tr><td class='row_index'>998</td><td>998</td></tr>
      <tr><td class='row_index'>999</td><td>999</td></tr>
    </tbody>
    </table>
    <div class='footer'>
      <div class='frame_dimensions'>1000 rows &times; 1 column</div>
    </div>
  </div></div>

::

  dt.Frame({"n": [1, 3], "s": ["foo", "bar"]})

.. raw:: html

  <div class=output-cell><div class='datatable'>
    <table class='frame'>
    <thead>
      <tr class='colnames'><td class='row_index'></td><th>n</th><th>s</th></tr>
      <tr class='coltypes'><td class='row_index'></td><td class='int' title='int8'>&#x25AA;</td><td class='str' title='str32'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
    </thead>
    <tbody>
      <tr><td class='row_index'>0</td><td>1</td><td>foo</td></tr>
      <tr><td class='row_index'>1</td><td>3</td><td>bar</td></tr>
    </tbody>
    </table>
    <div class='footer'>
      <div class='frame_dimensions'>2 rows &times; 2 columns</div>
    </div>
  </div></div>



Convert a Frame
---------------

Convert an existing Frame into a ``numpy`` array, a ``pandas`` DataFrame, or a pure Python object:

::

   nparr = df1.to_numpy()
   pddfr = df1.to_pandas()
   pyobj = df1.to_list()

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

Write the Frame's content into a ``csv`` file (also multi-threaded):

::

   df.to_csv("out.csv")

Save a Frame
------------

Save a Frame into a binary format on disk, then open it later instantly, regardless of the data size:

::

   df.to_jay("out.jay")
   df2 = dt.open("out.jay")

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

   df[:, "A"]         # select 1 column
   df[:10, :]         # first 10 rows
   df[::-1, "A":"D"]  # reverse rows order, columns from A to D
   df[27, 3]          # single element in row 27, column 3 (0-based)

Delete Rows/Columns
-------------------

Delete rows and or columns using:

::

   del df[:, "D"]     # delete column D
   del df[f.A < 0, :] # delete rows where column A has negative values

Filter Rows
-----------

Filter rows via an expression using the following. In this example, ``mean``, ``sd``, ``f`` are all symbols imported from ``datatable``.

::

   df[(f.x > mean(f.y) + 2.5 * sd(f.y)) | (f.x < -mean(f.y) - sd(f.y)), :]

Compute Columnar Expressions
----------------------------

Compute columnar expressions using:

::

   df[:, {"x": f.x, "y": f.y, "x+y": f.x + f.y, "x-y": f.x - f.y}]

Sort Columns
------------

Sort columns using:

::

    df.sort("A")
    df[:, :, sort(f.A)]


Perform Groupby Calculations
----------------------------

Perform groupby calculations using:

::

    df[:, mean(f.x), by("y")]


Append Rows/Columns
-------------------

Append rows / columns to a Frame using:

::

   df1.cbind(df2, df3)
   df1.rbind(df4, force=True)
