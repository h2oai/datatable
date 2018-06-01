#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable
from .rows_node import AllRFNode, SortedRFNode
from .cols_node import SliceCSNode, ArrayCSNode
from .context import make_engine
from .groupby_node import make_groupby
from .sort_node import make_sort
from .dtproxy import f
from datatable.utils.typechecks import TValueError

__all__ = ("make_datatable",
           "make_groupby",
           "make_sort",
           "resolve_selector",
           "SliceCSNode")



def make_datatable(dt, rows, select, groupby=None, sort=None, engine=None,
                   mode=None, replacement=None):
    """
    Implementation of the `Frame.__call__()` method.

    This is the "main" function in the module; it is responsible for
    evaluating various transformations when they are applied to a target
    Frame.
    """
    update_mode = mode == "update"
    delete_mode = mode == "delete"
    with f.bind_datatable(dt):
        ee = make_engine(engine, dt)
        ee.rowindex = dt.internal.rowindex
        rowsnode = ee.make_rowfilter(rows)
        grbynode = ee.make_groupby(groupby)
        colsnode = ee.make_columnset(select,
                                     new_cols_allowed=update_mode)
        sortnode = ee.make_sort(sort)

        if sortnode:
            if isinstance(rowsnode, AllRFNode) and not grbynode:
                rowsnode = SortedRFNode(sortnode)
            else:  # pragma: no cover
                raise NotImplementedError(
                    "Cannot yet apply sort argument to a view datatable or "
                    "combine with rows / groupby argument.")

        if delete_mode or update_mode:
            assert grbynode is None
            allcols = isinstance(colsnode, SliceCSNode) and colsnode.is_all()
            allrows = isinstance(rowsnode, AllRFNode)
            if delete_mode:
                if allrows:
                    if allcols:
                        dt.__init__(None)
                        return
                    if isinstance(colsnode, (SliceCSNode, ArrayCSNode)):
                        colslist = sorted(set(colsnode.get_list()))
                        dt._delete_columns(colslist)
                        return
                    raise TValueError("Cannot delete non-existing columns")
                elif allcols:
                    rowsnode.negate()
                    rowsnode.execute()
                    dt.internal.replace_rowindex(ee.rowindex)
                    dt._nrows = dt.internal.nrows
                    return
                else:
                    update_mode = True
                    replacement = None
                    # fall-through to the update_mode
            if update_mode:
                # Without `materialize`, when an update is applied to a view,
                # `rowsnode.execute()` will merge the rowindex implied by
                # `rowsnode` with its parent's rowindex. This will cause the
                # parent's data to be updated, which is wrong.
                dt.materialize()
                if isinstance(replacement, (int, float, str, type(None))):
                    replacement = datatable.Frame([replacement])
                    if allrows:
                        replacement.resize(dt.nrows)
                elif not isinstance(replacement, datatable.Frame):
                    replacement = datatable.Frame(replacement)
                rowsnode.execute()
                colsnode.execute_update(dt, replacement)
                return

        rowsnode.execute()
        if grbynode:
            grbynode.execute()

        colsnode.execute()
        res_dt = ee.columns.to_datatable()
        if grbynode and res_dt.nrows == dt.nrows:
            res_dt.groupby = ee.groupby
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
