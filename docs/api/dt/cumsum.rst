
.. xfunction:: datatable.cumsum
    :src: src/core/expr/fexpr_cumsum.cc pyfn_cumsum
    :tests: tests/dt/test-cumsum.py
    :cvar: doc_dt_cumsum
    :signature: cumsum(cols)

    .. x-version-added:: 1.1.0

    For each column from `cols` calculate cumulative sum.

    Parameters
    ----------
    cols: FExpr
        Input data for cumulative sum calculation.

    return: FExpr
        f-expression that converts input columns into the columns filled
        with the respective cumulative sums.
