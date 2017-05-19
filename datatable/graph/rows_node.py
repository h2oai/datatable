#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import types

import datatable
from datatable.expr import DatatableExpr, BaseExpr
from datatable.graph.node import Node
from datatable.graph.iterator_node import FilterNode
from datatable.utils.misc import normalize_slice, normalize_range
from datatable.utils.misc import plural_form as plural
from datatable.utils.typechecks import typed, ValueError, TypeError



#===============================================================================

class RowFilterNode(Node):
    """
    Base class for various row filters.

    A RowFilter encapsulates the `rows` argument in the main datatable
    evaluation function. This node is designed in such a way that it always
    produces a `RowMapping*` object in the C layer. In particular, this node
    does not fuse with any other nodes (for example with the `ColumnSetNode`).
    This is because it is not generally possible to know in advance the size
    of the rowmapping produced, which means that the output array has to be
    over-allocated and then its chunks moves around in order to assemble the
    final rowmapping (when executing in parallel). If we were to fuse the
    creation of rowmapping with the creation of output columns, then complexity
    of the code and the memory footprint and the number of memmoves would have
    all significantly increased probably resulting in poor performance.

    Also note that the rowmapping produced by this node always applies to the
    original datatable to which the main evaluation function was applied. If
    the original datatable is in fact a view, then the resulting rowmapping
    cannot be used as-is (view on a view is not allowed). The conversion of
    the produced rowmapping into the rowmapping applied to the source datatable
    is done in a separate step, and that step is already fusable with the
    ColumnSet production.
    """
    def __init__(self):
        super().__init__()
        self._cname = None


    def cget_rowmapping(self, context):
        """
        Return name of the C function that when executed creates a RowMapping.

        Specifically, this function will insert into the `context` the C code
        needed to construct and return the RowMapping. The C function will
        memoize its result, so that calling it multiple times is efficient.
        Likewise, this `cget_rowmapping` method can also be called multiple
        times safely. The generated C function will have the signature

            RowMapping* cget_rowmapping(void);
        """
        if not self._cname:
            self._cname = self._gen_c(context)
        return self._cname

    #---- Private/protected ----------------------------------------------------

    def _gen_c(self, context):
        (cbody, res) = self._gen_c_body(context)
        fn = "static RowMapping* get_rowmapping(void) {\n"
        fn += "    if (rowmapping == NULL) {\n"
        fn += cbody
        fn += "        rowmapping = %s;\n" % res
        fn += "    }\n"
        fn += "    return rowmapping;\n"
        fn += "}\n"
        context.add_global("rowmapping", "RowMapping*", "NULL")
        context.add_function("get_rowmapping", fn)
        return "get_rowmapping"

    def _gen_c_body(self, context):
        raise NotImplementedError




#===============================================================================

class Slice_RFNode(RowFilterNode):

    def __init__(self, start, count, step):
        super().__init__()
        assert start >= 0 and count >= 0
        self._start = start
        self._count = count
        self._step = step

    def _gen_c_body(self, context):
        context.add_extern("rowmapping_from_slice")
        expr = ("rowmapping_from_slice(%d, %d, %d)"
                % (self._start, self._count, self._step))
        return ("", expr)



#===============================================================================

class Array_RFNode(RowFilterNode):

    def __init__(self, array):
        super().__init__()
        self._array = array

    def _gen_c_body(self, context):
        context.add_extern("rowmapping_from_pyarray")
        body = "        PyObject *list = (PyObject*) %dLL;\n" % id(self._array)
        expr = "rowmapping_from_pyarray(list)"
        return (body, expr)



#===============================================================================

class MultiSlice_RFNode(RowFilterNode):

    def __init__(self, bases, counts, steps):
        super().__init__()
        self._bases = bases
        self._counts = counts
        self._steps = steps

    def _gen_c_body(self, context):
        context.add_extern("rowmapping_from_pyslicelist")
        body = ""
        body += "        PyObject *a = (PyObject*) %dLL;\n" % id(self._bases)
        body += "        PyObject *b = (PyObject*) %dLL;\n" % id(self._counts)
        body += "        PyObject *c = (PyObject*) %dLL;\n" % id(self._steps)
        expr = "rowmapping_from_pyslicelist(a, b, c)"
        return (body, expr)



