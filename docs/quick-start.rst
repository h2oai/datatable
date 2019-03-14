
Quick-start
===========

Install datatable
-----------------

Let’s begin by installing the latest stable version of ``datatable``
from PyPI:

.. code:: bash

    $ pip install datatable

If this didn’t work for you, or if you want to install the bleeding edge
version of the library, please check the :doc:`Installation </install>` page.


Assuming the installation was successful, you can now import the library
in a JupyterLab notebook or in a Python console:

::

    import datatable as dt

    dt.__version__


.. raw:: html

    <div class="output-cell">
    <div class='highlight'>
      <pre>0.8.0</pre>
    </div>
  </div>



Create Frame
------------

The fundamental unit of analysis in datatable is a ``Frame``. It is the
same notion as a pandas DataFrame or SQL table: data arranged in a
two-dimensional array with rows and columns.

You can create a ``Frame`` object from a variety of data sources:

-  from a python ``list`` or ``dictionary``:

::

    import datatable as dt
    import math

    DT = dt.Frame(A=range(5), B=[1.7, 3.4, 0, None, -math.inf],
                  C = ['two','one','one','two','two'],
                     stypes={"A": dt.int64})
    DT


.. raw:: html

    <div class=output-cell>
    <div class='datatable'>
      <table class="frame">
      <thead>
        <tr class="colnames"><td class="row_index"></td><th>A</th><th>B</th><th>C</th></tr>
        <tr class="coltypes"><td class="row_index"></td>
        <td class="int" title="int64">▪▪▪▪▪▪▪▪</td>
        <td class="real" title="float64">▪▪▪▪▪▪▪▪</td><td class="str" title="str32">▪▪▪▪</td></tr>
      </thead>
      <tbody>
        <tr><td class="row_index">0</td><td>0</td><td>1.7</td><td>two</td></tr>
        <tr><td class="row_index">1</td><td>1</td><td>3.4</td><td>one</td></tr>
        <tr><td class="row_index">2</td><td>2</td><td>0</td><td>one</td></tr>
        <tr><td class="row_index">3</td><td>3</td><td><span class="na">NA</span></td><td>two</td></tr>
        <tr><td class="row_index">4</td><td>4</td><td>−inf</td><td>two</td></tr>
      </tbody>
      </table>
      <div class="footer">
        <div class="frame_dimensions">5 rows × 3 columns</div>
      </div>
    </div>
    </div>




-  from a ``numpy array``

::

    import numpy as np

    np.random.seed(1)
    DT2 = dt.Frame(np.random.randn(3))
    DT2


.. raw:: html

    <div class=output-cell>
    <div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>C0</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>1.62435</td></tr>
        <tr><td class='row_index'>1</td><td>&minus;0.611756</td></tr>
        <tr><td class='row_index'>2</td><td>&minus;0.528172</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>3 rows &times; 1 column</div>
      </div>
    </div>
    </div>




-  from a ``pandas DataFrame``

::

    import pandas as pd

    DT3 = dt.Frame(pd.DataFrame({"A": range(3)}))
    DT3




.. raw:: html

    <div class=output-cell>
      <div class='datatable'>
        <table class='frame'>
        <thead>
          <tr class='colnames'><td class='row_index'></td><th>A</th></tr>
          <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
        </thead>
        <tbody>
          <tr><td class='row_index'>0</td><td>0</td></tr>
          <tr><td class='row_index'>1</td><td>1</td></tr>
          <tr><td class='row_index'>2</td><td>2</td></tr>
        </tbody>
        </table>
        <div class='footer'>
          <div class='frame_dimensions'>3 rows &times; 1 column</div>
        </div>
      </div>
    </div>




Convert Frame
-------------

Convert an existing ``Frame`` into a numpy array, a pandas DataFrame -
requires ``pandas`` and ``numpy``:

::

    DT_numpy = DT.to_numpy()
    DT_pandas = DT.to_pandas()

A frame can also be converted into python native data structures: a
dictionary, keyed by the column names; a list of columns, where each
column is itself a list of values; or a list of rows, where each row is
a tuple of values:

::

    DT_list = DT.to_list()
    DT_dict = DT.to_dict()
    DT_tuple = DT.to_tuples()

