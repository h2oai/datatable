
.. xfunction:: datatable.str.len
    :src: src/core/expr/str/fexpr_len.cc fn_len
    :tests: tests/str/test-len.py
    :cvar: doc_str_len
    :signature: len(column)

    Compute lengths of values in a string column.


    Parameters
    ----------
    column: FExpr[str]

    return: FExpr[int64]
