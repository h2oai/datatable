
.. xfunction:: datatable.by
    :src: src/core/expr/py_by.cc oby::oby_pyobject::m__init__
    :cvar: doc_dt_by
    :signature: by(*cols, add_columns=True)

    Group-by clause for use in Frame's square-bracket selector.

    Whenever a ``by()`` object is present inside a ``DT[i, j, ...]``
    expression, it makes all other expressions to be evaluated in
    group-by mode. This mode causes the following changes to the
    evaluation semantics:

    - A "Groupby" object will be computed for the frame ``DT``, grouping
      it by columns specified as the arguments to the ``by()`` call. This
      object keeps track of which rows of the frame belong to which group.

    - If an ``i`` expression is present (row filter), it will be
      interpreted within each group. For example, if ``i`` is a slice,
      then the slice will be applied separately to each group. Similarly,
      if ``i`` expression contains a formula with reduce functions, then
      those functions will be evaluated for each group. For example::

          >>> DT[f.A == max(f.A), :, by(f.group_id)]

      will select those rows where column A reaches its peak value within
      each group (there could be multiple such rows within each group).

    - Before ``j`` is evaluated, the ``by()`` clause adds all the groupby
      columns at the start of ``j`` (unless ``add_columns`` argument is
      ``False``). If ``j`` is a "select-all" slice (i.e. ``:`` or
      ``f[:]``), then the groupby columns will be excluded
      from the list of all columns, so that they will be present in the output
      only once.

    - During evaluation of ``j``, the reducer functions, such as
      :func:`min`, :func:`sum`, etc, will be evaluated by-group, that is
      they will find the minimal value in each group, the sum of values in
      each group, and so on. If a reducer expression is combined with a
      regular column expression, then the reduced column will be
      auto-expanded into a column that is constant within each group.

    - Note that if both ``i`` and ``j`` contain reducer functions, then
      those functions will have a slightly different notion of groups: the
      reducers in ``i`` will see each group "in full", whereas the
      reducers in ``j`` will see each group after it was filtered by the
      expression in ``i`` (and possibly not even see some of the groups
      at all, if they were filtered out completely).

    - If ``j`` contains only reducer expressions, then the final result
      will be a Frame containing just a single row for each
      group. This resulting frame will also be keyed by the grouped-by
      columns.


    The ``by()`` function expects a single column or a sequence of columns
    as the argument(s). It accepts either a column name, or an
    f-expression. In particular, you can perform a group-by on a
    dynamically computed expression::

        >>> DT[:, :, by(dt.math.floor(f.A/100))]

    The default behavior of groupby is to sort the groups in the ascending
    order, with NA values appearing before any other values. As a special
    case, if you group by an expression ``-f.A``, then it will be
    treated as if you requested to group by the column "A" sorting it in
    the descending order. This will work even with column types that are
    not arithmetic, for example "A" could be a string column here.


    Parameters
    ----------
    cols: FExpr
        Columns to group the frame by.

    add_columns: bool
        If ``True``, columns, by which the frame is grouped,
        are added to the beginning of the resulting frame.
        If ``False``, the resulting frame will only contain
        columns listed in ``j``.

    return: object
        ``datatable.by`` object for use in square-bracket selector.


    Examples
    --------
    .. code-block:: python

        >>> from datatable import dt, f, by
        >>>
        >>> df = dt.Frame({"group1": ["A", "A", "B", "B", "A"],
        ...                "group2": [1, 0, 1, 1, 1],
        ...                "var1": [343, 345, 567, 345, 212]})
        >>> df
           | group1  group2   var1
           | str32     int8  int32
        -- + ------  ------  -----
         0 | A            1    343
         1 | A            0    345
         2 | B            1    567
         3 | B            1    345
         4 | A            1    212
        [5 rows x 3 columns]


    Group by a single column::

        >>> df[:, dt.count(), by("group1")]
           | group1  count
           | str32   int64
        -- + ------  -----
         0 | A           3
         1 | B           2
        [2 rows x 2 columns]


    Group by multiple columns::

        >>> df[:, dt.sum(f.var1), by("group1", "group2")]
           | group1  group2   var1
           | str32     int8  int64
        -- + ------  ------  -----
         0 | A            0    345
         1 | A            1    555
         2 | B            1    912
        [3 rows x 3 columns]


    Return grouping result without the grouping column(s) by setting the
    ``add_columns`` parameter to ``False``::

        >>> df[:, dt.sum(f.var1), by("group1", "group2", add_columns=False)]
           |  var1
           | int64
        -- + -----
         0 |   345
         1 |   555
         2 |   912
        [3 rows x 1 column]


    :ref:`f-expressions` can be passed to :func:`by()`::

        >>> df[:, dt.count(), by(f.var1 < 400)]
           |    C0  count
           | bool8  int64
        -- + -----  -----
         0 |     0      1
         1 |     1      4
        [2 rows x 2 columns]


    By default, the groups are sorted in ascending order. The inverse is
    possible by negating the :ref:`f-expressions` in :func:`by()`::

        >>> df[:, dt.count(), by(-f.group1)]
           | group1  count
           | str32   int64
        -- + ------  -----
         0 | B           2
         1 | A           3
        [2 rows x 2 columns]

    An integer can be passed to the ``i`` section::

        >>> df[0, :, by("group1")]
           | group1  group2   var1
           | str32     int8  int32
        -- + ------  ------  -----
         0 | A            1    343
         1 | B            1    567
        [2 rows x 3 columns]

    A slice is also acceptable within the ``i`` section::

        >>> df[-1:, :, by("group1")]
           | group1  group2   var1
           | str32     int8  int32
        -- + ------  ------  -----
         0 | A            1    212
         1 | B            1    345
        [2 rows x 3 columns]


    .. note::

      :ref:`f-expressions` is not implemented yet for the ``i`` section in a
      groupby. Also, a sequence cannot be passed to the ``i`` section in the
      presence of :func:`by()`.


    See Also
    --------
    - :ref:`Grouping with by` user guide for more examples.
