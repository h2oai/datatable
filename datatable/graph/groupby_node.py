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


def make_groupby(grby, ee):
    if grby is None:
        return None
    if not isinstance(grby, by):
        grby = by(grby)

    grby.resolve_columns(ee)
    return grby



class by(core.by):

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
