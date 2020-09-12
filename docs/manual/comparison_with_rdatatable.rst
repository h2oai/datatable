.. py:currentmodule:: datatable

Comparison with R's data.table
==============================

``datatable`` is closely related to R's `data.table <https://data.table.gitlab.io/data.table/index.html>`__ attempts to mimic its core algorithms and API; however, there are differences due to language constraints.

This page shows how to perform similar basic operations in R's `data.table <https://data.table.gitlab.io/data.table/index.html>`__  versus ``datatable``.

Subsetting Rows
---------------
The sample data is from the example data in R's `data.table <https://data.table.gitlab.io/data.table/index.html>`__.

``data.table``::

    library(data.table)

    DT = data.table(x=rep(c("b","a","c"),each=3),
                    y=c(1,3,6), v=1:9)


``datatable``::

    from datatable import dt, f, g, by, update, ifelse, join, sort
    import numpy as np

    DT = dt.Frame({"x":np.repeat(["b","a","c"],3),
                   "y": [1,3,6]*3,
                   "v": range(1,10)}) # datatable




===============================   ============================================ ==================================================
data.table                           datatable                                   Action
===============================   ============================================ ==================================================
``DT[2]``                            ``DT[1, :]``                               Select 2nd row
``DT[2:3]``                          ``DT[1:3, :]``                             Select 2nd and 3rd row
``DT[3:2]``                         ``DT[[2,1], :]``                            Select 3rd and 2nd row
``DT[.N]``                          ``DT[-1, :]``                               Select the last row
``DT[y>2]``                         ``DT[f.y>2, :]``                            All rows where ``y > 2``
``DT[y>2 & v>5]``                   ``DT[(f.y>2) & (f.v>5), :]``                Compound logical expressions
``DT[!2:4]`` or ``DT[-(2:4)]``      ``DT[[0, slice(4, None)], :]``              All rows other than rows 2,3,4
``DT[order(x), ]``                  | ``DT.sort("x")`` or                       Sort by column ``x``, ascending
                                    | ``DT[:, :, sort("x")]``
``DT[order(-x)]``                   | ``DT.sort(-f.x)`` or                      Sort by column ``x``, descending
                                    | ``DT[:, :, sort(-f.x)]``
``DT[order(x, -y)]``                | ``DT.sort(x, -f.y)`` or                   Sort by column ``x`` ascending, ``y`` descending
                                    | ``DT[:, :, sort(x, -f.y)]``
===============================   ============================================ ==================================================

Note the use of the ``f`` symbol when performing computations or sorting in descending order. You can read more about :ref:`f-expressions`.

Selecting Columns
-----------------

=========================================== ============================================ ==============================================
data.table                                           datatable                                                  Action
=========================================== ============================================ ==============================================
``DT[, .(v)]``                               ``DT[:, 'v']``                              Select column ``v``
``DT[, .(x,v)]``                             ``DT[:, ['x', 'v']]``                       Select multiple columns
``DT[, .(m = x)]``                           ``DT[:. {"m" : f.x}]``                      Rename and select column
``DT[, .(sv=sum(v))]``                       ``DT[:, {"sv": dt.sum(f.v)}]``              Sum column ``v`` and rename as ``sv``
``DT[, .(v, v*2)]``                          ``DT[:, [f.v, f.v*2]]``                     Return two columns, ``v`` and ``v`` doubled
``DT[, 2]``                                  ``DT[:, 1]``                                Select the second column
``DT[, ncol(DT), with=FALSE]``               ``DT[:, -1]``                               Select last column
``DT[, .SD, .SDcols=x:y]``                   ``DT[:, f["x":"y"]]``                       Select columns ``x`` through ``y``
``DT[ , .SD, .SDcols = !x:y]``               | ``DT[:, [name not in ("x","y")``          Exclude columns ``x`` and ``y``
                                             |          ``for name in DT.names]]`` or
                                             | ``DT[:, f[:].remove(f['x':'y'])]``
``DT[ , .SD, .SDcols = patterns('^[xv]')]``  | ``DT[:, [name.startswith(("x","v"))``     Select columns that start with ``x`` or ``v``
                                             |          ``for name in DT.names]]``
=========================================== ============================================ ==============================================

In ``datatable``, a single column can also be selected as ``DT['v']``.

Subset rows and Select/Aggregate
--------------------------------

