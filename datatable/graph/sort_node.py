#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------

from datatable.utils.typechecks import TTypeError



class SortNode:
    __slots__ = ["_engine"]

    def __init__(self, ee):
        super().__init__()
        self._engine = ee

    @property
    def engine(self):
        return self._engine



class SingleColumnSortNode(SortNode):

    def __init__(self, ee, colidx):
        super().__init__(ee)
        self.colidx = colidx

    def make_rowindex(self):
        _dt = self.engine.dt.internal
        rowindex = _dt.sort(self.colidx)[0]
        return rowindex



def make_sort(sort, ee):
    if sort is None:
        return None

    if isinstance(sort, (int, str)):
        colidx = ee.dt.colindex(sort)
        return SingleColumnSortNode(ee, colidx)

    raise TTypeError("Invalid parameter %r for argument `rows`" % sort)
