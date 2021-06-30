
.. xfunction:: datatable.qcut
    :src: src/core/expr/fexpr_qcut.cc pyfn_qcut
    :cvar: doc_dt_qcut
    :tests: tests/dt/test-qcut.py
    :signature: qcut(cols, nquantiles=10)

    .. x-version-added:: 0.11

    Bin all the columns from `cols` into intervals with approximately
    equal populations. Thus, the intervals are chosen according to
    the sample quantiles of the data.

    If there are duplicate values in the data, they will all be placed
    into the same bin. In extreme cases this may cause the bins to be
    highly unbalanced.

    Parameters
    ----------
    cols: FExpr
        Input data for quantile binning.

    nquantiles: int | List[int]
        When a single number is specified, this number of quantiles
        will be used to bin each column from `cols`.

        When a list or a tuple is provided, each column will be binned
        by using its own number of quantiles. In the latter case,
        the list/tuple length must be equal to the number of columns
        in `cols`.

    return: FExpr
        f-expression that converts input columns into the columns filled
        with the respective quantile ids.

    See also
    --------
    :func:`cut()` -- function for equal-width interval binning.
