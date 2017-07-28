#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from datatable.utils.typechecks import TTypeError



class SortNode(object):

    def __init__(self, dt):
        super().__init__()
        self._dt = dt



class SingleColumn_SNode(SortNode):

    def __init__(self, dt, colidx):
        super().__init__(dt)
        self._colidx = colidx

    def make_rowindex(self):
        ri = self._dt.internal.sort(self._colidx)
        return ri



def make_sort(sort, dt):
    if sort is None:
        return None

    if isinstance(sort, (int, str)):
        colidx = dt.colindex(sort)
        return SingleColumn_SNode(dt, colidx)

    raise TTypeError("Invalid parameter %r for argument `rows`" % sort)
