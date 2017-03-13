#!/usr/bin/env python
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
from __future__ import division, print_function, unicode_literals

from datatable.utils.typechecks import ValueError


class _LazyDataTable(object):

    def __init__(self, src=None, dag=None):
        self._dt = src  # reference to the parent "eager" datatable
        self._dag = dag

    @property
    def nrows(self):
        return self._dt.nrows

    @property
    def ncols(self):
        return self._dt.ncols


    def __getattr__(self, attr):
        if self._dag is None:
            i = self._dt._inames.get(attr, None)
            if i is None:
                raise ValueError("Unknown column %s for %r" % (attr, self._dt))
            return _LazyDataTable(src=self._dt, dag=("col", i))
        raise NotImplemented


    def __getitem__(self, item):
        if isinstance(item, int):
            ncols = self.ncols
            if item < -ncols or item >= ncols:
                raise ValueError("Invalid column index %d for %r"
                                 % (item, self._dt))
            if item < 0:
                item += ncols
            return _LazyDataTable(src=self._dt, dag=("col", item))


    def ____make(m):
        return lambda self, rhs: _binop(self, m, rhs)
    for m in ["eq", "ne", "lt", "gt", "le", "ge", "and", "or",
              "add", "sub", "mul", "truediv", "floordiv",
              "mod", "pow", "lshift", "rshift", "radd", "rsub",
              "rmul", "rtruediv", "rfloordiv", "rmod", "rpow",
              "rlshift", "rrshift", "rand", "ror"]:
        locals()["__%s__" % m] = ____make(m)
    del ____make


    def __repr__(self):
        expr = repr(self._dag)  # _repr_expr_(self._dag)
        return "<lazyDT(#%d %dx%d) %s>" % (self._dt._id, self._dt.nrows,
                                           self._dt.ncols, expr)




def _binop(lhs, op, rhs):
    if isinstance(lhs, _LazyDataTable):
        root = lhs._dt
        lval = lhs._dag
    else:
        lval = lhs
    if isinstance(rhs, _LazyDataTable):
        if root and rhs._dt is not root:
            raise NotImplemented("Joins are not supported yet")
        root = rhs._dt
        rval = rhs._dag
    else:
        rval = rhs
    return _LazyDataTable(src=root, dag=(op, lval, rval))


def _repr_expr_(dag):
    if dag is None:
        return "."
    else:
        return "(%s)" % " ".join(_repr_expr_(d) if isinstance(d, tuple) else
                                 str(d)
                                 for d in dag)
