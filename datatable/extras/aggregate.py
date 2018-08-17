#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable as dt
from datatable import Frame
from datatable.lib import core

def aggregate(self, n_bins=500, nx_bins=50, ny_bins=50, max_dimensions=50, seed=0):
    """
    Aggregate datatable in-place.

    Parameters
    ----------
    n_bins: int
        Number of bins for 1D aggregation.
    nx_bins: int
        Number of x bins for 2D aggregation.
    ny_bins: int
        Number of y bins for 2D aggregation.
    max_dimensions: int
        Number of columns at which start using the projection method.
    seed: int
        Seed to be used for the projection method.

    Returns
    -------
    The target datatable is aggregated in-place and gets an additional column `count`
    with the number of members for each exemplar. The function returns
    a new one-column datatable that contains exemplar_ids for each of the original rows.
    """
    names_exemplars = self.names + ("count",)
    names_members = ("exemplar_id")
    dt_members = core.aggregate(self._dt, n_bins, nx_bins, ny_bins, max_dimensions, seed)
    self.__init__(self.internal, names_exemplars)
    return Frame(dt_members, names_members)