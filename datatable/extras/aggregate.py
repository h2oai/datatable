#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable as dt
from datatable.lib import core


def aggregate(dt_in, min_rows=500, n_bins=500, nx_bins=50, ny_bins=50,
              nd_max_bins=500, max_dimensions=50, seed=0, progress_fn=None,
              nthreads=0, buffer_rows = 10000):
    """
    Aggregate datatable in-place.

    Parameters
    ----------
    dt_in: datatable
        Frame to be aggregated in-place.
    min_rows: int
        Minimum number of rows in a dataset to perform an aggregation on.
    n_bins: int
        Number of bins for 1D aggregation.
    nx_bins: int
        Number of x bins for 2D aggregation.
    ny_bins: int
        Number of y bins for 2D aggregation.
    nd_max_bins: int
        Maximum number of exemplars for ND aggregation, not a hard limit.
    max_dimensions: int
        Number of columns at which start using the projection method.
    seed: int
        Seed to be used for the projection method.
    progress_fn: object
        Python function for progress reporting accepting two arguments:
        - `progress`, that is a value from 0 to 1;
        - `status_code`, 0 – in progress, 1 – completed.
    nthreads: int
        Number of OpenMP threads ND aggregator will use. Default is 0,
        i.e. automatically figure out the optimal number.

    Returns
    -------
    The target datatable is aggregated in-place and gets an additional
    column `count` with the number of members for a particular exemplar.
    The function returns a new one-column datatable that contains exemplar_ids
    for each of the original rows.
    """

    if progress_fn is not None and not callable(progress_fn):
        raise dt.TypeError("`progress_fn` argument should be a function")

    dt_members = core.aggregate(dt_in, min_rows, n_bins, nx_bins, ny_bins,
                                nd_max_bins, max_dimensions, seed, progress_fn,
                                nthreads, buffer_rows)

    return dt_members