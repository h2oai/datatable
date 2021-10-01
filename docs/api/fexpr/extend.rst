
.. xmethod:: datatable.FExpr.extend
    :src: src/core/expr/fexpr.cc PyFExpr::extend
    :cvar: doc_FExpr_extend
    :signature: extend(self, arg)

    Append ``FExpr`` `arg` to the current FExpr.

    Each ``FExpr`` represents a collection of columns, or a columnset. This
    method takes two such columnsets and combines them into a single one,
    similar to :func:`cbind() <datatable.cbind>`.


    Parameters
    ----------
    arg: FExpr
        The expression to append.

    return: FExpr
        New FExpr which is a combination of the current FExpr and `arg`.


    See also
    --------
    - :meth:`remove() <dt.FExpr.remove>` -- remove columns from a columnset.