====================================             ==========================================          ==============================================
data.table                                           datatable                                              Action
====================================             ==========================================          ==============================================
``DT[2:3, .(sum(v))]``                            ``DT[1:3, dt.sum(f.v)]``                             Sum column ``v`` over rows 2 and 3
``DT[2:3, .(sv=sum(v))]``                         ``DT[1:3, {"sv": dt.sum(f.v)}]``                     Same as above, new column name
``DT[x=="b", .(sum(v*y))]``                       ``DT[f.x=="b", dt.sum(f.v * f.y)]``                  Filter in ``i`` and aggregate in ``j``
``DT[x=="b", sum(v*y)]``                          ``DT[f.x=="b", dt.sum(f.v * f.y)][0, 0]``             Same as above, return as scalar
====================================             ==========================================          ==============================================

In `R <https://www.r-project.org/about.html>`_, indexing starts at 1 and when slicing, the first and last items are included. However, in `Python <https://www.python.org/>`_, indexing starts at 0, and when slicing, only the first item is included.

Grouping with :func:`by()`
----------------------------------------

========================================         =============================================    ============================================================
data.table                                           datatable                                              Action
========================================         =============================================    ============================================================
``DT[, sum(v), by=x]``                            ``DT[:, dt.sum(f.v), by('x')]``                  | Get the sum of column ``v``,
                                                                                                   | grouped by column ``x``
``DT[x!="a", sum(v), by=x]``                      ``DT[f.x!="a", :][:, dt.sum(f.v), by("x")]``     Get sum of ``v`` where ``x != a``
``DT[, .N, by=x]``                                ``DT[:, dt.count(), by("x")]``                   Number of rows per group
``DT[, .SD[1], by=x]``                            ``DT[0, :, by('x')]``                            | Select first row of ``y`` and ``v``
                                                                                                   | for each group in ``x``
``DT[, c(.N, lapply(.SD, sum)), by=x]``           ``DT[:, dt.sum(f[:]), by("x")]``                 | Get rows and sum columns ``v`` and ``y``
                                                                                                   | by group
``DT[, sum(v), by=.(y%%2)]``                      ``DT[:, dt.sum(f.v), by(f.y%2)]``                Expressions in :func:`by`

``DT[, .SD[which.min(v)], by=x]``                 ``DT[0, f[:], by("x"), dt.sort(f.v)]``           Get row per group where column ``v`` is minimum

``DT[, tail(.SD,2), by=x]``                       ``DT[-2:, :, by("x")]``                          Last 2 rows of each group
========================================         =============================================    ============================================================

In R's `data'table <https://data.table.gitlab.io/data.table/index.html>`__, the order of the groupings is preserved; in ``datatable``, the returned dataframe is sorted on the grouping column. ``DT[, sum(v), keyby=x]`` in data.table returns a dataframe ordered by column ``x``.

Also, in ``datatable``, :ref:`f-expressions` in the ``i`` section of a groupby is not yet implemented, hence the chaining method to get the sum of column ``v`` where ``x!=a``.

Multiple aggregations within a group can be executed in R's `data'table <https://data.table.gitlab.io/data.table/index.html>`__ with the syntax below ::

    DT[, list(MySum=sum(v),
              MyMin=min(v),
              MyMax=max(v)),
       by=.(x, y%%2)]

The same can be replicated in ``datatable`` by using a dictionary ::

    DT[:, {MySum: dt.sum(f.v),
           MyMin: dt.min(f.v),
           MyMax: dt.max(f.v)},
       by(f.x, f.y%2)]


Add/Update/Delete Columns
-------------------------

========================================         ===============================================  ============================================================
data.table                                           datatable                                              Action
========================================         ===============================================  ============================================================
``DT[, z:=42L]``                                 | ``DT[:, update(z=42)]`` or                       Add new column
                                                 | ``DT['z'] = 42`` or
                                                 | ``DT[:, 'z'] = 42`` or
                                                 | ``DT = DT[:, f[:].extend({"z":42})]``
``DT[, c('sv','mv') := .(sum(v), "X")]``         | ``DT[:, update(sv = dt.sum(f.v), mv = "X")]``    Add multiple columns
``DT[, z:=NULL]``                                | ``del DT['z']`` or                               Remove column
                                                 | ``del DT[:, 'z']`` or
                                                 | ``DT = DT[:, f[:].remove(f.z)]``
``DT["a", v:=42L, on="x"]``                      | ``DT[f.x=="a", update(v=42)]`` or                Subassign to existing ``v`` column
                                                 | ``DT[f.x=="a", 'v'] = 42``
``DT["b", v2:=84L, on="x"]``                     | ``DT[f.x=="b", update(v2=84)]`` or               Subassign to new column (NA padded)
                                                 | ``DT[f.x=='b', 'v2'] = 84``
