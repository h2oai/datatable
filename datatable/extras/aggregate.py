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
  dt_exemplars, dt_members = core.aggregate(self._dt, n_bins, nx_bins, ny_bins, max_dimensions, seed)
  names_exemplars = self.names + ("count",)
  names_members = ("exemplar_id")
  return Frame(dt_exemplars, names_exemplars), Frame(dt_members, names_members)