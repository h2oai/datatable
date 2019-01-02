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
        self._col = col

    def execute(self, ee):
        df = ee.dt.internal
        col = self._col
        if ee.rowindex:
            cf = core.columns_from_slice(df, ee.rowindex, col, 1, 1)
            df = cf.to_frame(None).internal
            col = 0
        rowindex, groupby = df.sort(col, True)
        f.set_rowindex(rowindex)
        ee.set_source_rowindex(rowindex)
        ee.clear_final_rowindex()
        if ee.rowindex:
            ee.set_final_rowindex(rowindex, ee.rowindex)
        ee.rowindex = rowindex
        ee.groupby = groupby
        ee.groupby_cols = [self._col]



def make_groupby(grby, ee):
    if grby is None:
        return None
    if isinstance(grby, by):
        grby.resolve_columns(ee)
        return grby

    # TODO: change to ee.make_columnset() when we can do multi-col sorts
    grbycol = process_column(grby, ee.dt)
    if not isinstance(grbycol, int):
        raise TTypeError("Currently only single-column group-bys are "
                         "supported")

    return SimpleGroupbyNode(ee, grbycol)



class by:
    def __init__(self, *args):
        self._cols = list(args)

    def resolve_columns(self, ee):
        for i, col in enumerate(self._cols):
            self._cols[i] = process_column(col, ee.dt)

    def execute(self, ee):
        df = ee.dt.internal
        rowindex, groupby = df.sort(*self._cols, True)
        f.set_rowindex(rowindex)
        ee.set_source_rowindex(rowindex)
        ee.clear_final_rowindex()
        if ee.rowindex:
            ee.set_final_rowindex(rowindex, ee.rowindex)
        ee.rowindex = rowindex
        ee.groupby = groupby
        ee.groupby_cols = list(self._cols)
