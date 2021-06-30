
.. xfunction:: datatable.cut
    :src: src/core/expr/fexpr_cut.cc pyfn_cut
    :tests: tests/dt/test-cut.py
    :cvar: doc_dt_cut
    :signature: cut(cols, nbins=10, bins=None, right_closed=True)

    .. x-version-added:: 0.11

    For each column from `cols` bin its values into equal-width intervals,
    when `nbins` is specified, or into arbitrary-width intervals,
    when interval edges are provided as `bins`.

    Parameters
    ----------
    cols: FExpr
        Input data for equal-width interval binning.

    nbins: int | List[int]
        When a single number is specified, this number of bins
        will be used to bin each column from `cols`.
        When a list or a tuple is provided, each column will be binned
        by using its own number of bins. In the latter case,
        the list/tuple length must be equal to the number of columns
        in `cols`.

    bins: List[Frame]
        List/tuple of single-column frames containing interval edges
        in strictly increasing order, that will be used for binning
        of the corresponding columns from `cols`. The list/tuple
        length must be equal to the number of columns in `cols`.

    right_closed: bool
        Each binning interval is `half-open`_. This flag indicates whether
        the right edge of the interval is closed, or not.

    return: FExpr
        f-expression that converts input columns into the columns filled
        with the respective bin ids.

    See also
    --------
    :func:`qcut()` -- function for equal-population binning.

    .. _`half-open`: https://en.wikipedia.org/wiki/Interval_(mathematics)#Terminology

