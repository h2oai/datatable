
.. xmethod:: datatable.FExpr.remove
    :src: src/core/expr/fexpr.cc PyFExpr::remove
    :cvar: doc_FExpr_remove
    :signature: remove(self, arg)

    Remove columns `arg` from the current FExpr.

    Each ``FExpr`` represents a collection of columns, or a columnset. Some
    of those columns are computed while others are specified "by reference",
    for example ``f.A``, ``f[:3]`` or ``f[int]``. This method allows you to
    remove by-reference columns from an existing FExpr.


    Parameters
    ----------
    arg: FExpr
        The columns to remove. These must be "columns-by-reference", i.e.
        they cannot be computed columns.

    return: FExpr
        New FExpr which is a obtained from the current FExpr by removing
        the columns in `arg`.


    See also
    --------
    - :meth:`extend() <dt.FExpr.extend>` -- append a columnset.