``DT[, m:=mean(v), by=x]``                       | ``DT[:, update(m=dt.mean(f.v)), by("x")]``       Add new column, assigning values group-wise
========================================         ===============================================  ============================================================

Note that the :func:`update` function, as well as the ``del`` function operates in-place; there is no need for reassignment. Another advantage of the :func:`update` method is that the row order of the dataframe is not changed, even in a groupby; this comes in handy in a lot of transformation operations.


Joins
------

At the moment, only the left outer join is implemented in ``datatable``. Another aspect is that the dataframe being joined must be keyed, the column or columns to be keyed must not have duplicates, and the joining column has to have the same name in both dataframes. You can read more about the :func:`join()` API and have a look at the `Tutorial on the join operator <https://datatable.readthedocs.io/en/latest/start/quick-start.html#join>`_

Left join in R's `data'table <https://data.table.gitlab.io/data.table/index.html>`_::

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

    DT = dt.Frame({"x":np.repeat(["b","a","c"],3),
                   "y": [1,3,6]*3,
                   "v": range(1,10)})

    X = dt.Frame({"x":('c','b'),
                  "v":(8,7),
                  "foo":(4,2)})

    X.key="x" # key the ``x`` column

    DT[:, :, join(X)]

        x	y	v	v.0	foo
    0	b	1	1	7	2
    1	b	3	2	7	2
    2	b	6	3	7	2
    3	a	1	4	NA	NA
    4	a	3	5	NA	NA
    5	a	6	6	NA	NA
    6	c	1	7	8	4
    7	c	3	8	8	4
    8	c	6	9	8	4

- An inner join could be simulated by removing the nulls. Again, a :func:`join` only works if the joining dataframe is keyed.

``data.table``::

    DT[X, on="x", nomatch=NULL]

       x y v i.v foo
    1: c 1 7   8   4
    2: c 3 8   8   4
    3: c 6 9   8   4
    4: b 1 1   7   2
    5: b 3 2   7   2
    6: b 6 3   7   2

``datatable``::

    DT[g[-1]!=None, :, join(X)] # g refers to the joining dataframe X

        x	y	v	v.0	foo
    0	b	1	1	7	2
    1	b	3	2	7	2
    2	b	6	3	7	2
    3	c	1	7	8	4
    4	c	3	8	8	4
    5	c	6	9	8	4

- A `not join` can be simulated as well.

``data.table``::

    DT[!X, on="x"]

       x y v
    1: a 1 4
    2: a 3 5
    3: a 6 6

``datatable``::

    DT[g[-1]==None, f[:], join(X)]

        x	y	v
    0	a	1	4
    1	a	3	5
    2	a	6	6

- Select the first row for each group

``data.table``::

    DT[X, on="x", mult="first"]

       x y v i.v foo
    1: c 1 7   8   4
    2: b 1 1   7   2

``datatable``::

    DT[g[-1]!=None, :, join(X)][0, :, by('x')] #chaining comes in handy here

        x	y	v	v.0	foo
    0	b	1	1	7	2
    1	c	1	7	8	4


- Select the last row for each group

``data.table``::

       x y v i.v foo
    1: c 6 9   8   4
    2: b 6 3   7   2

``datatable``::

    DT[g[-1]!=None, :, join(X)][-1, :, by('x')]

        x	y	v	v.0	foo
    0	b	6	3	7	2
    1	c	6	9	8	4

- Join and evaluate ``j`` for each row in ``i``

``data.table``::

    DT[X, sum(v), by=.EACHI, on="x"]

       x V1
    1: c 24
    2: b  6

``datatable``::

    DT[g[-1]!=None, :, join(X)][:, dt.sum(f.v), by("x")]

        x	v
    0	b	6
    1	c	24

- Aggregate on columns from both dataframes in ``j``

``data.table``::

     DT[X, sum(v)*foo, by=.EACHI, on="x"]

       x V1
    1: c 96
    2: b 12

``datatable``::

    DT[:, dt.sum(f.v*g.foo), join(X), by(f.x)][f[-1]!=0, :]

        x	C0
    0	b	12
    1	c	96

- Compute on columns with same name from both dataframes in ``j``

``data.table``::

    DT[X, sum(v)*i.v, by=.EACHI, on="x"]

       x  V1
    1: c 192
    2: b  42

``datatable``::

    DT[:, dt.sum(f.v*g.v), join(X), by(f.x)][f[-1]!=0, :]

        x	C0
    0	b	42
    1	c	192

Expect significant improvement in join functionality, with more concise syntax as ``datatable`` matures.