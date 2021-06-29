
.. xfunction:: datatable.str.len
    :src: src/core/expr/fexpr_slice.cc FExpr_Slice::evaluate_n
    :tests: tests/expr/str/test-len.py
    :cvar: doc_str_len
    :signature: len(column)

    Compute lengths of values in a string column.


    Parameters
    ----------
    column: FExpr[str]

    return: FExpr[int64]
