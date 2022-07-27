
.. xfunction:: datatable.ngroup
    :src: src/core/expr/fexpr_cumcountngroup.cc pyfn_ngroup
    :tests: tests/dt/test-cumcountngroup.py
    :cvar: doc_dt_ngroup
    :signature: ngroup(reverse=False)

    .. x-version-added:: 1.1.0

    For each row return a group number it belongs to. In the absence of
    :func:`by()` the frame is assumed to consist of one group only.


    Parameters
    ----------
    reverse: bool
        By default, when this parameter is ``False``, groups
        are numbered in the ascending order. Otherwise, when
        this paarmeter is ``True``, groups are numbered
        in the descending order.

    return: FExpr
        f-expression that returns group numbers for each row.


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

    Number groups in the ascending order::

        >>> DT[:, dt.ngroup(), f.C0]
           | C0        C1
           | str32  int64
        -- + -----  -----
         0 | a          0
         1 | a          0
         2 | a          0
         3 | b          1
         4 | b          1
         5 | c          2
         6 | c          2
         7 | c          2
        [8 rows x 2 columns]

    Number groups in the descending order::

        >>> DT[:, dt.ngroup(reverse = True), f.C0]
           | C0        C1
           | str32  int64
        -- + -----  -----
         0 | a          2
         1 | a          2
         2 | a          2
         3 | b          1
         4 | b          1
         5 | c          0
         6 | c          0
         7 | c          0
        [8 rows x 2 columns]

    Number groups in the absence of :func:`by()`::

        >>> DT[:, dt.ngroup()]
           | C0        C1
           | str32  int64
        -- + -----  -----
         0 | a          0
         1 | a          0
         2 | a          0
         3 | b          0
         4 | b          0
         5 | c          0
         6 | c          0
         7 | c          0
        [8 rows x 2 columns]

