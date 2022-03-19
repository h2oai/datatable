
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


    Examples
    --------

    Bin two-column frame by using the same number of quantiles for both columns:

    .. code-block:: python

        >>> from datatable import dt, f, qcut
        >>> DT = dt.Frame([range(5), [3, 14, 15, 92, 6]])
        >>> DT[:, qcut(f[:], nquantiles = 3)]
           |    C0     C1
           | int32  int32
        -- + -----  -----
         0 |     0      0
         1 |     0      1
         2 |     1      2
         3 |     2      2
         4 |     2      0
        [5 rows x 2 columns]


    Bin two-column frame by using column-specific number of quantiles:

    .. code-block:: python

        >>> from datatable import dt, f, qcut
        >>> DT = dt.Frame([range(5), [3, 14, 15, 92, 6]])
        >>> DT[:, qcut(f[:], nquantiles = [3, 5])]
           |    C0     C1
           | int32  int32
        -- + -----  -----
         0 |     0      0
         1 |     0      2
         2 |     1      3
         3 |     2      4
         4 |     2      1
        [5 rows x 2 columns]


    See also
    --------
    :func:`cut()` -- function for equal-width interval binning.
