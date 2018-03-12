#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable
from .rows_node import AllRFNode, SortedRFNode
from .cols_node import SliceCSNode, ArrayCSNode
from .sort_node import SortNode
from .groupby_node import make_groupby
from .context import make_engine
from .dtproxy import f
from datatable.utils.typechecks import TValueError

__all__ = ("make_datatable", "resolve_selector")



def make_datatable(dt, rows, select, groupby=None, sort=None, engine=None,
                   mode=None):
    """
    Implementation of the `Frame.__call__()` method.

    This is the "main" function in the module; it is responsible for
    evaluating various transformations when they are applied to a target
    Frame.
    """
    with f.bind_datatable(dt):
        ee = make_engine(engine, dt)
        ee.rowindex = dt.internal.rowindex
        rowsnode = ee.make_rowfilter(rows)
        grbynode = ee.make_groupby(groupby)
        colsnode = ee.make_columnset(select)
        sortnode = ee.make_sort(sort)

        if sortnode:
            if isinstance(rowsnode, AllRFNode) and not grbynode:
                rowsnode = SortedRFNode(sortnode)
            else:  # pragma: no cover
                raise NotImplementedError(
                    "Cannot yet apply sort argument to a view datatable or "
                    "combine with rows / groupby argument.")

        if mode == "delete":
            assert grbynode is None
            allcols = isinstance(colsnode, SliceCSNode) and colsnode.is_all()
            allrows = isinstance(rowsnode, AllRFNode)
            if allrows:
                if allcols:
                    dt._fill_from_dt(dt.__class__().internal)
                    return
                if isinstance(colsnode, (SliceCSNode, ArrayCSNode)):
                    colslist = sorted(set(colsnode.get_list()))
                    dt._delete_columns(colslist)
                    return
                raise TValueError("Cannot delete non-existing columns")
            elif allcols:
                rowsnode.execute()
                new_ri = ee.rowindex.inverse(dt.nrows)
                dt.internal.replace_rowindex(new_ri)
                dt._nrows = dt.internal.nrows
                return
            else:
                raise NotImplementedError("Deleting rows + columns from a Frame"
                                          " is not supported yet")

        rowsnode.execute()
        if grbynode:
            grbynode.execute()

        # Select subset of rows + subset of columns. In this case columns can
        # be simply copied by reference, and then the resulting datatable will
        # be either a plain "data" table if rowindex selects all rows and the
        # target datatable is not a view, or a "view" datatable otherwise.
        if isinstance(colsnode, (SliceCSNode, ArrayCSNode)):
            colsnode._rowindex = ee.rowindex
            columns = ee.execute(colsnode)
            res_dt = columns.to_datatable()
            return datatable.Frame(res_dt, names=colsnode.column_names)

        # Select computed columns + all rows from datatable which is not a
        # view -- in this case the rowindex is None, and the selected columns
        # can be copied by reference, while computed columns can be created
        # without the need to apply a RowIndex object. The Frame created
        # will be a "data" table.
        colsnode._rowindex = ee.rowindex
        columns = ee.execute(colsnode)
        res_dt = columns.to_datatable()
        return datatable.Frame(res_dt, names=colsnode.column_names)

    raise RuntimeError("Unable to calculate the result")  # pragma: no cover




def resolve_selector(item):
    rows = None
    if isinstance(item, tuple):
        if len(item) == 1:
            cols = item[0]
        elif len(item) == 2:
            rows, cols = item
        else:
            raise TValueError("Selector %r is not supported" % (item, ))
    else:
        cols = item
    return (rows, cols)
