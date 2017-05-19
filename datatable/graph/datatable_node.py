#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

import datatable
from .node import Node
from .context import EvaluationContext
from .rows_node import RowFilterNode
from .cols_node import ColumnSetNode
from datatable.utils.typechecks import typed



#===============================================================================

class DatatableEvaluatorNode(Node):
    """
    Root node for a datatable evaluation graph.

    This class takes as arguments: a :class:`RowFilterNode`, a
    :class:`ColumnSetNode`, a :class:`GroupByNode`, etc -- and binds them
    together into a single evaluation graph.

    Unlike any other node in the graph, this class contains an :method:`execute`
    method that does the following: (1) prepares an evalution context where the
    graph can be materialized, (2) generates C code for each node in the graph,
    (3) JIT-compiles the code and injects the binary into the runtime, (4)
    invokes the "main" function in that module to produce the resulting
    datatable, and finally (5) returns the datatable produced.

    Example:

        dtnode = DatatableEvaluatorNode(rows_node, select_node, ...)
        dtout = dtnode.execute()

    """

    @typed(rows=RowFilterNode, select=ColumnSetNode)
    def __init__(self, dt, rows=None, select=None):
        super().__init__()
        self._dt = dt
        self._rows = rows
        self._select = select


    def execute(self, verbose=False):
        self.use_context(EvaluationContext())
        dtgetter = self.cget_datatable()
        _dt = self.context.execute(dtgetter, verbose=verbose)
        return datatable.DataTable(_dt, colnames=self._select.column_names)



    def cget_datatable(self):
        rowmapping = self._rows.cget_rowmapping()
        columns = self._select.cget_columns()
        fnname = self.context.make_variable_name("get_datatable")
        fn = "PyObject* %s(void) {\n" % fnname
        fn += "    init();\n"

        if self._select.n_view_columns == 0:
            fn += self._gen_cbody_only_data_cols(rowmapping, columns)
        else:
            assert not self._select.dt.internal.isview
            fn += self._gen_cbody_dt_not_view(rowmapping, columns)

        fn += "}\n"
        self._context.add_function(fnname, fn)
        return fnname


    def on_context_set(self):
        self._rows.use_context(self.context)
        self._select.use_context(self.context)


    def _gen_cbody_only_data_cols(self, frowmapping, fcolumns):
        self.context.add_extern("pydatatable_assemble")
        fn = ""
        fn += "    int64_t nrows = %s()->nrows;\n" % frowmapping
        fn += "    Column** columns = %s();\n" % fcolumns
        fn += "    return pydatatable_assemble(nrows, columns);\n"
        return fn


    def _gen_cbody_dt_not_view(self, frowmapping, fcolumns):
        self.context.add_extern("pydatatable_assemble_view")
        return ("    RowMapping *rm = {make_rowmapping}();\n"
                "    Column **columns = {make_columns}();\n"
                "    return pydatatable_assemble_view"
                "((DataTable_PyObject*){dtptr}, rm, columns);\n"
                .format(dtptr=id(self._dt.internal),
                        make_rowmapping=frowmapping,
                        make_columns=fcolumns))
