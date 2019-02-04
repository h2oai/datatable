
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



Loading data
------------

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
:raw-html:`<b class="j">j</b>` the column selector, and ``...`` indicates
that additional modifiers might be added. If this looks familiar to you,
that's because it is. Exactly the same ``DT[i, j]`` notation is used in
mathematics when indexing matrices, in C/C++, in R, in pandas, in numpy, etc.
The only difference that datatable introduces is that it allows
:raw-html:`<b class="i">i</b>` to be anything that can conceivably be
interpreted as a row selector: an integer to select just one row, a slice,
a range, a list of integers, a list of slices, an expression, a boolean-valued
Frame, an integer-valued Frame, an integer numpy array, a generator, and so on.

The :raw-html:`<b class="j">j</b>` column selector is even more versatile.
In simplest case you can select just a single column by its index or name. But
also a list of columns, a slice, a string slice (of the form ``"A":"Z"``), a
list of booleans indicating which columns to pick, an expression, a list of
expressions, a dictionary of expressions (the keys will be used as new names
for the columns being selected). The :raw-html:`<b class="j">j</b>`
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



Groupbys / joins
----------------

In the `Data Manipulation`_ section we mentioned that ``DT[i, j, ...]`` selector
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
if :raw-html:`<b class="i">i</b>` is a slice that takes first 5 rows of a frame,
then in the presence of the ``by()`` modifier it will take the first 5 rows of
each group.

For example, in order to find the total amount of each product sold, write::

    from datatable import f, by, sum
    DT = dt.fread("transactions.csv")

    DT[:, sum(f.quantity), by(f.product_id)]


sort(...)
~~~~~~~~~

This modifier controls the order of the rows in the result, much like SQL clause
``ORDER BY``. If used in conjunction with ``by()``, it will order the rows
within each group.


join(...)
~~~~~~~~~

As the name suggests, this operator allows you to join another frame to the
current, equivalent to the SQL ``JOIN`` operator. Currently we support only
left outer joins.

In order to join frame ``X``, it must be keyed. A keyed frame is conceptually
similar to a SQL table with a unique primary key. This key may be either a
single column, or several columns::

    X.key = "id"

Once a frame is keyed, it can be joined to another frame ``DT``, provided that
``DT`` has the column(s) with the same name(s) as the key in ``X``::

    DT[:, :, join(X)]

This has the semantics of a natural left outer join. The ``X`` frame can be
considered as a dictionary, where the key column contains the keys, and all
other columns are the corresponding values. Then during the join each row of
``DT`` will be matched against the row of ``X`` with the same value of the
key column, and if there are no such value in ``X``, with an all-NA row.

The columns of the joined frame can be used in expressions using the ``g.``
prefix, for example::

    DT[:, sum(f.quantity * g.price), join(products)]

.. note:: In the future we will expand the syntax of the join operator to
          allow other kinds of joins, and also to remove the limitation that
          only keyed frames can be joined.



Offloading data
---------------

Just as our work has started with loading some data into datatable, eventually
you will want to do the opposite: store or move the data somewhere else. We
support multiple mechanisms for this.

First, the data can be converted into a pandas DataFrame or into a numpy array
(obviously, you have to have pandas or numpy libraries installed)::

    DT.to_pandas()
    DT.to_numpy()

A frame can also be converted into python native data structures: a dictionary,
keyed by the column names; a list of columns, where each column is itself a
list of values; or a list of rows, where each row is a tuple of values::

    DT.to_dict()
    DT.to_list()
    DT.to_tuples()

You can also save a frame into a CSV file, or into a binary .jay file::

    DT.to_csv("out.csv")
    DT.save("data.jay")
