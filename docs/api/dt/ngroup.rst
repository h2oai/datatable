
.. xfunction:: datatable.ngroup
    :src: src/core/expr/fexpr_cumcountngroup.cc pyfn_ngroup
    :tests: tests/dt/test-cumcountngroup.py
    :cvar: doc_dt_ngroup
    :signature: ngroup(reverse=False)

    .. x-version-added:: 1.1.0

    Returns the group number for each row. 
    
    It is applicable only in the presence of :func:`by`.

    If `reverse = True`, the numbering is done in descending order.

    Parameters
    ----------
    reverse: bool
        If ``False``, the group number is returned in ascending order. 
        If ``True``, the group number is returned in descending order.

    return: FExpr
        f-expression that returns the number for each group.

    except: ValueError
        The exception is raised if there is no groupby operation with :func:`by()`.

    Examples
    --------

    Create a sample datatable frame::

        >>> from datatable import dt, f, by
        >>> DT = dt.Frame(['a','a','a','b','b','c','c','c'])
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

    Return the group number in ascending order::

        >>> DT[:, dt.ngroup(reverse = False), f.C0]
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

    Return the group number in descending order::

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



