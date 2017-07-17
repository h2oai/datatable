#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import types

import _datatable
import datatable
from datatable.expr import DatatableExpr, BaseExpr
from .context import RequiresCModule
from .iterator_node import FilterNode
from datatable.utils.misc import normalize_slice, normalize_range
from datatable.utils.misc import plural_form as plural
from datatable.utils.typechecks import typed, TValueError, TTypeError



class RFNode(object):
    """Base class for all RowFilter nodes."""

    def __init__(self, dt):
        super().__init__()
        self._dt = dt

    def make_final_rowindex(self):
        dt = self._dt.internal
        ri = self.make_target_rowindex()
        if dt.isview:
            ri = _datatable.rowindex_uplift(ri, dt)
        return ri

    def make_target_rowindex(self):
        raise NotImplementedError



#===============================================================================

class AllRFNode(RFNode):
    """
    Class representing selection of all rows from the datatable.

    Although "all rows" selector can easily be implemented as a slice, we want
    to have a separate class because (1) this is a very common selector type,
    and (2) in some cases useful optimizations can be achieved if we know that
    all rows are selected from a datatable.
    """

    def make_target_rowindex(self):
        return None



#===============================================================================

class SliceRFNode(RFNode):

    def __init__(self, dt, start, count, step):
        super().__init__(dt)
        assert start >= 0 and count >= 0
        self._triple = (start, count, step)

    def make_target_rowindex(self):
        return _datatable.rowindex_from_slice(*self._triple)



#===============================================================================

class ArrayRFNode(RFNode):

    def __init__(self, dt, array):
        super().__init__(dt)
        self._array = array

    def make_target_rowindex(self):
        return _datatable.rowindex_from_array(self._array)



#===============================================================================

class MultiSliceRFNode(RFNode):

    def __init__(self, dt, bases, counts, steps):
        super().__init__(dt)
        self._bases = bases
        self._counts = counts
        self._steps = steps

    def make_target_rowindex(self):
        return _datatable.rowindex_from_slicelist(
            self._bases, self._counts, self._steps
        )



#===============================================================================

class DataColumnRFNode(RFNode):

    def __init__(self, dt):
        super().__init__(dt)
        self._column_dt = dt

    def make_target_rowindex(self):
        return _datatable.rowindex_from_column(self._column_dt.internal)



#===============================================================================

class FilterExprRFNode(RFNode, RequiresCModule):

    @typed(expr=BaseExpr)
    def __init__(self, dt, expr):
        super().__init__(dt)
        self._expr = expr
        self._fnode = FilterNode(expr)

    def _added_into_soup(self):
        self.soup.add("rows_filter", self._fnode)

    def make_target_rowindex(self):
        fnptr = self._fnode.get_result()
        nrows = self._fnode.nrows
        return _datatable.rowindex_from_filterfn(fnptr, nrows)

    def use_cmodule(self, cmod):
        self._fnode.use_cmodule(cmod)


#===============================================================================

def make_rowfilter(rows, dt, _nested=False):
    """
    Create a :class:`RowFilterNode` corresponding to descriptor `rows`.

    This is a factory function that instantiates an appropriate subclass of
    :class:`RowFilterNode`, depending on the provided argument `rows`, assuming
    it is applied to the datatable `dt`.

    :param rows: one of the following:
    :param dt: the datatable
    """
    nrows = dt.nrows
    if rows is Ellipsis or rows is None:
        return AllRFNode(dt)

    if rows is True or rows is False:
        # Note: True/False are integer objects in Python
        raise TTypeError("Boolean value cannot be used as a `rows` selector")

    # from_scalar = False
    if isinstance(rows, (int, slice, range)):
        # from_scalar = True
        rows = [rows]

    from_generator = False
    if isinstance(rows, types.GeneratorType):
        # If an iterator is given, materialize it first. Otherwise there
        # is no way to ensure that the produced indices are valid.
        rows = list(rows)
        from_generator = True

    if isinstance(rows, (list, tuple, set)):
        bases = []
        counts = []
        steps = []
        for i, elem in enumerate(rows):
            if isinstance(elem, int):
                if -nrows <= elem < nrows:
                    # `elem % nrows` forces the row number to become positive
                    bases.append(elem % nrows)
                else:
                    raise TValueError(
                        "Row `%d` is invalid for datatable with %s"
                        % (elem, plural(nrows, "row")))
            elif isinstance(elem, (range, slice)):
                if elem.step == 0:
                    raise TValueError("In %r step must not be 0" % elem)
                if not all(x is None or isinstance(x, int)
                           for x in (elem.start, elem.stop, elem.step)):
                    raise TValueError("%r is not integer-valued" % elem)
                if isinstance(elem, range):
                    res = normalize_range(elem, nrows)
                    if res is None:
                        raise TValueError(
                            "Invalid %r for a datatable with %s"
                            % (elem, plural(nrows, "row")))
                else:
                    res = normalize_slice(elem, nrows)
                start, count, step = res
                assert count >= 0
                if count == 0:
                    pass  # don't do anything
                elif count == 1:
                    bases.append(start)
                else:
                    if len(counts) < len(bases):
                        counts += [1] * (len(bases) - len(counts))
                        steps += [1] * (len(bases) - len(steps))
                    bases.append(start)
                    counts.append(count)
                    steps.append(step)
            else:
                if from_generator:
                    raise TValueError(
                        "Invalid row selector %r generated at position %d"
                        % (elem, i))
                else:
                    raise TValueError(
                        "Invalid row selector %r at element %d of the "
                        "`rows` list" % (elem, i))
        if not counts:
            if len(bases) == 1:
                return SliceRFNode(dt, bases[0], 1, 1)
            else:
                return ArrayRFNode(dt, bases)
        elif len(bases) == 1:
            if bases[0] == 0 and counts[0] == nrows and steps[0] == 1:
                return AllRFNode(dt)
            else:
                return SliceRFNode(dt, bases[0], counts[0], steps[0])
        else:
            return MultiSliceRFNode(dt, bases, counts, steps)

    if isinstance(rows, datatable.DataTable):
        if rows.ncols != 1:
            raise TValueError("`rows` argument should be a single-column "
                              "datatable, got %r" % rows)
        col0type = rows.types[0]
        if col0type != "bool":
            raise TTypeError("`rows` datatable should be a boolean column, "
                             "however it has type %s" % col0type)
        if rows.nrows != dt.nrows:
            s1rows = plural(rows.nrows, "row")
            s2rows = plural(dt.nrows, "row")
            raise TValueError("`rows` datatable has %s, but applied to a "
                              "datatable with %s" % (s1rows, s2rows))
        return DataColumnRFNode(rows)

    if isinstance(rows, types.FunctionType):
        lazydt = DatatableExpr(dt)
        return make_rowfilter(rows(lazydt), dt, _nested=True)

    if isinstance(rows, BaseExpr):
        return FilterExprRFNode(dt, rows)

    if _nested:
        raise TTypeError("Unexpected result produced by the `rows` "
                         "function: %r" % rows)
    else:
        raise TTypeError("Unexpected `rows` argument: %r" % rows)
