#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
#-------------------------------------------------------------------------------
import datatable
from .rows_node import AllRFNode, SortedRFNode
from .cols_node import SliceCSNode, ArrayCSNode
from .sort_node import make_sort
from .context import make_engine
from .dtproxy import f

__all__ = ("make_datatable", )



def make_datatable(dt, rows, select, sort, engine):
    """
    Implementation of the `DataTable.__call__()` method.

    This is the "main" function in the module; it is responsible for
    evaluating various transformations when they are applied to a target
    DataTable.
    """
    with f.bind_datatable(dt):
        ee = make_engine(engine, dt)
        rowsnode = ee.make_rowfilter(rows)
        colsnode = ee.make_columnset(select)
        sortnode = make_sort(sort, dt)

        if sortnode:
            if isinstance(rowsnode, AllRFNode):
                rowsnode = SortedRFNode(sortnode)
            else:  # pragma: no cover
                raise NotImplementedError(
                    "Cannot yet apply sort argument to a view datatable or "
                    "combine with rows argument.")

        # Select subset of rows + subset of columns. In this case columns can
        # be simply copied by reference, and then the resulting datatable will
        # be either a plain "data" table if rowindex selects all rows and the
        # target datatable is not a view, or a "view" datatable otherwise.
        if isinstance(colsnode, (SliceCSNode, ArrayCSNode)):
            rowindex = rowsnode.get_final_rowindex()
            colsnode._rowindex = rowindex
            columns = ee.execute(colsnode)
            res_dt = columns.to_datatable()
            return datatable.DataTable(res_dt, names=colsnode.column_names)

        # Select computed columns + all rows from datatable which is not a
        # view -- in this case the rowindex is None, and the selected columns
        # can be copied by reference, while computed columns can be created
        # without the need to apply a RowIndex object. The DataTable created
        # will be a "data" table.
        if isinstance(rowsnode, AllRFNode) and not dt.internal.isview:
            columns = ee.execute(colsnode)
            res_dt = columns.to_datatable()
            return datatable.DataTable(res_dt, names=colsnode.column_names)

    raise RuntimeError(  # pragma: no cover
        "Unable to handle input (rows=%r, select=%r)"
        % (rows, select))
