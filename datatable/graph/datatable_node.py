#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

import _datatable
import datatable
from .node import Node
from .rows_node import make_rowfilter, FilterExprRFNode
from .cols_node import make_columnset, SliceCSNode, ArrayCSNode
from .context import CModuleNode


#===============================================================================

class DatatableNode(Node):
    # """
    # Root node for a datatable evaluation graph.

    # This class takes as arguments: a :class:`RowFilterNode`, a
    # :class:`ColumnSetNode`, a :class:`GroupByNode`, etc -- and binds them
    # together into a single evaluation graph.

    # Unlike any other node in the graph, this class contains an :method:`execute`
    # method that does the following: (1) prepares an evalution context where the
    # graph can be materialized, (2) generates C code for each node in the graph,
    # (3) JIT-compiles the code and injects the binary into the runtime, (4)
    # invokes the "main" function in that module to produce the resulting
    # datatable, and finally (5) returns the datatable produced.
    # """

    def __init__(self, dt):
        super().__init__()
        self._dt = dt


    def get_result(self):
        rowsnode = self.soup.get("rows")
        selectnode = self.soup.get("select")
        rowindex = rowsnode.get_result()
        columns = selectnode.get_result()
        res_dt = _datatable.datatable_assemble(rowindex, columns)
        return datatable.DataTable(res_dt, colnames=selectnode.column_names)




def make_datatable(dt, rows, select):
    cmodule = CModuleNode()
    rows_node = make_rowfilter(rows, dt)
    cols_node = make_columnset(select, dt)

    if isinstance(rows_node, FilterExprRFNode):
        rows_node.use_cmodule(cmodule)

    # Select some (or all) rows + some (or all) columns. In this case columns
    # can be simply copied by reference, and then the resulting datatable will
    # be either a plain "data" table if rowindex selects all rows and the target
    # datatable is not a view, or a "view" datatable otherwise.
    if isinstance(cols_node, (SliceCSNode, ArrayCSNode)):
        rowindex = rows_node.make_final_rowindex()
        columns = cols_node.get_result()
        res_dt = _datatable.datatable_assemble(rowindex, columns)
        return datatable.DataTable(res_dt, colnames=cols_node.column_names)

    return None
