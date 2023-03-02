
.. xfunction:: datatable.cumcount
    :src: src/core/expr/fexpr_cumcountngroup.cc pyfn_cumcount
    :tests: tests/dt/test-cumcountngroup.py
    :cvar: doc_dt_cumcount
    :signature: cumcount(reverse=False)

    .. x-version-added:: 1.1.0

    Number rows within each group. In the absence of :func:`by()`
    the frame is assumed to consist of one group only.


    Parameters
    ----------
    reverse: bool
        By default, when this parameter is ``False``, the numbering
        is performed in the ascending order. Otherwise, when
        this parameter is ``True``, the numbering is done
        in the descending order.

    return: FExpr
        f-expression that returns row numbers within each group.


    Examples
    --------

    Create a sample datatable frame::

        >>> from datatable import dt, f, by
        >>> DT = dt.Frame(['a', 'a', 'a', 'b', 'b', 'c', 'c', 'c'])
        >>> DT
           | C0
           | str32
        -- + -----
         0 | a
         1 | a
         2 | a
         3 | b
         4 | b
         5 | c
         6 | c
         7 | c
        [8 rows x 1 column]

    Number rows within each group in the ascending order::

        >>> DT[:, dt.cumcount(), f.C0]
           | C0        C1
           | str32  int64
        -- + -----  -----
         0 | a          0
         1 | a          1
         2 | a          2
         3 | b          0
         4 | b          1
         5 | c          0
         6 | c          1
         7 | c          2
        [8 rows x 2 columns]

    Number rows within each group in the descending order::

        >>> DT[:, dt.cumcount(reverse = True), f.C0]
           | C0        C1
           | str32  int64
        -- + -----  -----
         0 | a          2
         1 | a          1
         2 | a          0
         3 | b          1
         4 | b          0
         5 | c          2
         6 | c          1
         7 | c          0
        [8 rows x 2 columns]

    Number rows in the absence of :func:`by()`::

        >>> DT[:, [f.C0, dt.cumcount()]]
           | C0        C1
           | str32  int64
        -- + -----  -----
         0 | a          0
         1 | a          1
         2 | a          2
         3 | b          3
         4 | b          4
         5 | c          5
         6 | c          6
         7 | c          7
        [8 rows x 2 columns]


