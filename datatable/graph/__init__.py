#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
#-------------------------------------------------------------------------------
import _datatable
import datatable
from .rows_node import make_rowfilter, AllRFNode, SortedRFNode
from .cols_node import make_columnset, SliceCSNode, ArrayCSNode
from .sort_node import make_sort
from .context import CModuleNode, RequiresCModule
from datatable.utils.typechecks import TTypeError

__all__ = ("make_datatable", )



def make_datatable(dt, rows, select, sort):
    """
    Implementation of the `DataTable.__call__()` method.

    This is the "main" function in the module; it is responsible for
    evaluating various transformations when they are applied to a target
    DataTable.
    """
    cmodule = CModuleNode()
    rows_node = make_rowfilter(rows, dt, cmodule)
    cols_node = make_columnset(select, dt)
    sort_node = make_sort(sort, dt)

    if isinstance(cols_node, RequiresCModule):
        cols_node.use_cmodule(cmodule)
    if sort_node:
        if isinstance(rows_node, AllRFNode):
            rows_node = SortedRFNode(sort_node)
        else:  # pragma: no cover
            raise TTypeError("Cannot apply sort argument to a view datatable "
                             "or combine with rows argument (yet).")

    # Select some (or all) rows + some (or all) columns. In this case columns
    # can be simply copied by reference, and then the resulting datatable will
    # be either a plain "data" table if rowindex selects all rows and the target
    # datatable is not a view, or a "view" datatable otherwise.
    if isinstance(cols_node, (SliceCSNode, ArrayCSNode)):
        rowindex = rows_node.get_final_rowindex()
        columns = cols_node.get_result()
        res_dt = _datatable.datatable_assemble(rowindex, columns)
        return datatable.DataTable(res_dt, colnames=cols_node.column_names)

    # Select computed columns + all rows from datatable which is not a view --
    # in this case the rowindex is None, and the selected columns can be copied
    # by reference, while computed columns can be created without the need to
    # apply a RowIndex object. The DataTable created will be a "data" table.
    if isinstance(rows_node, AllRFNode) and not dt.internal.isview:
        columns = cols_node.get_result()
        res_dt = _datatable.datatable_assemble(None, columns)
        return datatable.DataTable(res_dt, colnames=cols_node.column_names)

    raise RuntimeError(  # pragma: no cover
        "Unable to handle input (rows=%r, select=%r)"
        % (rows, select))
