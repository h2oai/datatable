
.. xpy:module:: datatable


datatable API
=============

Symbols listed here are available for import from the root of the ``datatable``
module.


Submodules
----------

.. list-table::
    :widths: auto
    :class: api-table

    * - :mod:`exceptions. <datatable.exceptions>`
      - ``datatable`` warnings and exceptions.

    * - :mod:`internal. <datatable.internal>`
      - Access to some internal details of ``datatable`` module.

    * - :mod:`math. <datatable.math>`
      - Mathematical functions, similar to python's :mod:`math` module.

    * - :mod:`models. <datatable.models>`
      - A small set of data analysis tools.

    * - :mod:`re. <datatable.re>`
      - Functions using regular expressions.

    * - :mod:`str. <datatable.str>`
      - Functions for working with string columns.

    * - :mod:`time. <datatable.time>`
      - Functions for working with date/time columns.




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

    * - :class:`Type`
      - Column's type, similar to numpy's ``dtype``.

    * - :class:`stype`
      - [DEPRECATED] Enum of column "storage" types.

    * - :class:`ltype`
      - [DEPRECATED] Enum of column "logical" types.


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
      - Group-by clause for use in Frame’s square-bracket selector
    * - :func:`join()`
      - Join clause for use in Frame’s square-bracket selector
    * - :func:`sort()`
      - Sort clause for use in Frame’s square-bracket selector
    * - :func:`update()`
      - Create new or update existing columns within a frame
    * -
      -
    * - :func:`cbind()`
      - Combine frames by columns

    * - :func:`rbind()`
      - Combine frames by rows

    * - :func:`repeat()`
      - Concatenate frame by rows
    * -
      -
    * - :func:`as_type()`
      - Cast column into another type
    * - :func:`ifelse()`
      - Ternary if operator
    * - :func:`shift()`
      - Shift column by a given number of rows
    * - :func:`cut()`
      - Bin a column into equal-width intervals
    * - :func:`qcut()`
      - Bin a column into equal-population intervals
    * - :func:`split_into_nhot()`
      - Split and nhot-encode a single-column frame

    * -
      -
    * - :func:`init_styles()`
      - Inject datatable's stylesheets into the Jupyter notebook
    * - :func:`rowall()`
      - Row-wise `all() <https://docs.python.org/3/library/functions.html#all>`_ function
    * - :func:`rowany()`
      - Row-wise `any() <https://docs.python.org/3/library/functions.html#any>`_ function
    * - :func:`rowcount()`
      - Calculate the number of non-missing values per row
    * - :func:`rowfirst()`
      - Find the first non-missing value row-wise
    * - :func:`rowlast()`
      - Find the last non-missing value row-wise
    * - :func:`rowmax()`
      - Find the largest element row-wise
    * - :func:`rowmean()`
      - Calculate the mean value row-wise
    * - :func:`rowmin()`
      - Find the smallest element row-wise
    * - :func:`rowsd()`
      - Calculate the standard deviation row-wise
    * - :func:`rowsum()`
      - Calculate the sum of all values row-wise
    * -
      -
    * - :func:`intersect()`
      - Calculate the set intersection of values in the frames
    * - :func:`setdiff()`
      - Calculate the set difference between the frames
    * - :func:`symdiff()`
      - Calculate the symmetric difference between the sets of values in the frames
    * - :func:`union()`
      - Calculate the union of values in the frames
    * - :func:`unique()`
      - Find unique values in a frame
    * -
      -
    * - :func:`corr()`
      - Calculate correlation between two columns
    * - :func:`count()`
      - Count non-missing values per column
    * - :func:`cov()`
      - Calculate covariance between two columns
    * - :func:`max()`
      - Find the largest element per column
    * - :func:`mean()`
      - Calculate mean value per column
    * - :func:`median()`
      - Find the median element per column
    * - :func:`min()`
      - Find the smallest element per column
    * - :func:`sd()`
      - Calculate the standard deviation per column
    * - :func:`sum()`
      - Calculate the sum of all values per column
    * - :func:`countna()`
      - Count the number of NA values per column


Other
-----

.. list-table::
    :widths: auto
    :class: api-table

    * - :data:`build_info`
      - Information about the build of the ``datatable`` module.

    * - :data:`dt`
      - The ``datatable`` module itself.

    * - :data:`f`
      - The primary namespace used during :meth:`DT[...] <dt.Frame.__getitem__>` call.

    * - :data:`g`
      - Secondary namespace used during :meth:`DT[..., join()] <dt.Frame.__getitem__>` call.

    * - :class:`options`
      - ``datatable`` options.



.. toctree::
    :hidden:

    exceptions.       <exceptions>
    internal.         <internal>
    math.             <math>
    models.           <models>
    options.          <options>
    re.               <re>
    str.              <str>
    time.             <time>
    FExpr             <fexpr>
    Frame             <frame>
    ltype             <ltype>
    Namespace         <namespace>
    stype             <stype>
    Type              <type>
    as_type()         <dt/as_type>
    build_info        <dt/build_info>
    by()              <dt/by>
    cbind()           <dt/cbind>
    corr()            <dt/corr>
    count()           <dt/count>
    countna()         <dt/countna>
    cov()             <dt/cov>
    cut()             <dt/cut>
    dt                <dt/dt>
    f                 <dt/f>
    first()           <dt/first>
    fread()           <dt/fread>
    g                 <dt/g>
    ifelse()          <dt/ifelse>
    init_styles()     <dt/init_styles>
    intersect()       <dt/intersect>
    iread()           <dt/iread>
    join()            <dt/join>
    last()            <dt/last>
    max()             <dt/max>
    mean()            <dt/mean>
    median()          <dt/median>
    min()             <dt/min>
    qcut()            <dt/qcut>
    rbind()           <dt/rbind>
    repeat()          <dt/repeat>
    rowall()          <dt/rowall>
    rowany()          <dt/rowany>
    rowcount()        <dt/rowcount>
    rowfirst()        <dt/rowfirst>
    rowlast()         <dt/rowlast>
    rowmax()          <dt/rowmax>
    rowmean()         <dt/rowmean>
    rowmin()          <dt/rowmin>
    rowsd()           <dt/rowsd>
    rowsum()          <dt/rowsum>
    sd()              <dt/sd>
    setdiff()         <dt/setdiff>
    shift()           <dt/shift>
    sort()            <dt/sort>
    split_into_nhot() <dt/split_into_nhot>
    sum()             <dt/sum>
    symdiff()         <dt/symdiff>
    union()           <dt/union>
    unique()          <dt/unique>
    update()          <dt/update>
