
Getting started
===============

Install datatable
-----------------

Let's begin by installing the latest stable version of ``datatable`` from PyPI:

.. code-block:: bash

    $ pip install datatable

If this didn't work for you, or if you want to install the bleeding edge
version of the library, please check the :doc:`Installation </install>` page.

Assuming the installation was successful, you can now import the library in
a JupyterLab notebook, or in Python console:

::

  import datatable as dt
  print(dt.__version__)

.. raw:: html

  <div class="output-cell"><div class='highlight'>
    <pre>0.7.0</pre>
  </div></div>



Load the data
----------------

The fundamental unit of analysis in datatable is a data ``Frame``. It is the
same notion as a pandas DataFrame, or SQL table: data arranged in
two-dimensional array with rows and columns.

You can create a ``Frame`` object from a variety of data sources: from a python
list or dictionary, from numpy array, or from pandas DataFrame.

::

  DT1 = dt.Frame(A=range(5), B=[1.7, 3.4, 0, None, -math.inf],
                 stypes={"A": dt.int64})
  DT2 = dt.Frame(pandas_dataframe)
  DT3 = dt.Frame(numpy_array)

You can also load a CSV/text/Excel file, or open a previously saved binary
``.jay`` file:

::

  DT4 = dt.fread("~/Downloads/dataset_01.csv")
  DT5 = dt.open("data.jay")

The ``fread()`` function shown above is both powerful and extremely fast. It can
automatically detect parse parameters for the majority of text files, load data
from .zip archives or URLs, read Excel files, and much more.



Data manipulation
-----------------

Once the data is loaded into a Frame, you may want to do certain operations with
it: extract/remove/modify subsets of the data, perform calculations, reshape,
group, join with other datasets, etc. In datatable, the primary vehicle for all
these operations is the square-bracket notation inspired by traditional matrix
indexing but overcharged with power (this notation was pioneered in R data.table
and is the main axis of intersection between these two libraries).

In short, almost all operations with a Frame can be expressed as

.. raw:: html

    <style>
    .sqbrak {
        display: flex;
        justify-content: center;
        margin-bottom: 16pt;
    }
    .sqbrak, .i, .j {
        font-family: Menlo, Consolas, Monaco, monospace;
        font-weight: bold;
    }
    .sqbrak div {
        font-size: 160%;
        margin: 0;
    }
    .x { color: #9AA; }
    .i { color: #36AA36; }
    .j { color: #E03636; }
    </style>
    <div class="sqbrak">
      <div>
        DT<span class=x>[</span><span class=i>i</span><span class=x>,</span>
        <span class=j>j</span><span class=x>, ...]</span></div>
    </div>

.. role:: raw-html(raw)
   :format: html

where :raw-html:`<span class="i">i</span>` is the row selector,
:raw-html:`<span class="j">j</span>` the column selector, and ``...``
indicates that additional modifiers might be added. If this looks familiar to
you, that's because it is. Exactly the same ``DT[i, j]`` notation is used in
mathematics when indexing matrices, in C/C++, in R, in pandas, in numpy, etc.
The only difference that datatable introduces is that it allows
:raw-html:`<span class="i">i</span>` to be anything that can conceivably be
interpreted as a row selector: an integer to select just one row, a slice,
a range, a list of integers, a list of slices, an expression, a boolean-valued
Frame, an integer-valued Frame, an integer numpy array, a generator, and so on.

The :raw-html:`<span class="j">j</span>` column selector is even more versatile.
In simplest case you can select just a single column by its index or name. But
also a list of columns, a slice, a string slice (of the form ``"A":"Z"``), a
list of booleans indicating which columns to pick, an expression, a list of
expressions, a dictionary of expressions (the keys will be used as new names
for the columns being selected). The :raw-html:`<span class="j">j</span>`
expression can even be a python type (such as ``int`` or ``dt.float32``),
selecting all columns matching that type.

In addition to the selector expression shown above, we support the update and
delete statements too:

::

  DT[i, j] = r
  del DT[i, j]

The first expression will replace values in the subset ``[i, j]`` of Frame
``DT`` with the values from ``r``, which could be either a constant, or a
suitably-sized Frame, or an expression that operates on frame ``DT``.

The second expression deletes values in the subset ``[i, j]``. This is
interpreted as follows: if ``i`` selects all rows, then the columns given by
``j`` are removed from the Frame; if ``j`` selects all columns, then the rows
given by ``i`` are removed; if neither ``i`` nor ``j`` span all rows/columns
of the Frame, then the elements in the subset ``[i, j]`` are replaced with
NAs.



What the f.?
------------

You may have noticed already that we mentioned several times the possibility
of using expressions in :raw-html:`<span class="i">i</span>` or
:raw-html:`<span class="j">j</span>`, and in other places. In the simplest form
an expression looks like

::

  f.ColA

which indicates a column ``ColA`` in some Frame. Here ``f`` is a variable that
has to be imported from datatable module. This variable provides a convenient
way to reference any column in a Frame. In addition to the notation above, the
following is also supported:

::

  f[3]
  f["ColB"]

denoting the fourth column and the column ``ColB`` respectively.

These f-expression support arithmetic operations, various mathematical and
aggregate functions. For example, in order to select the values from column
``A`` normalized to range ``[0; 1]`` we can write the following:

::

  from datatable import f, min, max
  DT[:, (f.A - min(f.A))/(max(f.A) - min(f.A))]

This is equivalent to the following SQL query:

.. code:: SQL

  SELECT (f.A - MIN(f.A))/(MAX(f.A) - MIN(f.A)) FROM DT AS f

So, what exactly is ``f``? We call it a "frame proxy", as it becomes a
simple way to refer to the Frame that we currently operate on. More precisely,
whenever ``DT[i, j]`` is evaluated and we encounter an ``f``-expression there,
that ``f`` becomes replaced with the frame ``DT`` and the columns are looked
up on that Frame. The same expression can later on be applied to a different
Frame, and it will refer to the columns in that other Frame.

At some point you may notice that that datatable also exports symbol ``g``. This
``g`` is also a frame proxy, however it already refers to the *second* frame in
the evaluated expression. This second frame appears when you are *joining* two
or more frames together (more on that later). When that happens, symbol ``g`` is
used to refer to the columns of the joined frame.



