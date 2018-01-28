#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

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
        self._colidx = colidx

    def make_rowindex(self):
        _dt = self.engine.dt.internal
        rowindex = _dt.sort(self._colidx)
        return rowindex



def make_sort(sort, ee):
    if sort is None:
        return None

    if isinstance(sort, (int, str)):
        colidx = ee.dt.colindex(sort)
        return SingleColumnSortNode(ee, colidx)

    raise TTypeError("Invalid parameter %r for argument `rows`" % sort)
