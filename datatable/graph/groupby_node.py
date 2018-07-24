#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
from .cols_node import process_column
from datatable.lib import core
from datatable.utils.typechecks import TTypeError
from datatable.graph.dtproxy import f


class SimpleGroupbyNode:

    def __init__(self, ee, col):
        self._engine = ee
        self._col = col

    def execute(self):
        df = self._engine.dt.internal
        col = self._col
        if self._engine.rowindex:
            cf = core.columns_from_slice(df, self._engine.rowindex, col, 1, 1)
            df = cf.to_datatable()
            col = 0
        rowindex, groupby = df.sort(col, True)
        f.set_rowindex(rowindex)
        self._engine.rowindex = rowindex
        self._engine.groupby = groupby
        self._engine.groupby_cols = [self._col]



def make_groupby(grby, ee):
    if grby is None:
        return None

    # TODO: change to ee.make_columnset() when we can do multi-col sorts
    grbycol = process_column(grby, ee.dt)
    if not isinstance(grbycol, int):
        raise TTypeError("Currently only single-column group-bys are "
                         "supported")

    return SimpleGroupbyNode(ee, grbycol)
