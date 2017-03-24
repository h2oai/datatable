#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
from __future__ import division, print_function, unicode_literals

# from datatable.utils.typechecks import ValueError


class DataTableExpr(object):

    def __init__(self, src=None, dag=None, type=None):
        self._src = src
        self._dag = dag
        self._type = type


    def __getattr__(self, attr):
        """Retrieve a named column from the datatable."""
        if self._dag is None:
            i = self._src._inames.get(attr, None)
            if i is None:
                raise ValueError("Unknown column %s for %r" % (attr, self._src))
            coltype = self._src.types[i]
            return DataTableExpr(src=self._src, dag=("col", i), type=coltype)
        else:
            raise ValueError("Cannot extract a named column from an expression")


    def __getitem__(self, item):
        """Retrieve an indexed column from the datatable."""
        if self._dag is not None:
            raise ValueError("Cannot extract an indexed column from an "
                             "expression %r" % self)
        dt = self._src
        if isinstance(item, int):
            ncols = dt.ncols
            if item < -ncols or item >= ncols:
                raise ValueError("Invalid column index %d for %r"
                                 % (item, dt))
            if item < 0:
                item += ncols
            coltype = dt.types[item]
            return DataTableExpr(src=dt, dag=("col", item), type=coltype)
        if isinstance(item, str):
            return self.__getattr__(item)
        if isinstance(item, DataTableExpr):
            raise TypeError("Dynamic column selectors are not supported")
        raise TypeError("Unknown selector type: %s" % type(item))


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
        return "<DTexpr(#%d %dx%d) %s>" % (self._src._id, self._src.nrows,
                                           self._src.ncols, expr)



_boolops = {"eq", "ne", "lt", "gt", "le", "ge", "and", "or", "rand", "ror"}
def _binop(lhs, op, rhs):
    if isinstance(lhs, DataTableExpr):
        parent = lhs._src
        if not lhs._dag:
            raise TypeError("Operations on all columns are not allowed")
        ltype = lhs._type
        if not ltype:
            raise TypeError("Unable to detect type of %r" % lhs)
    else:
        ltype = _detect_type(lhs)
    if isinstance(rhs, DataTableExpr):
        if not rhs._dag:
            raise TypeError("Operations on all columns are not allowed")
        if parent and rhs._src is not parent:
            raise NotImplemented("Joins are not supported yet")
        parent = rhs._src
        rtype = rhs._type
        if not rtype:
            raise TypeError("Unable to detect type of %r" % lhs)
    else:
        rtype = _detect_type(rhs)

    # TODO: we need much more accurate type checking here...
    if op in _boolops:
        restype = "bool"
    else:
        if ltype == "str" and rtype == "str":
            restype = "str"
        if ltype == "real" or rtype == "real":
            restype = "real"
        elif ltype == "int" or rtype == "int":
            restype = "int"
        elif ltype == "bool" or rtype == "bool":
            restype = "bool"
        else:
            raise TypeError("Unsupported operationn '%s' on operands of "
                            "types %s and %s" % (op, ltype, rtype))
    return DataTableExpr(src=parent, dag=(op, lhs, rhs), type=restype)


def _detect_type(val):
    if val == 0 or val == 1:  # note: True == 1 and False == 0
        return "bool"
    if isinstance(val, int):
        return "int"
    if isinstance(val, float):
        return "real"
    if isinstance(val, str):
        return "str"
    return "obj"


def _repr_expr_(dag):
    if dag is None:
        return "."
    else:
        return "(%s)" % " ".join(_repr_expr_(d) if isinstance(d, tuple) else
                                 str(d)
                                 for d in dag)
