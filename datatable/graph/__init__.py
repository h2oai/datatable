#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable
from .rows_node import AllRFNode, SortedRFNode, make_rowfilter
from .cols_node import SliceCSNode, ArrayCSNode, make_columnset
from .context import make_engine
from .groupby_node import make_groupby, by
from .sort_node import make_sort
from .join_node import join
from .dtproxy import f, g
from datatable.expr import BaseExpr
from datatable.lib import core
from datatable.utils.typechecks import TValueError

__all__ = ("make_datatable", "f", "g", "by", "join",
           "SliceCSNode")



def make_datatable(dt, rows, select, groupby=None, join=None, sort=None,
                   engine=None, mode=None, replacement=None):
    """
    Implementation of the `Frame.__call__()` method.

    This is the "main" function in the module; it is responsible for
    evaluating various transformations when they are applied to a target
    Frame.
    """
    if isinstance(groupby, datatable.join):
        join = groupby
        groupby = None
    update_mode = mode == "update"
    delete_mode = mode == "delete"
    jframe = join.joinframe if join else None
    with f.bind_datatable(dt), g.bind_datatable(jframe):
        ee = make_engine(engine, dt, jframe)
        ee.rowindex = dt.internal.rowindex
        rowsnode = make_rowfilter(rows, ee)
        grbynode = make_groupby(groupby, ee)
        colsnode = make_columnset(select, ee, update_mode)
        sortnode = make_sort(sort, ee)

        if join:
            join.execute(ee)

        if sortnode:
            if isinstance(rowsnode, AllRFNode) and not grbynode:
                rowsnode = SortedRFNode(sortnode)
            else:  # pragma: no cover
                raise NotImplementedError(
                    "Cannot yet apply sort argument to a view datatable or "
                    "combine with rows / groupby argument.")

        if delete_mode or update_mode:
            assert grbynode is None
            allcols = colsnode.is_all()
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
                        replacement = datatable.repeat(replacement, dt.nrows)
                elif isinstance(replacement, datatable.Frame):
                    pass
                elif isinstance(replacement, BaseExpr):
                    _col = replacement.evaluate_eager(ee)
                    _colset = core.columns_from_columns([_col])
                    replacement = _colset.to_frame(None)
                else:
                    replacement = datatable.Frame(replacement)
                rowsnode.execute()
                colsnode.execute_update(dt, replacement)
                return

        rowsnode.execute()
        if grbynode:
            grbynode.execute()

        colsnode.execute()
        res_dt = ee.columns.to_frame(colsnode.column_names)
        if grbynode and res_dt.nrows == dt.nrows:
            res_dt.internal.groupby = ee.groupby
        return res_dt

    raise RuntimeError("Unable to calculate the result")  # pragma: no cover
