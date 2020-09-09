.. py:currentmodule:: datatable

Comparison with Rdatatable
==========================

``Datatable`` strives to mimic `Rdatatable's <https://rdatatable.gitlab.io/data.table/index.html>`_ API as closely as possible; however, there are differences due to language constraints. 

This page shows how to perform similar basic operations in `Rdatatable <https://rdatatable.gitlab.io/data.table/index.html>`_  in ``datatable``.

Subsetting Rows
----------------
Sample data::

    from datatable import dt, f, g, by, update, ifelse, join, sort
    import numpy as np

    df = dt.Frame({"x":np.repeat(["b","a","c"],3),
                   "y": [1,3,6]*3,
                   "v": range(1,10)}) # datatable

    DT = data.table(x=rep(c("b","a","c"),each=3),
                    y=c(1,3,6), v=1:9) # rdatatable


===============================   ============================================ ==================================================
RDatatable                           Datatable                                   Action
===============================   ============================================ ==================================================
``DT[2]``                            ``df[1, :]``                               Select 2nd row
``DT[2:3]``                          ``df[1:3, :]``                             Select 2nd and 3rd row
``DT[3:2]``                         ``df[[2,1], :]``                            Select 3rd and 2nd row
``DT[.N]``                          ``df[-1, :]``                               Select the last row
``DT[y>2]``                         ``df[f.y>2, :]``                            All rows where ``y > 2``
``DT[y>2 & v>5]``                   ``df[(f.y>2) & (f.v>5), :]``                Compound logical expressions
``DT[!2:4]`` or ``DT[-(2:4)]``      ``df[[0, slice(4, None)], :]``              All rows other than rows 2,3,4
``DT[order(x), ]``                  | ``df.sort("x")`` or                       Sort by column ``x``, ascending
                                    | ``df[:, :, sort("x")]``
``DT[order(-x)]``                   | ``df.sort(-f.x)`` or                      Sort by column ``x``, descending
                                    | ``df[:, :, sort(-f.x)]``
``DT[order(x, -y)]``                | ``df.sort(x, -f.y)`` or                   Sort by column ``x`` ascending, ``y`` descending
                                    | ``df[:, :, sort(x, -f.y)]``
===============================   ============================================ ==================================================

Note the use of the ``f`` symbol when performing computations or sorting in descending order. You can read more about :ref:`f-expressions`.

Selecting Columns
------------------

=========================================== ============================================ ==============================================
RDatatable                                           Datatable                                                  Action
=========================================== ============================================ ==============================================
``DT[, .(v)]``                               ``df[:, 'v']``                              Select column ``v``
``DT[, .(x,v)]``                             ``df[:, ['x', 'v']]``                       Select multiple columns
``DT[, .(m = x)]``                           ``df[:. {"m" : f.x}]``                      Rename and select column
``DT[, .(sv=sum(v))]``                       ``df[:, {"sv": dt.sum(f.v)}]``              Sum column ``v`` and rename as ``sv``
``DT[, .(v, v*2)]``                          ``df[:, [f.v, f.v*2]]``                     Return two columns, ``v`` and ``v`` doubled
``DT[, 2]``                                  ``df[:, 1]``                                Select the second column
``DT[, ncol(DT), with=FALSE]``               ``df[:, -1]``                               Select last column
``DT[, .SD, .SDcols=x:y]``                   ``df[:, f["x":"y"]]``                       Select columns ``x`` through ``y``
``DT[ , .SD, .SDcols = !x:y]``               | ``df[:, [name not in ("x","y")``          Exclude columns ``x`` and ``y``
                                             |          ``for name in df.names]]`` or
                                             | ``df[:, f[:].remove(f['x':'y'])]``
``DT[ , .SD, .SDcols = patterns('^[xv]')]``  | ``df[:, [name.startswith(("x","v"))``     Select columns that start with x or v
                                             |          ``for name in df.names]]``
=========================================== ============================================ ==============================================

In ``datatable``, a single column can also be selected as ``df['v']``.

Subset rows and Select/Compute
-------------------------------

====================================             ==========================================          ==============================================
RDatatable                                           Datatable                                              Action
====================================             ==========================================          ==============================================
``DT[2:3, .(sum(v))]``                            ``df[1:3, dt.sum(f.v)]``                             Sum column ``v`` over rows 2 and 3
``DT[2:3, .(sv=sum(v))]``                         ``df[1:3, {"sv": dt.sum(f.v)}]``                     Same as above, new column name
``DT[x=="b", .(sum(v*y))]``                       ``df[f.x=="b", dt.sum(f.v * f.y)]``                  Filter in ``i`` and aggregate in ``j``
``DT[x=="b", sum(v*y)]``                          ``df[f.x=="b", dt.sum(f.v * f.y)][0,0]``             Same as above, return as scalar
====================================             ==========================================          ==============================================

