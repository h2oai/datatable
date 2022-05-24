
.. xfunction:: datatable.rowall
    :src: src/core/expr/fnary/rowall.cc FExpr_RowAll::apply_function
    :cvar: doc_dt_rowall
    :tests: tests/ijby/test-rowwise.py
    :signature: rowall(*cols)

    For each row in `cols` return `True` if all values in that row are `True`,
    or otherwise return `False`. The function uses shortcut evaluation:
    if the `False` value is found in one of the columns, then the subsequent columns
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
        >>> DT = dt.Frame({"A": [True, True],
        ...                "B": [True, False],
        ...                "C": [True, True]})
        >>> DT
           |     A      B      C
           | bool8  bool8  bool8
        -- + -----  -----  -----
         0 |     1      1      1
         1 |     1      0      1
        [2 rows x 3 columns]

    ::

        >>> DT[:, dt.rowall(f[:])]
           |    C0
           | bool8
        -- + -----
         0 |     1
         1 |     0
        [2 rows x 1 column]


    See Also
    --------
    - :func:`rowany()` -- row-wise `any() <https://docs.python.org/3/library/functions.html#any>`_ function.
