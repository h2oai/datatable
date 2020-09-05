
.. py:module:: datatable

datatable API
=============

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


.. toctree::
    :hidden:

    Frame           <frame>
    FExpr           <fexpr>
    models.         <models>
    math.           <math>
    by()            <dt/by>
    cbind()         <dt/cbind>
    cut()           <dt/cut>
    fread()         <dt/fread>
    ifelse()        <dt/ifelse>
    iread()         <dt/iread>
    max()           <dt/max>
    mean()          <dt/mean>
    min()           <dt/min>
    qcut()          <dt/qcut>
    rbind()         <dt/rbind>
    repeat()        <dt/repeat>
    sd()            <dt/sd>
    shift()         <dt/shift>
    sort()          <dt/sort>
    update()        <dt/update>