In `R <https://www.r-project.org/about.html>`_, indexing starts at 1 and when slicing, the first and last items are included. However, in `Python <https://www.python.org/>`_, indexing starts at 0, and when slicing, only the first item is included.

Grouping Operations - j and by
-------------------------------

========================================         ============================================     ============================================================
RDatatable                                           Datatable                                              Action
========================================         ============================================     ============================================================
``DT[, sum(v), by=x]``                            ``df[:, dt.sum(f.v), by('x')]``                  | Get the sum of column ``v``,
                                                                                                   | grouped by column ``x``
``DT[x!="a", sum(v), by=x]``                      ``df[f.x!="a",:][:, dt.sum(f.v), by("x")]``      Get sum of ``v`` where ``x != a``
``DT[, .N, by=x]``                                ``df[:, dt.count(), by("x")]``                   Number of rows per group
``DT[, .SD[1], by=x]``                            ``df[0, : , by('x')]``                           | Select first row of ``y`` and ``v``
                                                                                                   | for each group in ``x``
``DT[, c(.N, lapply(.SD, sum)), by=x]``           ``df[:, dt.sum(f[:]), by("x")]``                 | Get rows and sum columns ``v`` and ``y``
                                                                                                   | by group
``DT[, sum(v), by=.(y%%2)]``                      ``df[:, dt.sum(f.v), by(f.y%2)]``                Expressions in :func:`by`

``DT[, .SD[which.min(v)], by=x]``                 ``df[0, f[:], by("x"), dt.sort(f.v)]``           Get row per group where column ``v`` is minimum

``DT[, tail(.SD,2), by=x]``                       ``df[-2:, :, by("x")]``                          Last 2 rows of each group
========================================         ============================================     ============================================================

In `Rdatatable <https://rdatatable.gitlab.io/data.table/index.html>`_, the order of the groupings is preserved; in ``datatable``, the returned dataframe is sorted on the grouping column. ``DT[, sum(v), keyby=x]`` in Rdatatable returns a dataframe ordered by column ``x``.

Also, in ``datatable``, f-expressions in the ``i`` section of a groupby is not yet implemented, hence the chaining method to get the sum of column ``v`` where ``x!=a``.

Multiple aggregations within a group can be executed in `Rdatatable <https://rdatatable.gitlab.io/data.table/index.html>`_ with the syntax below ::

    DT[, list(MySum=sum(v),
              MyMin=min(v),
              MyMax=max(v)),
       by=.(x, y%%2)]     

The same can be replicated in ``datatable`` by using a dictionary ::

    df[:, {MySum : dt.sum(f.v),
           MyMin : dt.min(f.v),
           MyMax : dt.max(f.v)},
       by(f.x, f.y%2)]


Add/Update/Delete Column
-------------------------

========================================         ============================================     ============================================================
RDatatable                                           Datatable                                              Action
========================================         ============================================     ============================================================
``DT[, z:=42L]``                                 | ``df[f.x=="a", update(z=42)]`` or                Add new column
                                                 | ``df[f.x=="a", 'z'] = 42`` or
                                                 | ``df = df[:, f[:].extend({"z":42})]``
``DT[, z:=NULL]``                                | ``del df['z']`` or                               Remove column
                                                 | ``del df[:, 'z']`` or
                                                 | ``df = df[:,f[:].remove(f.z)]``
``DT["a", v:=42L, on="x"]``                      | ``df[f.x=="a", update(v=42)]`` or                Subassign to exiting ``v`` column
                                                 | ``df[f.x=="a", 'v'] = 42``
``DT["b", v2:=84L, on="x"]``                     | ``df[f.x=="b", update(v2=84)]`` or               Subassign to new column (NA padded)
                                                 | ``df[f.x=='b', 'v2'] = 84``
``DT[, m:=mean(v), by=x]``                       | ``df[:, update(m=dt.mean(f.v)), by("x")]``       Add new column by group
========================================         ============================================     ============================================================

Note that the :func:`update` function, as well as the ``del`` function operates in-place; there is no need for reassignment. Another advantage of the :func:`update` method is that the row order of the dataframe is not changed, even in a groupby; this comes in handy in a lot of transformation operations.


Joins
-------

At the moment, the only :func:`join` implemented in `datatable` is a left outer join