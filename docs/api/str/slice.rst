
.. xfunction:: datatable.str.slice
    :src: src/core/expr/fexpr_slice.cc FExpr_Slice::evaluate_n
    :tests: tests/str/test-slice.py
    :cvar: doc_str_slice
    :signature: slice(col, start, stop, step=1)

    Apply slice ``[start:stop:step]`` to each value in a `column` of string
    type.

    Instead of this function you can directly apply a slice expression to
    the column expression::

        - ``f.A[1:-1]`` is equivalent to
        - ``dt.str.slice(f.A, 1, -1)``.


    Parameters
    ----------
    col : FExpr[str]
        The column to which the slice should be applied.

    return: FExpr[str]
        A column containing sliced string values from the source column.


    Examples
    --------
    >>> DT = dt.Frame(A=["apples", "bananas", "cherries", "dates",
    ...                  "eggplants", "figs", "grapes", "kiwi"])
    >>> DT[:, dt.str.slice(f.A, None, 5)]
       | A
       | str32
    -- + -----
     0 | apple
     1 | banan
     2 | cherr
     3 | dates
     4 | eggpl
     5 | figs
     6 | grape
     7 | kiwi
    [8 rows x 1 column]
