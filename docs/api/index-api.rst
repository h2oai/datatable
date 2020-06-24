
.. py:module:: datatable

datatable API
=============

=========================  =====================================================
**Classes**
--------------------------------------------------------------------------------
:class:`Frame`             Main "table of data" class
:class:`ltype`
:class:`stype`

**Functions**
--------------------------------------------------------------------------------
:func:`fread()`            Read CSV/text/XLSX/Jay/other files
:func:`iread()`            Same as `fread()`, but read multiple files at once

:func:`by()`
:func:`join()`
:func:`sort()`
:func:`update()`

:func:`cbind()`            Combine frames by columns
:func:`rbind()`            Combine frames by rows
:func:`repeat()`

:func:`ifelse()`           Ternary if operator
:func:`isna()`
:func:`shift()`

:func:`init_styles()`

:func:`rowall()`
:func:`rowany()`
:func:`rowcount()`
:func:`rowfirst()`
:func:`rowlast()`
:func:`rowmax()`
:func:`rowmean()`
:func:`rowmin()`
:func:`rowsd()`
:func:`rowsum()`

:func:`intersect()`
:func:`setdiff()`
:func:`symdiff()`
:func:`union()`
:func:`unique()`

:func:`corr()`             Coefficient of correlation between two columns
:func:`count()`            Count non-missing values in a column
:func:`cov()`              The covariance between two columns
:func:`max()`              The largest element in a column
:func:`mean()`             Arithmetic mean of all values in a column
:func:`median()`           The median element in a column
:func:`min()`              The smallest element in a column
:func:`sd()`               The standard deviation of values in a column
:func:`sum()`              Sum of values in a column
=========================  =====================================================


.. toctree::
    :hidden:

    Frame           <frame>
    FTRL            <ftrl>
    math.           <math>
    by()            <dt/by>
    cbind()         <dt/cbind>
    fread()         <dt/fread>
    ifelse()        <dt/ifelse>
    iread()         <dt/iread>
    rbind()         <dt/rbind>
    repeat()        <dt/repeat>
    shift()         <dt/shift>
    sort()          <dt/sort>
    update()        <dt/update>
