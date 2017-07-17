#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

import _datatable
import datatable
from .node import Node
from .rows_node import make_rowfilter, All_RFNode, FilterExpr_RFNode
from .cols_node import make_columnset, Slice_CSNode, Array_CSNode


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



    #---------------------------------------------------------------------------

    # def execute(self, verbose=False):
    #     self.use_context(NodeSoup())
    #     mainfn = self.cget_datatable()
    #     _dt = self.soup.execute(mainfn, verbose=verbose)
    #     return datatable.DataTable(_dt, colnames=self._select.column_names)

    # def cget_datatable(self):
    #     rowindex = self._rows.cget_rowindex()
    #     self._select.use_rowindex(self._rows, self._dt)
    #     columns = self._select.cget_columns()
    #     fnname = self.soup.make_variable_name("get_datatable")
    #     fn = "PyObject* %s(void) {\n" % fnname
    #     fn += "    init();\n"

    #     if self._select.n_view_columns == 0:
    #         fn += self._gen_cbody_only_data_cols(rowindex, columns)
    #     else:
    #         assert not self._select.dt.internal.isview
    #         fn += self._gen_cbody_dt_not_view(rowindex, columns)

    #     fn += "}\n"
    #     self._context.add_function(fnname, fn)
    #     return fnname


    # def on_context_set(self):
    #     self._rows.use_context(self.soup)
    #     self._select.use_context(self.soup)


    # def _gen_cbody_only_data_cols(self, frowindex, fcolumns):
    #     self.soup.add_extern("pydatatable_assemble")
    #     fn = ""
    #     fn += "    int64_t nrows = %s()->length;\n" % frowindex
    #     fn += "    Column** columns = %s();\n" % fcolumns
    #     fn += "    return (PyObject*) pydatatable_assemble(nrows, columns);\n"
    #     return fn


    # def _gen_cbody_dt_not_view(self, frowindex, fcolumns):
    #     self.soup.add_extern("pydatatable_assemble_view")
    #     return ("    RowIndex *ri = {make_rowindex}();\n"
    #             "    Column **columns = {make_columns}();\n"
    #             "    return (PyObject*) pydatatable_assemble_view"
    #             "((DataTable_PyObject*){dtptr}, ri, columns);\n"
    #             .format(dtptr=id(self._dt.internal),
    #                     make_rowindex=frowindex,
    #                     make_columns=fcolumns))


def make_datatable(dt, rows, select):
    rows_node = make_rowfilter(rows, dt)
    cols_node = make_columnset(select, dt)

    # Select some (or all) rows + some (or all) columns. In this case there is
    # no need to create a view (unless `dt` was a view): columns can be simply
    # shallow-copied.
    if isinstance(cols_node, (Slice_CSNode, Array_CSNode)) and not isinstance(rows_node, FilterExpr_RFNode):
        rowindex = rows_node.make_final_rowindex()
        columns = cols_node.get_result()
        res_dt = _datatable.datatable_assemble(rowindex, columns)
        return datatable.DataTable(res_dt, colnames=cols_node.column_names)

    return None
