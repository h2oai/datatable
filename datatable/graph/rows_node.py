#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import types

import _datatable
import datatable
from datatable.expr import DatatableExpr, BaseExpr
from .node import Node
from .iterator_node import FilterNode
from datatable.utils.misc import normalize_slice, normalize_range
from datatable.utils.misc import plural_form as plural
from datatable.utils.typechecks import typed, TValueError, TTypeError



#===============================================================================

class RowFilterNode(Node):
    """
    Base class for nodes that filter datatable's rows creating a RowIndex.

    A RowFilter encapsulates the `rows` argument in the main datatable
    evaluation function. This node is designed in such a way that it always
    produces a `RowIndex*` object in the C layer. In particular, this node
    does not fuse with any other nodes (for example with the `ColumnSetNode`).
    This is because it is not generally possible to know in advance the size
    of the rowindex produced, which means that the output array has to be
    over-allocated and then its chunks moved around in order to assemble the
    final rowindex (when executing in parallel). If we were to fuse the
    creation of rowindex with the creation of output columns, then complexity
    of the code and the memory footprint and the number of memmoves would have
    all significantly increased probably resulting in poor performance.

    Also note that the rowindex produced by this node always applies to the
    original datatable to which the main evaluation function was applied. If
    the original datatable is in fact a view, then the resulting rowindex
    cannot be used as-is (view on a view is not allowed). The conversion of
    the produced rowindex into the rowindex applied to the source datatable
    is done in a separate step, and that step is already fusable with the
    ColumnSet production.
    """

    def __init__(self, rows, dt):
        super().__init__()
        self._target = _make_rowfilter(rows, dt)

    def _added_into_soup(self):
        self.soup.add("rows:target", self._target)

    def get_result(self):
        return self._target.get_result()



class RFNode(Node):
    """Base class for all RowFilter nodes."""

    def __init__(self, dt):
        super().__init__()
        self._dt = dt

    def make_final_rowindex(self):
        dt = self._dt.internal
        ri = self.make_target_rowindex()
        if dt.isview:
            return _datatable.rowindex_merge(ri, dt.rowindex)
        else:
            return ri

    def make_target_rowindex(self):
        raise NotImplementedError



#===============================================================================

class All_RFNode(RFNode):
    """
    Class representing selection of all rows from the datatable.

    Although "all rows" selector can easily be implemented as a slice, we want
    to have a separate class because (1) this is a very common selector type,
    and (2) in some cases useful optimizations can be achieved if we know that
    all rows are selected from a datatable.
    """

    def make_target_rowindex(self):
        return None

    def get_result(self):  # temporary
        t = Slice_RFNode(self._dt, 0, self._dt.nrows, 1)
        return t.get_result()


#===============================================================================

class Slice_RFNode(RFNode):

    def __init__(self, dt, start, count, step):
        super().__init__(dt)
        assert start >= 0 and count >= 0
        self._triple = (start, count, step)

    def get_result(self):
        return _datatable.rowindex_from_slice(*self._triple)

    def make_target_rowindex(self):
        return _datatable.rowindex_from_slice(*self._triple)


#===============================================================================

class Array_RFNode(Node):

    def __init__(self, array):
        super().__init__()
        self._array = array

    def get_result(self):
        return _datatable.rowindex_from_array(self._array)



#===============================================================================

class MultiSlice_RFNode(Node):

    def __init__(self, bases, counts, steps):
        super().__init__()
        self._bases = bases
        self._counts = counts
        self._steps = steps

    def get_result(self):
        return _datatable.rowindex_from_slicelist(
            self._bases, self._counts, self._steps
        )



#===============================================================================

class DataColumn_RFNode(Node):

    def __init__(self, dt):
        super().__init__()
        self._column_dt = dt

    def get_result(self):
        return _datatable.rowindex_from_column(self._column_dt.internal)



#===============================================================================

class FilterExpr_RFNode(Node):

    @typed(expr=BaseExpr)
    def __init__(self, expr):
        super().__init__()
        self._expr = expr
        self._fnode = None

    def _added_into_soup(self):
        self._fnode = FilterNode(self._expr)
        self.soup.add("rows_filter", self._fnode)

    def get_result(self):
        fnptr = self._fnode.get_result()
        nrows = self._fnode.nrows
        return _datatable.rowindex_from_filterfn(fnptr, nrows)



#===============================================================================

def _make_rowfilter(rows, dt, _nested=False):
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
        return All_RFNode(dt)

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
                return Slice_RFNode(dt, bases[0], 1, 1)
            else:
                return Array_RFNode(bases)
        elif len(bases) == 1:
            if bases[0] == 0 and counts[0] == nrows and steps[0] == 1:
                return All_RFNode(dt)
            else:
                return Slice_RFNode(dt, bases[0], counts[0], steps[0])
        else:
            return MultiSlice_RFNode(bases, counts, steps)

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
        return DataColumn_RFNode(rows)

    if isinstance(rows, types.FunctionType):
        lazydt = DatatableExpr(dt)
        return _make_rowfilter(rows(lazydt), dt, _nested=True)

    if isinstance(rows, BaseExpr):
        return FilterExpr_RFNode(rows)

    if _nested:
        raise TTypeError("Unexpected result produced by the `rows` "
                         "function: %r" % rows)
    else:
        raise TTypeError("Unexpected `rows` argument: %r" % rows)
