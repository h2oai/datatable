#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
from .cols_node import process_column
from datatable.utils.typechecks import TTypeError, TValueError


class SimpleGroupbyNode:

    def __init__(self, ee, col):
        self._engine = ee
        self._col = col

    def execute(self):
        df = self._engine.dt
        rowindex = df.internal.sort(self._col, True)
        self._engine.rowindex = rowindex



def make_groupby(grby, ee):
    if grby is None:
        return None

    # TODO: change to ee.make_columnset() when we can do multi-col sorts
    grbycol = process_column(grby, ee.dt)
    if not isinstance(grbycol, int):
        raise TTypeError("Currently only single-column group-bys are "
                         "supported")

    return SimpleGroupbyNode(ee, grbycol)
