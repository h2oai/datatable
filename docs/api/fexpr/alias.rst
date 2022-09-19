
.. xmethod:: datatable.FExpr.alias
    :src: src/core/expr/fexpr.cc PyFExpr::alias
    :cvar: doc_FExpr_alias
    :signature: alias(self, *names)

    Assign new names to the columns from the current ``FExpr``.


    Parameters
    ----------
    names: str | List[str] | Tuple[str]
        New names that should be assigned to the columns from
        the current ``FExpr``.

    return: FExpr
        New ``FExpr`` which is a obtained from the current one by setting
        new `names`.
