#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import types

from .node import Node
from datatable.expr import DatatableExpr, ExprNode
from datatable.utils.misc import plural_form as plural
from datatable.utils.misc import normalize_slice



class ColumnSetNode(Node):

    def __init__(self):
        pass


#===============================================================================

class SliceView_CSNode(ColumnSetNode):

    def __init__(self, dt, start, count, step):
        super().__init__()
        self._dt = dt
        self._start = start
        self._count = count
        self._step = step
        self._cname = ""

    @property
    def dt(self):
        return self._dt

    @property
    def n_columns(self):
        return self._count

    @property
    def n_view_columns(self):
        # All columns are view columns
        return self._count

    @property
    def column_names(self):
        if self._step == 0:
            s = self._dt.names[self._start]
            return tuple([s] * self._count)
        else:
            end = self._start + self._count * self._step
            return self._dt.names[self._start:end:self._step]

    def cget_columns(self, context):
        if not self._cname:
            self._cname = self._gen_c(context)
        return self._cname


    def _gen_c(self, context):
        varname = context.make_variable_name()
        fnname = "get_" + varname
        ncols = self._count
        if not context.has_function(fnname):
            fn = "static Column** %s(void) {\n" % fnname
            fn += "    ViewColumn **cols = calloc(%d, sizeof(ViewColumn*));\n" \
                  % (ncols + 1)
            fn += "    Column **srccols = dt->source->columns;\n"
            fn += "    if (cols == NULL) return NULL;\n"
            fn += "    int64_t j = %dLL;\n" % self._start
            fn += "    for (int64_t i = 0; i < %d; i++) {\n" % ncols
            fn += "        cols[i] = malloc(sizeof(ViewColumn));\n"
            fn += "        if (cols[i] == NULL) return NULL;\n"
            fn += "        cols[i]->srcindex = j;\n"
            fn += "        cols[i]->mtype = MT_VIEW;\n"
            fn += "        cols[i]->stype = srccols[j]->stype;\n"
            fn += "        j += %dLL;\n" % self._step
            fn += "    }\n"
            fn += "    cols[%d] = NULL;\n" % ncols
            fn += "    return (Column**) cols;\n"
            fn += "}\n"
            context.add_function(fnname, fn)
        return fnname



#===============================================================================

def make_columnset(cols, dt, _nested=False):
    if cols is Ellipsis:
        return SliceView_CSNode(dt, 0, dt.ncols, 1)

    if isinstance(cols, (int, str, slice, ExprNode)):
        cols = [cols]

    if isinstance(cols, (list, tuple)):
        ncols = dt._ncols
        out = []
        for col in cols:
            if isinstance(col, int):
                if -ncols <= col < ncols:
                    if col < 0:
                        col += ncols
                    out.append((col, dt._names[col]))
                else:
                    n_columns = plural(ncols, "column")
                    raise ValueError(
                        "datatable has %s; column number %d is invalid"
                        % (n_columns, col))
            elif isinstance(col, str):
                if col in dt._inames:
                    out.append((dt._inames[col], col))
                else:
                    raise ValueError(
                        "Column %r not found in the datatable" % col)
            elif isinstance(col, slice):
                start = col.start
                stop = col.stop
                step = col.step
                if isinstance(start, str) or isinstance(stop, str):
                    if start is None:
                        col0 = 0
                    elif isinstance(start, str):
                        if start in dt._inames:
                            col0 = dt._inames[start]
                        else:
                            raise ValueError(
                                "Column name %r not found in the datatable"
                                % start)
                    else:
                        raise ValueError(
                            "The slice should start with a column name: %s"
                            % col)
                    if stop is None:
                        col1 = ncols
                    elif isinstance(stop, str):
                        if stop in dt._inames:
                            col1 = dt._inames[stop] + 1
                        else:
                            raise ValueError("Column name %r not found in "
                                             "the datatable" % stop)
                    else:
                        raise ValueError("The slice should end with a "
                                         "column name: %r" % col)
                    if step is None or step == 1:
                        step = 1
                    else:
                        raise ValueError("Column name slices cannot use "
                                         "strides: %r" % col)
                    if col1 <= col0:
                        col1 -= 2
                        step = -1
                    for i in range(col0, col1, step):
                        out.append((i, dt._names[i]))
                else:
                    if not all(x is None or isinstance(x, int)
                               for x in (start, stop, step)):
                        raise ValueError("%r is not integer-valued" % col)
                    col0, count, step = normalize_slice(col, ncols)
                    for i in range(count):
                        j = col0 + i * step
                        out.append((j, dt._names[j]))

            elif isinstance(col, ExprNode):
                out.append((col, str(col)))

        return out

    if isinstance(cols, types.FunctionType) and not _nested:
        dtexpr = DatatableExpr(dt)
        res = cols(dtexpr)
        return dt._cols_selector(res, True)

    raise ValueError("Unknown `select` argument: %r" % cols)

