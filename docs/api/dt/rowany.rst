
.. xfunction:: datatable.rowany
    :src: src/core/expr/fnary/rowany.cc FExpr_RowAny::apply_function
    :tests: tests/ijby/test-rowwise.py
    :cvar: doc_dt_rowany
    :signature: rowany(*cols)

    For each row in `cols` return `True` if any of the values in that row
    are `True`, or otherwise return `False`. The function uses shortcut evaluation:
    if the `True` value is found in one of the columns, then the subsequent columns
    are skipped. Missing values are counted as `False`.


    Parameters
    ----------
    cols: FExpr[bool]
        Input boolean columns.

    return: FExpr[bool]
        f-expression consisting of one boolean column that has the same number
        of rows as in `cols`.

    except: TypeError
        The exception is raised when one of the columns from `cols`
        has a non-boolean type.


    Examples
    --------
    ::

        >>> from datatable import dt, f
        >>> DT = dt.Frame({"A":[True, True],
        ...                "B":[True, False],
        ...                "C":[True, True]})
        >>> DT
           |     A      B      C
           | bool8  bool8  bool8
        -- + -----  -----  -----
         0 |     1      1      1
         1 |     1      0      1
        [2 rows x 3 columns]

    ::

        >>> DT[:, dt.rowany(f[:])]
           |    C0
           | bool8
        -- + -----
         0 |     1
         1 |     1
        [2 rows x 1 column]


    See Also
    --------
    - :func:`rowall()` -- row-wise `all() <https://docs.python.org/3/library/functions.html#all>`_ function.