#===============================================================================

class DataColumn_RFNode(RowFilterNode):

    def __init__(self, dt):
        super().__init__()
        self._column_dt = dt

    def _gen_c_body(self, context):
        context.add_extern("rowmapping_from_datacolumn")
        dt_ptr = id(self._column_dt.internal)
        body = "        DataTable *dt = ((DataTable_PyObject*) %dLL)->ref;\n" \
               % dt_ptr
        expr = "rowmapping_from_datacolumn(dt->columns[0], dt->nrows)"
        return (body, expr)



#===============================================================================

class FilterExpr_RFNode(RowFilterNode):

    @typed(expr=BaseExpr)
    def __init__(self, expr):
        super().__init__()
        self._expr = expr

    def _gen_c_body(self, context):
        context.add_extern("rowmapping_from_filterfn32")
        filter_node = FilterNode(self._expr)
        f = filter_node.generate_c(context)
        nrows = filter_node.nrows
        return ("", "rowmapping_from_filterfn32(&%s, %d)" % (f, nrows))



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
    if rows is Ellipsis:
        return Slice_RFNode(0, nrows, 1)

    # from_scalar = False
    if isinstance(rows, (int, slice, range)):
        # from_scalar = True
        rows = [rows]

    from_generator = False
    if isinstance(rows, types.GeneratorType):
        # If an iterator is given, materialize it first. Otherwise there
        # is no way of telling whether the produced indices are valid.
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
                    raise ValueError(
                        "Row `%d` is invalid for datatable with %s"
                        % (elem, plural(nrows, "row")))
            elif isinstance(elem, (range, slice)):
                if elem.step == 0:
                    raise ValueError("In %r step must not be 0" % elem)
                if not all(x is None or isinstance(x, int)
                           for x in (elem.start, elem.stop, elem.step)):
                    raise ValueError("%r is not integer-valued" % elem)
                if isinstance(elem, range):
                    res = normalize_range(elem, nrows)
                    if res is None:
                        raise ValueError(
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
                    raise ValueError(
                        "Invalid row selector %r generated at position %d"
                        % (elem, i))
                else:
                    raise ValueError(
                        "Invalid row selector %r at element %d of the "
                        "`rows` list" % (elem, i))
        if not counts:
            if len(bases) == 1:
                return Slice_RFNode(bases[0], 1, 1)
            elif len(bases) == 2:
                return Slice_RFNode(bases[0], 2, bases[1] - bases[0])
            else:
                return Array_RFNode(bases)
        elif len(bases) == 1:
            return Slice_RFNode(bases[0], counts[0], steps[0])
        else:
            return MultiSlice_RFNode(bases, counts, steps)

    if isinstance(rows, datatable.DataTable):
        if rows.ncols != 1:
            raise ValueError("`rows` argument should be a single-column "
                             "datatable, got %r" % rows)
        col0type = rows.types[0]
        if col0type != "bool":
            raise TypeError("`rows` datatable should be a boolean column, "
                            "however it has type %s" % col0type)
        if rows.nrows != dt.nrows:
            s1rows = plural(rows.nrows, "row")
            s2rows = plural(dt.nrows, "row")
            raise ValueError("`rows` datatable has %s, but applied to a "
                             "datatable with %s" % (s1rows, s2rows))
        return DataColumn_RFNode(rows)

    if isinstance(rows, types.FunctionType):
        lazydt = DatatableExpr(dt)
        return make_rowfilter(rows(lazydt), dt, _nested=True)

    if isinstance(rows, BaseExpr):
        return FilterExpr_RFNode(rows)

    if _nested:
        raise TypeError("Unexpected result produced by the `rows` "
                        "function: %r" % rows)
    else:
        raise TypeError("Unexpected `rows` argument: %r" % rows)