Read data
---------

You can also load a CSV/text/Excel file, or open a previously saved
binary ``.jay`` file:

::

    DT4 = dt.fread("dataset_01.xlsx")
    DT5 = dt.fread("dataset_02.csv")
    DT6 = dt.open("data.jay")

``fread()`` function shown above is both powerful and extremely fast. It
can automatically detect parse parameters for the majority of text
files, load data from .zip archives or URLs, read Excel files, and much
more.

-  Automatically detects separators, headers, column types, quoting
   rules, etc.
-  Reads from majority of text files, load data from ``.zip`` archives or
   URLs, read Excel files, URL, shell, raw text, \* archives, glob
-  Provides multi-threaded file reading for maximum speed
-  Includes a progress indicator when reading large files
-  Reads both RFC4180-compliant and non-compliant files

Write data
----------

Write the Frame’s content into a ``.csv`` file in a multi-threaded way:

::

    DT.to_csv("out.csv")

You can also save a frame into a binary ``.jay`` file:

::

    DT.to_jay("data.jay")

Frame Properties
----------------

Investigate your Frame using descripting operators

::

    DT.shape # number of rows and columns

.. raw:: html

    <div class="output-cell">
    <div class='highlight'>
      <pre>(5, 3)</pre>
    </div>
    </div>

::

    DT.names # column names

.. raw:: html

    <div class="output-cell">
    <div class='highlight'>
      <pre>('A', 'B', 'C')</pre>
    </div>
    </div>

::

    DT.stypes # column types

.. raw:: html

    <div class="output-cell">
    <div class='highlight'>
      <pre>(stype.int64, stype.float64, stype.str32)</pre>
    </div>
    </div>


Data manipulation
-----------------

Once the data is loaded into a Frame, you may want to do certain
operations with it: extract/remove/modify subsets of the data, perform
calculations, reshape, group, join with other datasets, etc. In
datatable, the primary vehicle for all these operations is the
square-bracket notation inspired by traditional matrix indexing but
overcharged with power (this notation was pioneered in R data.table and
is the main axis of intersection between these two libraries).

In short, almost all operations with a Frame can be expressed as:

