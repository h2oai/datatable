#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
#-------------------------------------------------------------------------------
# Collection of classes to construct a graph for datatable evaluation. All
# classes herein are meant for internal use only.
#
import _datatable
import datatable
from .rows_node import make_rowfilter, AllRFNode
from .cols_node import make_columnset, SliceCSNode, ArrayCSNode
from .context import CModuleNode, RequiresCModule

__all__ = ("make_datatable", )



def make_datatable(dt, rows, select):
    cmodule = CModuleNode()
    rows_node = make_rowfilter(rows, dt)
    cols_node = make_columnset(select, dt)

    if isinstance(rows_node, RequiresCModule):
        rows_node.use_cmodule(cmodule)
    if isinstance(cols_node, RequiresCModule):
        cols_node.use_cmodule(cmodule)

    # Select some (or all) rows + some (or all) columns. In this case columns
    # can be simply copied by reference, and then the resulting datatable will
    # be either a plain "data" table if rowindex selects all rows and the target
    # datatable is not a view, or a "view" datatable otherwise.
    if isinstance(cols_node, (SliceCSNode, ArrayCSNode)):
        rowindex = rows_node.make_final_rowindex()
        columns = cols_node.get_result()
        res_dt = _datatable.datatable_assemble(rowindex, columns)
        return datatable.DataTable(res_dt, colnames=cols_node.column_names)

    if isinstance(rows_node, AllRFNode) and not dt.internal.isview:
        columns = cols_node.get_result()
        res_dt = _datatable.datatable_assemble(None, columns)
        return datatable.DataTable(res_dt, colnames=cols_node.column_names)

    raise RuntimeError(  # pragma: no cover
        "Unable to handle input (rows=%r, select=%r)"
        % (rows, select))
