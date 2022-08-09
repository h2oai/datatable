
Comparison with R's data.table
==============================

``datatable`` is closely related to R's `data.table`_ and attempts to mimic
its API; however, there are differences due to language constraints.

This page shows how to perform similar basic operations in R's ``data.table``
versus ``datatable``.


Subsetting Rows
---------------
The examples used here are from the `examples`_ data in R's ``data.table``.

.. code-block:: R

    library(data.table)

    DT = data.table(x=rep(c("b","a","c"),each=3),
                    y=c(1,3,6), v=1:9)


.. code-block:: python

    >>> from datatable import dt, f, g, by, update, join, sort
    >>>
    >>> DT = dt.Frame(x = ["b"]*3 + ["a"]*3 + ["c"]*3,
    ...               y = [1, 3, 6] * 3,
    ...               v = range(1, 10))
    >>> DT
       | x          y      v
       | str32  int32  int32
    -- + -----  -----  -----
     0 | b          1      1
     1 | b          3      2
     2 | b          6      3
     3 | a          1      4
     4 | a          3      5
     5 | a          6      6
     6 | c          1      7
     7 | c          3      8
     8 | c          6      9
    [9 rows x 3 columns]


=================================================  ============================================ =====================================
Action                                                data.table                                   datatable
=================================================  ============================================ =====================================
Select 2nd row                                       ``DT[2]``                                    ``DT[1, :]``
Select 2nd and 3rd row                               ``DT[2:3]``                                  ``DT[1:3, :]``
Select 3rd and 2nd row                               ``DT[3:2]``                                  ``DT[[2,1], :]``
Select 2nd and 5th rows                              ``DT[c(2,5)]``                               ``DT[[1,4], :]``
Select all rows from 2nd to 5th                      ``DT[2:5]``                                  ``DT[[1:5, :]``
Select rows in reverse from 5th to the 1st           ``DT[5:1]``                                  ``DT[4::-1, :]``
Select the last row                                  ``DT[.N]``                                   ``DT[-1, :]``
All rows where ``y > 2``                             ``DT[y>2]``                                  ``DT[f.y>2, :]``
Compound logical expressions                         ``DT[y>2 & v>5]``                            ``DT[(f.y>2) & (f.v>5), :]``
All rows other than rows 2,3,4                       ``DT[!2:4]`` or ``DT[-(2:4)]``               ``DT[[0, slice(4, None)], :]``
Sort by column ``x``, ascending                      ``DT[order(x), ]``                           | ``DT.sort("x")`` or
                                                                                                  | ``DT[:, :, sort("x")]``
Sort by column ``x``, descending                     ``DT[order(-x)]``                            | ``DT.sort(-f.x)`` or
                                                                                                  | ``DT[:, :, sort(-f.x)]``
Sort by column ``x`` ascending, ``y`` descending     ``DT[order(x, -y)]``                         | ``DT.sort(x, -f.y)`` or
                                                                                                  | ``DT[:, :, sort(f.x, -f.y)]``
=================================================  ============================================ =====================================

.. note::

    Note the use of the ``f`` symbol when performing computations or
    sorting in descending order. You can read more about :ref:`f-expressions`.

.. note::

    In R, ``DT[2]`` would mean **2nd row**, whereas in python ``DT[2]`` would
    select the **3rd column**.


In ``data.table``, when selecting rows you do not need to indicate the columns.
So, something like the code below works fine:

.. code-block:: R

    # data.table
    DT[y==3]

       x y v
    1: b 3 2
    2: a 3 5
    3: c 3 8

In ``datatable``, however, when selecting rows there has to be a column
selector, or you get an error::

    >>> DT[f.y == 3]
    TypeError: Column selector must be an integer or a string, not <class 'datatable.FExpr'>

The code above fails because ``datatable`` only allows single-column selection
using the style above::

    >>> DT['y']
       |     y
       | int32
    -- + -----
     0 |     1
     1 |     3
     2 |     6
     3 |     1
     4 |     3
     5 |     6
     6 |     1
     7 |     3
     8 |     6
    [9 rows x 1 column]

As such, when ``datatable`` sees an :ref:`f-expressions`, it thinks you are
selecting a column, and appropriately errors out.


Since, in this case, we are selecting all columns, we can use either a colon
(``:``) or the Ellipsis symbol(``...``)::

    >>> DT[f.y==3, :]
    >>> DT[f.y==3, ...]



Selecting columns
-----------------

============================================= =============================================== ==============================================
Action                                                data.table                                   datatable
============================================= =============================================== ==============================================
Select column ``v``                             ``DT[, .(v)]``                                 ``DT[:, 'v']`` or ``DT['v']``
Select multiple columns                         ``DT[, .(x,v)]``                               ``DT[:, ['x', 'v']]``
Rename and select column                        ``DT[, .(m = x)]``                             ``DT[:, {"m" : f.x}]``
Sum column ``v`` and rename as ``sv``           ``DT[, .(sv=sum(v))]``                         ``DT[:, {"sv": dt.sum(f.v)}]``
Return two columns, ``v`` and ``v`` doubled     ``DT[, .(v, v*2)]``                            ``DT[:, [f.v, f.v*2]]``
Select the second column                        ``DT[, 2]``                                    ``DT[:, 1]`` or ``DT[1]``
Select last column                              ``DT[, ncol(DT), with=FALSE]``                 ``DT[:, -1]``
Select columns ``x`` through ``y``              ``DT[, .SD, .SDcols=x:y]``                     ``DT[:, f["x":"y"]]``  or ``DT[:, 'x':'y']``
Exclude columns ``x`` and ``y``                 ``DT[ , .SD, .SDcols = !x:y]``                 | ``DT[:, [name not in ("x","y")``
                                                                                               |          ``for name in DT.names]]`` or
                                                                                               | ``DT[:, f[:].remove(f['x':'y'])]``
Select columns that start with ``x`` or ``v``   ``DT[ , .SD, .SDcols = patterns('^[xv]')]``    | ``DT[:, [name.startswith(("x", "v"))``
                                                                                               |          ``for name in DT.names]]``
============================================= =============================================== ==============================================

In ``data.table``, you can select a column by using a variable name with the
double dots prefix:

.. code-block:: R

    col = 'v'
    DT[, ..col]

In ``datatable``, you do not need the prefix::

    >>> col = 'v'
    >>> DT[:, col]  # or DT[col]
       |       v
       | float64
    -- + -------
     0 | 1
     1 | 1.41421
     2 | 1.73205
     3 | 2
     4 | 2.23607
     5 | 2.44949
     6 | 2.64575
     7 | 2.82843
     8 | 3
    [9 rows x 1 column]

If the column names are stored in a character vector, the double dots prefix
also works:

.. code-block:: R

    cols = c('v', 'y')
    DT[, ..cols]

In ``datatable``, you can store the list/tuple of column names in a variable

.. code-block:: python

    >>> cols = ['v', 'y']
    >>> DT[:, cols]
       |       v        y
       | float64  float64
    -- + -------  -------
     0 | 1        1
     1 | 1.41421  1.73205
     2 | 1.73205  2.44949
     3 | 2        1
     4 | 2.23607  1.73205
     5 | 2.44949  2.44949
     6 | 2.64575  1
     7 | 2.82843  1.73205
     8 | 3        2.44949
    [9 rows x 2 columns]



Subset rows and Select/Aggregate
--------------------------------

======================================           ==========================================          ==============================================
Action                                                data.table                                         datatable
======================================           ==========================================          ==============================================
Sum column ``v`` over rows 2 and 3                  ``DT[2:3, .(sum(v))]``                            ``DT[1:3, dt.sum(f.v)]``
Same as above, new column name                      ``DT[2:3, .(sv=sum(v))]``                         ``DT[1:3, {"sv": dt.sum(f.v)}]``
Filter in ``i`` and aggregate in ``j``              ``DT[x=="b", .(sum(v*y))]``                       ``DT[f.x=="b", dt.sum(f.v * f.y)]``
Same as above, return as scalar                     ``DT[x=="b", sum(v*y)]``                          ``DT[f.x=="b", dt.sum(f.v * f.y)][0, 0]``
======================================           ==========================================          ==============================================

In R indexing starts at 1 and when slicing, the first and the last items are
both included. However, in Python, indexing starts at 0, and when slicing all
items except the last are included.

Some ``SD`` (Subset of Data) operations can be replicated in ``datatable``

Aggregate several columns
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: R

    DT[, lapply(.SD, mean), .SDcols = c("y","v")]

              y v
    1: 3.333333 5

.. code-block:: python

    >>> DT[:, dt.mean([f.y,f.v])]
       |       y        v
       | float64  float64
    -- + -------  -------
     0 | 3.33333        5
    [1 row x 2 columns]


Modify columns using a condition
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: R

    DT[, .SD - 1, .SDcols = is.numeric]

       y v
    1: 0 0
    2: 2 1
    3: 5 2
    4: 0 3
    5: 2 4
    6: 5 5
    7: 0 6
    8: 2 7
    9: 5 8

.. code-block:: python

    >>> DT[:, f[int] - 1]
       |    C0     C1
       | int32  int32
    -- + -----  -----
     0 |     0      0
     1 |     2      1
     2 |     5      2
     3 |     0      3
     4 |     2      4
     5 |     5      5
     6 |     0      6
     7 |     2      7
     8 |     5      8
    [9 rows x 2 columns]

Modify several columns and keep others unchanged
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: R

    DT[, c("y", "v") := lapply(.SD, sqrt),
         .SDcols = c("y", "v")]

       x        y        v
    1: b 1.000000 1.000000
    2: b 1.732051 1.414214
    3: b 2.449490 1.732051
    4: a 1.000000 2.000000
    5: a 1.732051 2.236068
    6: a 2.449490 2.449490
    7: c 1.000000 2.645751
    8: c 1.732051 2.828427
    9: c 2.449490 3.000000

.. code-block:: python

    >>> # there is a square root function the datatable math module
    >>> DT[:, update(**{name:f[name]**0.5 for name in ("y","v")})]
    >>> DT
       | x            y        v
       | str32  float64  float64
    -- + -----  -------  -------
     0 | b      1        1
     1 | b      1.73205  1.41421
     2 | b      2.44949  1.73205
     3 | a      1        2
     4 | a      1.73205  2.23607
     5 | a      2.44949  2.44949
     6 | c      1        2.64575
     7 | c      1.73205  2.82843
     8 | c      2.44949  3
    [9 rows x 3 columns]



Grouping with :func:`by()`
--------------------------

===========================================================   ==============================================   ============================================================
Action                                                            data.table                                         datatable
===========================================================   ==============================================   ============================================================
Get the sum of column ``v`` grouped by column ``x``               ``DT[, sum(v), by=x]``                            ``DT[:, dt.sum(f.v), by('x')]``

Get sum of ``v`` where ``x != a``                                 ``DT[x!="a", sum(v), by=x]``                      ``DT[f.x!="a", :][:, dt.sum(f.v), by("x")]``

Number of rows per group                                          ``DT[, .N, by=x]``                                ``DT[:, dt.count(), by("x")]``

Select first row of ``y`` and ``v`` for each group in ``x``       ``DT[, .SD[1], by=x]``                            ``DT[0, :, by('x')]``

Get row count and sum columns ``v`` and ``y`` by group            ``DT[, c(.N, lapply(.SD, sum)), by=x]``           ``DT[:, [dt.count(), dt.sum(f[:])], by("x")]``

Expressions in :func:`by`                                        ``DT[, sum(v), by=.(y%%2)]``                       ``DT[:, dt.sum(f.v), by(f.y%2)]``

Get row per group where column ``v`` is minimum                  ``DT[, .SD[which.min(v)], by=x]``                  ``DT[0, f[:], by("x"), dt.sort(f.v)]``

First 2 rows of each group                                      ``DT[, head(.SD,2), by=x]``                         ``DT[:2, :, by("x")]``

Last 2 rows of each group                                       ``DT[, tail(.SD,2), by=x]``                         ``DT[-2:, :, by("x")]``
===========================================================   ==============================================   ============================================================

In R's ``data.table``, the order of the groupings is preserved; in
``datatable``, the returned dataframe is sorted on the grouping column.
``DT[, sum(v), keyby=x]`` in data.table returns a dataframe ordered by
column ``x``.

In ``data.table``, ``i`` is executed before the grouping, while in
``datatable``, ``i`` is executed after the grouping.

Also, in ``datatable``, :ref:`f-expressions` in the ``i`` section of a
groupby is not yet implemented, hence the chaining method to get the sum of
column ``v`` where ``x!=a``.

Multiple aggregations within a group can be executed in R's ``data.table``
with the syntax below:

.. code-block:: R

    DT[, list(MySum=sum(v),
              MyMin=min(v),
              MyMax=max(v)),
       by=.(x, y%%2)]

The same can be replicated in ``datatable`` by using a dictionary::

    >>> DT[:, {'MySum': dt.sum(f.v),
    ...        'MyMin': dt.min(f.v),
    ...        'MyMax': dt.max(f.v)},
    ...    by(f.x, f.y%2)]



Add/Update/Delete Columns
-------------------------

============================================ =========================================================  ============================================================
Action                                                       data.table                                         datatable
============================================ =========================================================  ============================================================
Add new column                                ``DT[, z:=42L]``                                          | ``DT[:, update(z=42)]`` or
                                                                                                        | ``DT['z'] = 42`` or
                                                                                                        | ``DT[:, 'z'] = 42`` or
                                                                                                        | ``DT = DT[:, f[:].extend({"z":42})]``
Add multiple columns                          ``DT[, c('sv','mv') := .(sum(v), "X")]``                  | ``DT[:, update(sv = dt.sum(f.v), mv = "X")]`` or
                                                                                                        | ``DT[:, f[:].extend({"sv": dt.sum(f.v), "mv": "X"})]``
Remove column                                 ``DT[, z:=NULL]``                                         | ``del DT['z']`` or
                                                                                                        | ``del DT[:, 'z']`` or
                                                                                                        | ``DT = DT[:, f[:].remove(f.z)]``
Subassign to existing ``v`` column            ``DT["a", v:=42L, on="x"]``                               | ``DT[f.x=="a", update(v=42)]`` or
                                                                                                        | ``DT[f.x=="a", 'v'] = 42``
Subassign to new column (NA padded)           ``DT["b", v2:=84L, on="x"]``                              | ``DT[f.x=="b", update(v2=84)]`` or
                                                                                                        | ``DT[f.x=='b', 'v2'] = 84``
Add new column, assigning values group-wise   ``DT[, m:=mean(v), by=x]``                                | ``DT[:, update(m=dt.mean(f.v)), by("x")]``
============================================ =========================================================  ============================================================

In ``data.table``, you can create a new column with a variable

.. code-block:: R

    col = 'rar'
    DT[, ..col:=4242]

Similar operation for the above in ``datatable``::

    >>> col = 'rar'
    >>> DT[col] = 4242
    >>> # or DT[:, update(col = 4242)]

.. note::

    The :func:`update` function, as well as the ``del`` operator operate
    in-place; there is no need for reassignment. Another advantage of the
    :func:`update` method is that the row order of the dataframe is not
    changed, even in a groupby; this comes in handy in a lot of
    transformation operations.


Joins
-----

At the moment, only the left outer join is implemented in ``datatable``.
Another aspect is that the dataframe being joined must be keyed, the column or
columns to be keyed must not have duplicates, and the joining column has to
have the same name in both dataframes. You can read more about the
:func:`join()` API and have a look at the :ref:`Tutorial on join operators
<join tutorial>`.

Left join in R's ``data.table``:

.. code-block:: R

    DT = data.table(x=rep(c("b","a","c"),each=3), y=c(1,3,6), v=1:9)
    X = data.table(x=c("c","b"), v=8:7, foo=c(4,2))

    X[DT, on="x"]

       x  v foo y i.v
    1: b  7   2 1   1
    2: b  7   2 3   2
    3: b  7   2 6   3
    4: a NA  NA 1   4
    5: a NA  NA 3   5
    6: a NA  NA 6   6
    7: c  8   4 1   7
    8: c  8   4 3   8
    9: c  8   4 6   9

Join in ``datatable``::

    >>> DT = dt.Frame(x = ["b"]*3 + ["a"]*3 + ["c"]*3,
    ...               y = [1, 3, 6] * 3,
    ...               v = range(1, 10))
    >>> X = dt.Frame({"x":('c','b'),
    ...               "v":(8,7),
    ...               "foo":(4,2)})
    >>> X.key = "x"
    >>>
    >>> DT[:, :, join(X)]
       | x          y      v    v.0    foo
       | str32  int32  int32  int32  int32
    -- + -----  -----  -----  -----  -----
     0 | b          1      1      7      2
     1 | b          3      2      7      2
     2 | b          6      3      7      2
     3 | a          1      4     NA     NA
     4 | a          3      5     NA     NA
     5 | a          6      6     NA     NA
     6 | c          1      7      8      4
     7 | c          3      8      8      4
     8 | c          6      9      8      4
    [9 rows x 5 columns]

An inner join could be simulated by removing the nulls. Again, a :func:`join`
only works if the joining dataframe is keyed.

.. code-block:: R

    DT[X, on="x", nomatch=NULL]

       x y v i.v foo
    1: c 1 7   8   4
    2: c 3 8   8   4
    3: c 6 9   8   4
    4: b 1 1   7   2
    5: b 3 2   7   2
    6: b 6 3   7   2

.. code-block:: python

    >>> DT[g[-1] != None, :, join(X)]  # g refers to the joining dataframe X
       | x          y      v    v.0    foo
       | str32  int32  int32  int32  int32
    -- + -----  -----  -----  -----  -----
     0 | b          1      1      7      2
     1 | b          3      2      7      2
     2 | b          6      3      7      2
     3 | c          1      7      8      4
     4 | c          3      8      8      4
     5 | c          6      9      8      4
    [6 rows x 5 columns]

A *not join* can be simulated as well:

.. code-block:: R

    DT[!X, on="x"]

       x y v
    1: a 1 4
    2: a 3 5
    3: a 6 6

.. code-block:: python

    >>> DT[g[-1]==None, f[:], join(X)]
       | x          y      v
       | str32  int32  int32
    -- + -----  -----  -----
     0 | a          1      4
     1 | a          3      5
     2 | a          6      6
    [3 rows x 3 columns]

Select the first row for each group:

.. code-block:: R

    DT[X, on="x", mult="first"]

       x y v i.v foo
    1: c 1 7   8   4
    2: b 1 1   7   2

.. code-block:: python

    >>> DT[g[-1] != None, :, join(X)][0, :, by('x')]  # chaining comes in handy here
       | x          y      v    v.0    foo
       | str32  int32  int32  int32  int32
    -- + -----  -----  -----  -----  -----
     0 | b          1      1      7      2
     1 | c          1      7      8      4
    [2 rows x 5 columns]


Select the last row for each group:

.. code-block:: R

    DT[X, on="x", mult="last"]

       x y v i.v foo
    1: c 6 9   8   4
    2: b 6 3   7   2

.. code-block:: python

    >>> DT[g[-1]!=None, :, join(X)][-1, :, by('x')]
       | x          y      v    v.0    foo
       | str32  int32  int32  int32  int32
    -- + -----  -----  -----  -----  -----
     0 | b          6      3      7      2
     1 | c          6      9      8      4
    [2 rows x 5 columns]


Join and evaluate ``j`` for each row in ``i``:

.. code-block:: R

    DT[X, sum(v), by=.EACHI, on="x"]

       x V1
    1: c 24
    2: b  6

.. code-block:: python

    >>> DT[g[-1]!=None, :, join(X)][:, dt.sum(f.v), by("x")]
       | x          v
       | str32  int64
    -- + -----  -----
     0 | b          6
     1 | c         24
    [2 rows x 2 columns]

Aggregate on columns from both dataframes in ``j``:

.. code-block:: R

    DT[X, sum(v)*foo, by=.EACHI, on="x"]

       x V1
    1: c 96
    2: b 12

.. code-block:: python

    >>> DT[:, dt.sum(f.v*g.foo), join(X), by(f.x)][f[-1]!=0, :]
       | x         C0
       | str32  int64
    -- + -----  -----
     0 | b         12
     1 | c         96
    [2 rows x 2 columns]

Aggregate on columns with same name from both dataframes in ``j``:

.. code-block:: R

    DT[X, sum(v)*i.v, by=.EACHI, on="x"]

       x  V1
    1: c 192
    2: b  42

.. code-block:: python

    >>> DT[:, dt.sum(f.v*g.v), join(X), by(f.x)][f[-1]!=0, :]
       | x         C0
       | str32  int64
    -- + -----  -----
     0 | b         42
     1 | c        192
    [2 rows x 2 columns]

Expect significant improvement in join functionality, with more concise syntax,
as well as additions of more features in the future.



Functions in R/data.table not yet implemented
---------------------------------------------

This is a list of some functions in ``data.table`` that do not have an
equivalent in ``datatable`` yet, that we would likely implement

- Reshaping functions

  - `melt <https://rdatatable.gitlab.io/data.table/reference/melt.data.table.html>`__
  - `dcast <https://rdatatable.gitlab.io/data.table/reference/dcast.data.table.html>`__

- Convenience functions for filtering and subsetting

  - `like <https://rdatatable.gitlab.io/data.table/reference/like.html>`__
  - `between <https://rdatatable.gitlab.io/data.table/reference/between.html>`__
  - `inrange <https://rdatatable.gitlab.io/data.table/reference/between.html>`__
  - `between <https://rdatatable.gitlab.io/data.table/reference/between.html>`__
  - `%chin% <https://rdatatable.gitlab.io/data.table/reference/chmatch.html>`__

- Duplicate functions

  - `duplicated <https://rdatatable.gitlab.io/data.table/reference/duplicated.html>`__
  - `unique <https://rdatatable.gitlab.io/data.table/reference/duplicated.html>`__
    in ``data.table`` returns unique rows, while :func:`unique()` in ``datatable``
    returns a single column of unique values in the entire dataframe.

- Aggregation functions

  - `frank <https://rdatatable.gitlab.io/data.table/reference/frank.html>`__
  - `frollmean <https://rdatatable.gitlab.io/data.table/reference/froll.html>`__
  - `frollsum <https://rdatatable.gitlab.io/data.table/reference/froll.html>`__
  - `frollapply <https://rdatatable.gitlab.io/data.table/reference/froll.html>`__
  - `rollup <https://rdatatable.gitlab.io/data.table/reference/groupingsets.html>`__
  - `cube <https://rdatatable.gitlab.io/data.table/reference/groupingsets.html>`__
  - `groupingsets <https://rdatatable.gitlab.io/data.table/reference/groupingsets.html>`__

- Missing values functions

  - `fcoalesce <https://rdatatable.gitlab.io/data.table/reference/coalesce.html>`__

Also, at the moment, custom aggregations in the ``j`` section are not supported
in ``datatable`` -- we intend to implement that at some point.

There are no datetime functions in ``datatable``, and string operations are
limited as well.

If there are any functions that you would like to see in ``datatable``, please
head over to `github`_ and raise a feature request.


.. _`data.table` : https://data.table.gitlab.io/data.table/index.html
.. _`examples` : https://rdatatable.gitlab.io/data.table/reference/data.table.html#examples
.. _`R` : https://www.r-project.org/about.html
.. _`github` : https://github.com/h2oai/datatable/issues