.. raw:: html

    <style>
    .sqbrak {
        display: flex;
        justify-content: center;
        margin-bottom: 16pt;
        color: #9AA;  /* whitespace color */
    }
    .sqbrak, .i, .j {
        font-family: Menlo, Consolas, Monaco, monospace;
        font-weight: bold;
    }
    .sqbrak div {
        font-size: 160%;
        margin: 0;
    }
    .dt { color: #000; }
    .i  { color: #36AA36; }
    .j  { color: #E03636; }
    .by { color: #33A; }
    .jn { color: #A3A; }
    .s  { color: #3AA; }
    </style>
    <div class="sqbrak">
      <div>
        <b class=dt>DT</b>[<b class=i>i</b>, <b class=j>j</b>, ...]
      </div>
    </div>

.. role:: raw-html(raw)
   :format: html


where :raw-html:`<b class="i">i</b>` is the row selector,
:raw-html:`<b class="j">j</b>` is the column selector, and ``...`` indicates
that additional modifiers might be added. If this looks familiar to you,
that's because it is. Exactly the same ``DT[i, j]`` notation is used in
mathematics when indexing matrices, in C/C++, in R, in pandas, in numpy, etc.
The only difference that datatable introduces is that it allows
:raw-html:`<b class="i">i</b>` to be anything that can conceivably be
interpreted as a row selector: an integer to select just one row, a slice,
a range, a list of integers, a list of slices, an expression, a boolean-valued
Frame, an integer-valued Frame, an integer numpy array, a generator, and so on.

The :raw-html:`<b class="j">j</b>` column selector is even more versatile.
In the simplest case, you can select just a single column by its index or name. But
also accepted are a list of columns, a slice, a string slice (of the form ``"A":"Z"``), a
list of booleans indicating which columns to pick, an expression, a list of
expressions, and a dictionary of expressions. (The keys will be used as new names
for the columns being selected.) The :raw-html:`<b class="j">j</b>`
expression can even be a python type (such as ``int`` or ``dt.float32``),
selecting all columns matching that type.

::

    DT[:, "A"]         # select 1 column

.. raw:: html

  <div class="output-cell">
    <div class='datatable'>
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
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>5 rows &times; 1 column</div>
      </div>
    </div>
  </div>

::

    DT[:3, :]         # first 3 rows

.. raw:: html

  <div class="output-cell">
    <div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>B</th><th>C</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='str' title='str32'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>0</td><td>1.7</td><td>two</td></tr>
        <tr><td class='row_index'>1</td><td>1</td><td>3.4</td><td>one</td></tr>
        <tr><td class='row_index'>2</td><td>2</td><td>0</td><td>one</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>3 rows &times; 3 columns</div>
      </div>
    </div>
  </div>

::

    DT[::-1, "A":"C"]  # reverse rows order, columns from A to C

.. raw:: html

  <div class="output-cell">
    <div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>B</th><th>C</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='str' title='str32'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>4</td><td>&minus;inf</td><td>two</td></tr>
        <tr><td class='row_index'>1</td><td>3</td><td><span class=na>NA</span></td><td>two</td></tr>
        <tr><td class='row_index'>2</td><td>2</td><td>0</td><td>one</td></tr>
        <tr><td class='row_index'>3</td><td>1</td><td>3.4</td><td>one</td></tr>
        <tr><td class='row_index'>4</td><td>0</td><td>1.7</td><td>two</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>5 rows &times; 3 columns</div>
      </div>
    </div>
  </div>

::

    DT[3, 2]          # single element in row 3, column 2 (0-based)

.. raw:: html

    <div class="output-cell">
    <div class='highlight'>
      <pre>'two'</pre>
    </div>
    </div>




In addition to the selector expression shown above, we support the
update and delete statements too:

.. code:: python

      DT[i, j] = r # update value in subset [i,j] with r

      del DT[i, j] # delete subset [i,j] from DT

The first expression will replace values in the subset ``[i, j]`` of
Frame ``DT`` with the values from ``r``, which could be either a
constant, or a suitably-sized Frame, or an expression that operates on
frame ``DT``.

The second expression deletes values in the subset ``[i, j]``. This is
interpreted as follows: if :raw-html:`<b class="i">i</b>` selects all rows,
then the columns given by :raw-html:`<b class="j">j</b>` are removed from the
Frame; if :raw-html:`<b class="j">j</b>` selects all columns, then the rows
given by :raw-html:`<b class="i">i</b>` are removed; if neither
:raw-html:`<b class="i">i</b>` nor :raw-html:`<b class="j">j</b>` span all
rows/columns of the Frame, then the elements in the subset ``[i, j]`` are
replaced with NAs.

::

    DT[:,"X"] = 53    # create new column and assign it value
    DT

.. raw:: html

  <div class="output-cell">
    <div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>B</th><th>C</th><th>X</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='str' title='str32'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int8'>&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>0</td><td>1.7</td><td>two</td><td>53</td></tr>
        <tr><td class='row_index'>1</td><td>1</td><td>3.4</td><td>one</td><td>53</td></tr>
        <tr><td class='row_index'>2</td><td>2</td><td>0</td><td>one</td><td>53</td></tr>
        <tr><td class='row_index'>3</td><td>3</td><td><span class=na>NA</span></td><td>two</td><td>53</td></tr>
        <tr><td class='row_index'>4</td><td>4</td><td>&minus;inf</td><td>two</td><td>53</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>5 rows &times; 4 columns</div>
      </div>
    </div>
    </div>

::

    DT[1:3,["X","Z"]] = 55  # update existing and create new column with new value
    DT

.. raw:: html

  <div class="output-cell">
    <div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>B</th><th>C</th><th>X</th><th>Z</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='str' title='str32'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int8'>&#x25AA;</td><td class='int' title='int8'>&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>0</td><td>1.7</td><td>two</td><td>53</td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>1</td><td>1</td><td>3.4</td><td>one</td><td>55</td><td>55</td></tr>
        <tr><td class='row_index'>2</td><td>2</td><td>0</td><td>one</td><td>55</td><td>55</td></tr>
        <tr><td class='row_index'>3</td><td>3</td><td><span class=na>NA</span></td><td>two</td><td>53</td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>4</td><td>4</td><td>&minus;inf</td><td>two</td><td>53</td><td><span class=na>NA</span></td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>5 rows &times; 5 columns</div>
      </div>
    </div>
    </div>

::

    del DT[:,"X"]
    DT

.. raw:: html

 <div class="output-cell">
    <div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>B</th><th>C</th><th>Z</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='str' title='str32'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int8'>&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>0</td><td>1.7</td><td>two</td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>1</td><td>1</td><td>3.4</td><td>one</td><td>55</td></tr>
        <tr><td class='row_index'>2</td><td>2</td><td>0</td><td>one</td><td>55</td></tr>
        <tr><td class='row_index'>3</td><td>3</td><td><span class=na>NA</span></td><td>two</td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>4</td><td>4</td><td>&minus;inf</td><td>two</td><td><span class=na>NA</span></td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>5 rows &times; 4 columns</div>
      </div>
    </div>
    </div>

Compute Per-Column Summary Stats
--------------------------------

Detailed description of Frame functions can be found in :doc:`Frame
documentation </api/frame>`

::

    DT.sum()

.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>B</th><th>C</th><th>Z</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='str' title='str32'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>10</td><td>&minus;inf</td><td><span class=na>NA</span></td><td>110</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>1 row &times; 4 columns</div>
      </div>
    </div>
    </div>

::

    DT.max()

.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>B</th><th>C</th><th>Z</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='str' title='str32'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int8'>&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>4</td><td>3.4</td><td><span class=na>NA</span></td><td>55</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>1 row &times; 4 columns</div>
      </div>
    </div>
    </div>

::

    DT.min()

.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>B</th><th>C</th><th>Z</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='str' title='str32'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int8'>&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>0</td><td>&minus;inf</td><td><span class=na>NA</span></td><td>55</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>1 row &times; 4 columns</div>
      </div>
    </div>
    </div>

::

    DT.mean()

.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>B</th><th>C</th><th>Z</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>2</td><td>&minus;inf</td><td><span class=na>NA</span></td><td>55</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>1 row &times; 4 columns</div>
      </div>
    </div>
    </div>

::

    DT.sd()

.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>B</th><th>C</th><th>Z</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>1.58114</td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td>0</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>1 row &times; 4 columns</div>
      </div>
    </div>
    </div>

::

    DT.mode()

.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>B</th><th>C</th><th>Z</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='str' title='str32'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int8'>&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>0</td><td>&minus;inf</td><td>two</td><td>55</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>1 row &times; 4 columns</div>
      </div>
    </div>
    </div>

::

    DT.nmodal()

.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>B</th><th>C</th><th>Z</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>1</td><td>1</td><td>3</td><td>2</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>1 row &times; 4 columns</div>
      </div>
    </div>
    </div>

::

    DT.nunique()

.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>B</th><th>C</th><th>Z</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>5</td><td>4</td><td>2</td><td>1</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>1 row &times; 4 columns</div>
      </div>
    </div>
    </div>

What the f.?
------------

You may have noticed already that we mentioned several times the
possibility of using expressions in :raw-html:`<b class="i">i</b>`
or :raw-html:`<b class="j">j</b>` and in other places.
In the simplest form an expression looks like

.. code:: python

      f.ColA

which indicates a column ``ColA`` in some Frame. Here ``f`` is a
variable that has to be imported from the datatable module. This
variable provides a convenient way to reference any column in a Frame.
In addition to the notation above, the following is also supported:

.. code:: python

      f[3]
      f["ColB"]

denoting the fourth column and the column ``ColB`` respectively.

Compute columnar expressions using:

.. code:: python

    df[:, {"x": f.x, "y": f.y, "x+y": f.x + f.y, "x-y": f.x - f.y}]

These f-expressions support arithmetic operations as well as various
mathematical and aggregate functions. For example, in order to select
the values from column ``A`` normalized to range ``[0; 1]`` we can write
the following:

::

    from datatable import f, min, max

    DT[:, {"A_normalized":(f.A - min(f.A))/(max(f.A) - min(f.A))}]

.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A_normalized</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>0</td></tr>
        <tr><td class='row_index'>1</td><td>0.25</td></tr>
        <tr><td class='row_index'>2</td><td>0.5</td></tr>
        <tr><td class='row_index'>3</td><td>0.75</td></tr>
        <tr><td class='row_index'>4</td><td>1</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>5 rows &times; 1 column</div>
      </div>
    </div>
    </div>




This is equivalent to the following SQL query:

.. code:: sql

      SELECT (f.A - MIN(f.A))/(MAX(f.A) - MIN(f.A)) FROM DT AS f

So, what exactly is ``f``? We call it a "**frame proxy**", as it becomes
a simple way to refer to the Frame that we currently operate on. More
precisely, whenever ``DT[i, j]`` is evaluated and we encounter an
``f``-expression there, that ``f`` becomes replaced with the frame
``DT``, and the columns are looked up on that Frame. The same expression
can later on be applied to a different Frame, and it will refer to the
columns in that other Frame.

At some point you may notice that that datatable also exports symbol
``g``. This ``g`` is also a frame proxy; however it already refers to
the *second* frame in the evaluated expression. This second frame
appears when you are *joining* two or more frames together (more on that
later). When that happens, symbol ``g`` is used to refer to the columns
of the joined frame.

This syntax allows do comlex filtering in user friendly way:

::

    DT[f.A > 1,"A":"B"]  # conditional selecting

.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>B</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>2</td><td>0</td></tr>
        <tr><td class='row_index'>1</td><td>3</td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>2</td><td>4</td><td>&minus;inf</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>3 rows &times; 2 columns</div>
      </div>
    </div>
    </div>

::

    from datatable import sd, mean

    DT[(f.A > mean(f.B) + 2.5 * sd(f.A)) | (f.A < -mean(f.Z) - sd(f.B)), #which rows to select
       ["A","C"]] #which columns to select


.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>C</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='str' title='str32'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>0</td><td>two</td></tr>
        <tr><td class='row_index'>1</td><td>1</td><td>one</td></tr>
        <tr><td class='row_index'>2</td><td>2</td><td>one</td></tr>
        <tr><td class='row_index'>3</td><td>3</td><td>two</td></tr>
        <tr><td class='row_index'>4</td><td>4</td><td>two</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>5 rows &times; 2 columns</div>
      </div>
    </div>
    </div>


Groupbys / joins
----------------

In the `Data Manipulation`_ section we mentioned that the ``DT[i, j, ...]`` selector
can take zero or more modifiers, which we denoted as ``...``. The available
modifiers are ``by()``, ``join()`` and ``sort()``. Thus, the full form of the
square-bracket selector is:

.. raw:: html

    <div class="sqbrak">
      <div>
        <b class=dt>DT</b>[<b class=i>i</b>, <b class=j>j</b>,
        <b class=by>by()</b>, <b class=s>sort()</b>, <b class=jn>join()</b>]
      </div>
    </div>


by(...)
~~~~~~~

This modifier splits the frame into groups by the provided column(s), and then
applies :raw-html:`<b class="i">i</b>` and :raw-html:`<b class="j">j</b>` within
each group. This mostly affects aggregator functions such as ``sum()``,
``min()`` or ``sd()``, but may also apply in other circumstances. For example,
if :raw-html:`<b class="i">i</b>` is a slice that takes the first 5 rows of a frame,
then in the presence of the ``by()`` modifier it will take the first 5 rows of
each group.

For example, in order to find the total amount of each product sold, write:


::

    from datatable import f, by, sum

    DT[:, {"sum_A":sum(f.A)}, by(f.C)]

.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>C</th><th>sum_A</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='str' title='str32'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>one</td><td>3</td></tr>
        <tr><td class='row_index'>1</td><td>two</td><td>7</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>2 rows &times; 2 columns</div>
      </div>
    </div>
    </div>




or calculate mean value by groups in colums

::

    from datatable import mean

    DT[:, {"mean_A" : mean(f.A)}, by("C")]




.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>C</th><th>mean_A</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='str' title='str32'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>one</td><td>1.5</td></tr>
        <tr><td class='row_index'>1</td><td>two</td><td>2.33333</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>2 rows &times; 2 columns</div>
      </div>
    </div>
    </div>




sort(...)
~~~~~~~~~

This modifier controls the order of the rows in the result, much like
SQL clause ``ORDER BY``. If used in conjunction with ``by()``, it will
order the rows within each group.

::

    from datatable import sort

    DT[:,:,sort(f.B)]




.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>B</th><th>C</th><th>Z</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='str' title='str32'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int8'>&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>3</td><td><span class=na>NA</span></td><td>two</td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>1</td><td>4</td><td>&minus;inf</td><td>two</td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>2</td><td>2</td><td>0</td><td>one</td><td>55</td></tr>
        <tr><td class='row_index'>3</td><td>0</td><td>1.7</td><td>two</td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>4</td><td>1</td><td>3.4</td><td>one</td><td>55</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>5 rows &times; 4 columns</div>
      </div>
    </div>
    </div>




::

    DT.sort("Z")




.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>A</th><th>B</th><th>C</th><th>Z</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='str' title='str32'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int8'>&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>0</td><td>1.7</td><td>two</td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>1</td><td>3</td><td><span class=na>NA</span></td><td>two</td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>2</td><td>4</td><td>&minus;inf</td><td>two</td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>3</td><td>1</td><td>3.4</td><td>one</td><td>55</td></tr>
        <tr><td class='row_index'>4</td><td>2</td><td>0</td><td>one</td><td>55</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>5 rows &times; 4 columns</div>
      </div>
    </div>
    </div>




join(...)
~~~~~~~~~

As the name suggests, this operator allows you to join another frame to
the current, equivalent to the SQL ``JOIN`` operator. Currently we
support only left outer joins.

In order to join frame ``X``, it must be keyed. A keyed frame is
conceptually similar to a SQL table with a unique primary key. This key
may be either a single column, or several columns:

.. code:: python

    X.key = "id"

Once a frame is keyed, it can be joined to another frame ``DT``,
provided that ``DT`` has the column(s) with the same name(s) as the key
in ``X``:

.. code:: python

    DT[:, :, join(X)]

This has the semantics of a natural left outer join. The ``X`` frame can
be considered as a dictionary, where the key column contains the keys,
and all other columns are the corresponding values. Then during the join
each row of ``DT`` will be matched against the row of ``X`` with the
same value of the key column, and if there are no such value in ``X``,
with an all-NA row.

The columns of the joined frame can be used in expressions using the
``g.`` prefix.

**NOTE:** In the future, we will expand the syntax of the join operator
to allow other kinds of joins and also to remove the limitation that
only keyed frames can be joined.

::

    DT1 = dt.Frame(product_id = [1, 1, 1, 2, 2, 2, 3, 3, 3],
                   quantity = [11, 22, 16, 45, 65, 60, 33, 37, 39],
                   stypes={"quantity": dt.int64})
    DT1




.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>product_id</th><th>quantity</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int8'>&#x25AA;</td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>1</td><td>11</td></tr>
        <tr><td class='row_index'>1</td><td>1</td><td>22</td></tr>
        <tr><td class='row_index'>2</td><td>1</td><td>16</td></tr>
        <tr><td class='row_index'>3</td><td>2</td><td>45</td></tr>
        <tr><td class='row_index'>4</td><td>2</td><td>65</td></tr>
        <tr><td class='row_index'>5</td><td>2</td><td>60</td></tr>
        <tr><td class='row_index'>6</td><td>3</td><td>33</td></tr>
        <tr><td class='row_index'>7</td><td>3</td><td>37</td></tr>
        <tr><td class='row_index'>8</td><td>3</td><td>39</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>9 rows &times; 2 columns</div>
      </div>
    </div>
    </div>

::

    DT2 = dt.Frame(product_id = [1, 2, 3], price = [1, 2, 3],
                   stypes={"price": dt.int64})
    DT2

.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>product_id</th><th>price</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int8'>&#x25AA;</td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>1</td><td>1</td></tr>
        <tr><td class='row_index'>1</td><td>2</td><td>2</td></tr>
        <tr><td class='row_index'>2</td><td>3</td><td>3</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>3 rows &times; 2 columns</div>
      </div>
    </div>
    </div>

::

    from datatable import g, join

    DT2.key = "product_id"
    DT3 = DT1[:, {"sales": f.quantity * g.price}, by(f.product_id), join(DT2)]
    DT3

.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>product_id</th><th>sales</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int8'>&#x25AA;</td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>1</td><td>11</td></tr>
        <tr><td class='row_index'>1</td><td>1</td><td>22</td></tr>
        <tr><td class='row_index'>2</td><td>1</td><td>16</td></tr>
        <tr><td class='row_index'>3</td><td>2</td><td>90</td></tr>
        <tr><td class='row_index'>4</td><td>2</td><td>130</td></tr>
        <tr><td class='row_index'>5</td><td>2</td><td>120</td></tr>
        <tr><td class='row_index'>6</td><td>3</td><td>99</td></tr>
        <tr><td class='row_index'>7</td><td>3</td><td>111</td></tr>
        <tr><td class='row_index'>8</td><td>3</td><td>117</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>9 rows &times; 2 columns</div>
      </div>
    </div>
    </div>




Append
------

Append rows / columns to a Frame using:

.. code:: python

    df1.cbind(df2, df3)
    df1.rbind(df4, force = True)

::

    DT1.cbind(DT3[:,"sales"])
    DT1




.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>product_id</th><th>quantity</th><th>sales</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int8'>&#x25AA;</td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>1</td><td>11</td><td>11</td></tr>
        <tr><td class='row_index'>1</td><td>1</td><td>22</td><td>22</td></tr>
        <tr><td class='row_index'>2</td><td>1</td><td>16</td><td>16</td></tr>
        <tr><td class='row_index'>3</td><td>2</td><td>45</td><td>90</td></tr>
        <tr><td class='row_index'>4</td><td>2</td><td>65</td><td>130</td></tr>
        <tr><td class='row_index'>5</td><td>2</td><td>60</td><td>120</td></tr>
        <tr><td class='row_index'>6</td><td>3</td><td>33</td><td>99</td></tr>
        <tr><td class='row_index'>7</td><td>3</td><td>37</td><td>111</td></tr>
        <tr><td class='row_index'>8</td><td>3</td><td>39</td><td>117</td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>9 rows &times; 3 columns</div>
      </div>
    </div>
    </div>




::

    DT1.rbind(DT, force = True)
    DT1




.. raw:: html

    <div class="output-cell"><div class='datatable'>
      <table class='frame'>
      <thead>
        <tr class='colnames'><td class='row_index'></td><th>product_id</th><th>quantity</th><th>sales</th><th>A</th><th>B</th><th>C</th><th>Z</th></tr>
        <tr class='coltypes'><td class='row_index'></td><td class='int' title='int8'>&#x25AA;</td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='real' title='float64'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='str' title='str32'>&#x25AA;&#x25AA;&#x25AA;&#x25AA;</td><td class='int' title='int8'>&#x25AA;</td></tr>
      </thead>
      <tbody>
        <tr><td class='row_index'>0</td><td>1</td><td>11</td><td>11</td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>1</td><td>1</td><td>22</td><td>22</td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>2</td><td>1</td><td>16</td><td>16</td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>3</td><td>2</td><td>45</td><td>90</td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>4</td><td>2</td><td>65</td><td>130</td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>5</td><td>2</td><td>60</td><td>120</td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>6</td><td>3</td><td>33</td><td>99</td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>7</td><td>3</td><td>37</td><td>111</td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>8</td><td>3</td><td>39</td><td>117</td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>9</td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td>0</td><td>1.7</td><td>two</td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>10</td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td>1</td><td>3.4</td><td>one</td><td>55</td></tr>
        <tr><td class='row_index'>11</td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td>2</td><td>0</td><td>one</td><td>55</td></tr>
        <tr><td class='row_index'>12</td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td>3</td><td><span class=na>NA</span></td><td>two</td><td><span class=na>NA</span></td></tr>
        <tr><td class='row_index'>13</td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td><span class=na>NA</span></td><td>4</td><td>&minus;inf</td><td>two</td><td><span class=na>NA</span></td></tr>
      </tbody>
      </table>
      <div class='footer'>
        <div class='frame_dimensions'>14 rows &times; 7 columns</div>
      </div>
    </div>
    </div>
