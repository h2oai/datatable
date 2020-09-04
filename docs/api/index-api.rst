
.. py:module:: datatable

datatable API
=============

Symbols listed here are available for import from the ``datatable`` module.


Submodules
----------

.. list-table::
    :widths: auto
    :class: api-table

    * - :mod:`.math <datatable.math>`
      - Mathematical functions, similar to python's ``math`` module.

    * - :mod:`.models <datatable.models>`
      - A small set of data analysis tools.


Classes
-------

.. list-table::
    :widths: auto
    :class: api-table

    * - :class:`Frame`
      - Main "table of data" class. This is the equivalent of pandas' or Julia's
        ``DataFrame``, R's ``data.table`` or ``tibble``, SQL's ``TABLE``, etc.

    * - :class:`FExpr`
      - Helper class for computing formulas over a frame.

    * - :class:`Namespace`
      - Helper class for addressing columns in a frame.

    * - :class:`stype`
      - Enum of column "storage" types, analogous to numpy's ``dtype``.

    * - :class:`ltype`
      - Enum of column "logical" types, similar to standard Python notion
        of a ``type``.


Functions
---------

.. list-table::
    :widths: auto
    :class: api-table

    * - :func:`fread()`
      - Read CSV/text/XLSX/Jay/other files

    * - :func:`iread()`
      - Same as :func:`fread()`, but read multiple files at once

    * -
      -
    * - :func:`by()`
      -
    * - :func:`join()`
      -
    * - :func:`sort()`
      -
    * - :func:`update()`
      -
    * -
      -
    * - :func:`cbind()`
      - Combine frames by columns

    * - :func:`rbind()`
      - Combine frames by rows

    * - :func:`repeat()`
      -
    * -
      -
    * - :func:`ifelse()`
      - Ternary if operator
    * - :func:`isna()`
      -
    * - :func:`shift()`
      -
    * - :func:`cut()`
      - Bin a column into equal-width intervals

    * - :func:`qcut()`
      - Bin a column into equal-population intervals

    * -
      -
    * - :func:`init_styles()`
      -
    * - :func:`rowall()`
      -
    * - :func:`rowany()`
      -
    * - :func:`rowcount()`
      -
    * - :func:`rowfirst()`
      -
    * - :func:`rowlast()`
      -
    * - :func:`rowmax()`
      -
    * - :func:`rowmean()`
      -
    * - :func:`rowmin()`
      -
    * - :func:`rowsd()`
      -
    * - :func:`rowsum()`
      -
    * -
      -
    * - :func:`intersect()`
      -
    * - :func:`setdiff()`
      -
    * - :func:`symdiff()`
      -
    * - :func:`union()`
      -
    * - :func:`unique()`
      -
    * -
      -
    * - :func:`corr()`
      - Coefficient of correlation between two columns
    * - :func:`count()`
      - Count non-missing values in a column
    * - :func:`cov()`
      - The covariance between two columns
    * - :func:`max()`
      - The largest element in a column
    * - :func:`mean()`
      - Arithmetic mean of all values in a column
    * - :func:`median()`
      - The median element in a column
    * - :func:`min()`
      - The smallest element in a column
    * - :func:`sd()`
      - The standard deviation of values in a column
    * - :func:`sum()`
      - Sum of values in a column


Other
-----

.. list-table::
    :widths: auto
    :class: api-table

    * - :data:`build_info`
      - Information about the build of the datatable module.

    * - :data:`dt`
      - The datatable module.

    * - :data:`f`
      - The primary namespace used during :meth:`DT[i,j,...] <Frame.__getitem__>` call.


.. toctree::
    :hidden:

    math.           <math>
    models.         <models>
    Frame           <frame>
    FExpr           <fexpr>
    Namespace       <namespace>
    build_info      <dt/build_info>
    by()            <dt/by>
    cbind()         <dt/cbind>
    cut()           <dt/cut>
    dt              <dt/dt>
    f               <dt/f>
    fread()         <dt/fread>
    ifelse()        <dt/ifelse>
    iread()         <dt/iread>
    qcut()          <dt/qcut>
    rbind()         <dt/rbind>
    repeat()        <dt/repeat>
    shift()         <dt/shift>
    sort()          <dt/sort>
    update()        <dt/update>
