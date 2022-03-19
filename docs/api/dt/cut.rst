
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
        A list or a tuple of frames for binning the corresponding columns from `cols`.
        Each frame should have only one column that is populated with the edges of
        the binning intervals in strictly increasing order. Number of elements in
        `bins` must be equal to the number of columns in `cols`.

    right_closed: bool
        Each binning interval is `half-open`_. This flag indicates whether
        the right edge of the interval is closed, or not.

    return: FExpr
        f-expression that converts input columns into the columns filled
        with the respective bin ids.

    Examples
    --------

    Bin one-column frame by specifying a number of bins:

    .. code-block:: python

        >>> from datatable import dt, f, cut
        >>> DT = dt.Frame([1, 3, 5])
        >>> DT[:, cut(f[:], nbins = 5)]
           |    C0
           | int32
        -- + -----
         0 |     0
         1 |     2
         2 |     4
        [3 rows x 1 column]

    Bin one-column frame by specifying edges of the binning intervals:

    .. code-block:: python

        >>> from datatable import dt, f, cut
        >>> DT = dt.Frame([1, 3, 5])
        >>> BINS = [dt.Frame(range(5))]
        >>> DT[:, cut(f[:], bins = BINS)] # Note, "5" goes out of bounds and is binned as "NA"
           |    C0
           | int32
        -- + -----
         0 |     0
         1 |     2
         2 |    NA
        [3 rows x 1 column]


    Bin two-column frame by specifying a number of bins for each column separately:

    .. code-block:: python

        >>> from datatable import dt, f, cut
        >>> DT = dt.Frame([[1, 3, 5], [5, 7, 9]])
        >>> DT[:, cut(f[:], nbins = [5, 10])]
           |    C0     C1
           | int32  int32
        -- + -----  -----
         0 |     0      0
         1 |     2      4
         2 |     4      9
        [3 rows x 2 columns]


    Bin two-column frame by specifying edges of the binning intervals:

    .. code-block:: python

        >>> from datatable import dt, f, cut
        >>> DT = dt.Frame([[1, 3, 5], [0.1, -0.1, 1.5]])
        >>> BINS = [dt.Frame(range(10)), dt.Frame([-1.0, 0, 1.0, 2.0])]
        >>> DT[:, cut(f[:], bins = BINS)]
           |    C0     C1
           | int32  int32
        -- + -----  -----
         0 |     0      1
         1 |     2      0
         2 |     4      2
        [3 rows x 2 columns]


    See also
    --------
    :func:`qcut()` -- function for equal-population binning.

    .. _`half-open`: https://en.wikipedia.org/wiki/Interval_(mathematics)#Terminology

