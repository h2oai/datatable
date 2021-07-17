
.. xfunction:: datatable.models.aggregate
    :src: src/core/models/aggregate.cc aggregate
    :tests: tests/models/test-aggregate.py
    :cvar: doc_models_aggregate
    :signature: aggregate(frame, min_rows=500, n_bins=500, nx_bins=50, ny_bins=50,
        nd_max_bins=500, max_dimensions=50, seed=0, double_precision=False,
        fixed_radius=None)

    Aggregate a frame into clusters. Each cluster consists of
    a set of members, i.e. a subset of the input frame, and is represented by
    an exemplar, i.e. one of the members.

    For one- and two-column frames the aggregation is based on
    the standard equal-interval binning for numeric columns and
    grouping operation for string columns.

    In the general case, a parallel one-pass ad hoc algorithm is employed.
    It starts with an empty exemplar list and does one pass through the data.
    If a partucular observation falls into a bubble with a given radius and
    the center being one of the exemplars, it marks this observation as a
    member of that exemplar's cluster. If there is no appropriate exemplar found,
    the observation is marked as a new exemplar.

    If the `fixed_radius` is `None`, the algorithm will start
    with the `delta`, that is radius squared, being equal to the machine precision.
    When the number of gathered exemplars becomes larger than `nd_max_bins`,
    the following procedure is performed:

    - find the mean distance between all the gathered exemplars;
    - merge all the exemplars that are within the half of this distance;
    - adjust `delta` by taking into account the initial bubble radius;
    - save the exemplar's merging information for the final processing.

    If the `fixed_radius` is set to a valid numeric value, the algorithm
    will stick to that value and will not adjust `delta`.

    **Note:** the general n-dimensional algorithm takes into account the numeric
    columns only, and all the other columns are ignored.


    Parameters
    ----------
    frame: Frame
        The input frame containing numeric or string columns.

    min_rows: int
        Minimum number of rows the input frame should have to be aggregated.
        If `frame` has less rows than `min_rows`, aggregation
        is bypassed, in the sence that all the input rows become exemplars.

    n_bins: int
        Number of bins for 1D aggregation.

    nx_bins: int
        Number of bins for the first column for 2D aggregation.

    ny_bins: int
        Number of bins for the second column for 2D aggregation.

    nd_max_bins: int
        Maximum number of exemplars for ND aggregation. It is guaranteed
        that the ND algorithm will return less than `nd_max_bins` exemplars,
        but the exact number may vary from run to run due to parallelization.

    max_dimensions: int
        Number of columns at which the projection method is used for
        ND aggregation.

    seed: int
        Seed to be used for the projection method.

    double_precision: bool
        An option to indicate whether double precision, i.e. `float64`,
        or single precision, i.e. `float32`, arithmetic should be used
        for computations.

    fixed_radius: float
        Fixed radius for ND aggregation, use it with caution.
        If set, `nd_max_bins` will have no effect and in the worst
        case number of exemplars may be equal to the number of rows
        in the data. For big data this may result in extremly large
        execution times. Since all the columns are normalized to `[0, 1)`,
        the `fixed_radius` value should be chosen accordingly.

    return: Tuple[Frame, Frame]
        The first element in the tuple is the aggregated frame, i.e.
        the frame containing exemplars, with the shape of
        `(nexemplars, frame.ncols + 1)`, where `nexemplars` is
        the number of gathered exemplars. The first `frame.ncols` columns
        are the columns from the input frame, and the last column
        is the `members_count` that has stype `int32` containing
        number of members per exemplar.

        The second element in the tuple is the members frame with the shape of
        `(frame.nrows, 1)`. Each row in this frame corresponds to the
        row with the same id in the input `frame`. The single column `exemplar_id`
        has an stype of `int32` and contains the exemplar ids that a particular
        member belongs to. These ids are effectively the ids of
        the exemplar's rows from the input frame.


    except: TypeError
        The exception is raised when one of the `frame`'s columns has an
        unsupported stype, i.e. there is a column that is both non-numeric
        and non-string.
