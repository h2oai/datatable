#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import os
import types
from typing import Dict, Tuple

# noinspection PyUnresolvedReferences
import _datatable as c
from .widget import DataFrameWidget

from datatable.exec import EvaluationModule
from datatable.expr import DatatableExpr, ExprNode, ColSelectorExpr
from datatable.utils.misc import normalize_slice, normalize_range, timeit
from datatable.utils.misc import plural_form as plural
from datatable.utils.typechecks import TypeError, ValueError, typed

__all__ = ("DataTable", )


class DataTable(object):
    _id_counter_ = 0

    def __init__(self, src=None, colnames=None):
        DataTable._id_counter_ += 1
        self._id = DataTable._id_counter_  # type: int
        self._ncols = 0      # type: int
        self._nrows = 0      # type: int
        self._types = None   # type: Tuple[str]
        self._stypes = None  # type: Tuple[str]
        self._names = None   # type: Tuple[str]
        # Mapping of column names to their indices
        self._inames = None  # type: Dict[str, int]
        self._dt = None      # type: c.DataTable
        self._fill_from_source(src, colnames=colnames)



    #---------------------------------------------------------------------------
    # Basic properties
    #---------------------------------------------------------------------------

    @property
    def nrows(self):
        """Number of rows in the datatable."""
        return self._nrows

    @property
    def ncols(self):
        """Number of columns in the datatable."""
        return self._ncols

    @property
    def shape(self):
        """Tuple (number of rows, number of columns)."""
        return (self._nrows, self._ncols)

    @property
    def names(self):
        """Tuple of column names."""
        return self._names

    @property
    def types(self):
        """Tuple of column types."""
        return self._types

    @property
    def stypes(self):
        """Tuple of column storage types."""
        if self._stypes is None:
            self._stypes = self._dt.stypes
        return self._stypes

    @property
    def internal(self):
        """Access to the underlying C DataTable object."""
        return self._dt


    #---------------------------------------------------------------------------
    # Display
    #---------------------------------------------------------------------------

    def __repr__(self):
        srows = plural(self._nrows, "row")
        scols = plural(self._ncols, "col")
        return "<DataTable #%d (%s x %s)>" % (self._id, srows, scols)

    def _display_in_terminal_(self):
        self.view()

    def _data_viewer(self, row0, row1, col0, col1):
        view = self._dt.window(row0, row1, col0, col1)
        return {
            "names": self._names[col0:col1],
            "types": view.types,
            "stypes": view.stypes,
            "columns": view.data,
        }

    def view(self):
        widget = DataFrameWidget(self._nrows, self._ncols, self._data_viewer)
        widget.render()


    @typed(colidx=int)
    def _hex(self, colidx):
        if not (-self.ncols <= colidx < self.ncols):
            raise ValueError("Invalid column index %d" % colidx)
        if colidx < 0:
            colidx += self.ncols
        col = self.internal.column(colidx)
        names = ["%02X" % i for i in range(16)] + [""]

        def data_viewer(row0, row1, col0, col1):
            view = c.DataWindow(self._dt, row0, row1, col0, col1, colidx)
            return {
                "names": names[col0:col1],
                "types": view.types,
                "stypes": view.stypes,
                "columns": view.data,
            }

        print("Column %d, Name: %r" % (colidx, self._names[colidx]))
        print("Ltype: %s, Stype: %s, Mtype: %s"
              % (col.ltype, col.stype, col.mtype))
        if col.isview:
            print("Column index in the source datatable: %d" % col.srcindex)
            return
        else:
            datasize = col.data_size
            print("Data size: %d" % datasize)
            print("Meta: %s" % col.meta)
            widget = DataFrameWidget((datasize + 15) // 16, 17, data_viewer)
            widget.render()


    #---------------------------------------------------------------------------
    # Initialization helpers
    #---------------------------------------------------------------------------

    def _fill_from_source(self, src, colnames):
        if isinstance(src, list):
            if isinstance(src[0], list):
                self._fill_from_list(src, names=colnames)
            else:
                self._fill_from_list([src], names=colnames)
        elif isinstance(src, (tuple, set)):
            self._fill_from_list(list(src), names=colnames)
        elif isinstance(src, dict):
            self._fill_from_list(list(src.values()), names=tuple(src.keys()))
        elif isinstance(src, c.DataTable):
            self._fill_from_dt(src, names=colnames)
        else:
            self._dt = c.DataTable()


    def _fill_from_list(self, src, names=None):
        self._fill_from_dt(c.datatable_from_list(src), names=names)


    def _fill_from_pdt(self, dirname):
        with open(os.path.join(dirname, "_info.pdt")) as inp:
            nrows = int(next(inp))
            columns = []
            colnames = []
            for line in inp:
                stype, colname, f = line.strip().split(" ", 3)
                columns.append(os.path.join(dirname, f))
                colnames.append(colname)
        self._fill_from_dt(c.dt_from_memmap(columns), names=colnames)
        assert self.nrows == nrows, "Wrong number of rows read: %d" % self.nrows


    def _fill_from_dt(self, dt, names=None):
        self._dt = dt
        self._ncols = dt.ncols
        self._nrows = dt.nrows
        self._types = dt.types
        if not names:
            names = tuple("C%d" % (i + 1) for i in range(self._ncols))
        if not isinstance(names, tuple):
            names = tuple(names)
        assert len(names) == self._ncols
        self._names = names
        self._inames = {n: i for i, n in enumerate(names)}



    #---------------------------------------------------------------------------
    # Main processor function
    #---------------------------------------------------------------------------

    @timeit
    def __call__(self, rows=None, select=None, verbose=False
                 #update=None, groupby=None, join=None, sort=None, limit=None
                 ):
        """
        Perform computation on a datatable, and return the result.

        :param rows:
            Which rows to operate upon. Could be one of the following:

                - ... (ellipsis), representing all rows of the datatable.
                - an integer, representing a single row at the given index. The
                  rows are numbered starting from 0. Negative indices are
                  allowed, indicating rows counted from the end of the
                  datatable (i.e. -1 is the last row).
                - a slice, representing some ordered subset of rows. The slice
                  has exactly the same semantics as in Python, for example
                  `slice(None, 10)` selects the first 10 rows, and
                  `slice(None, None, -1)` selects all rows in reverse.
                - a range, also representing some subset of rows. The range has
                  the semantics of a list into which this range would expand.
                  This is very similar to a slice, except with regard
                  to negative indices. For example in order to select all rows
                  in reverse for a datatable with N rows, you'd write
                  `range(N-1, -1, -1)`, whereas a slice with the same triple of
                  parameters produces a 0-rows result (because `N - 1` and `-1`
                  is the same row).
                - a list / tuple / generator of integers, slices, or ranges.
                - a ``DataTable`` with a single boolean column and having same
                  number of rows as the current datatable, this will select
                  only those rows in the current datatable where the provided
                  column has truthful value
                - a function that takes a single parameter -- the current
                  datatable -- and returns any of the selectors mentioned
                  above. Within this function, the frame behaves lazily.

        :param select:
            When this parameter is specified, a new datatable will be computed
            and returned from this call. This parameter cannot be combined with
            ``update``. Possible values:

                - ..., to select all columns in the current frame
                - an integer, selecting a single column at the given index
                - a string, selecting a single column by name
                - a slice, selecting a range of columns
                - a Mapper object, bound to one (or more) columns of the current
                  datatable. This object is callable, taking the per-row value
                  of the bound column, and producing a single result or a list
                  of results. When a list is produced, it will be used to create
                  as many columns in the resulting datatable as there are
                  elements in the list. The Mapper may also explicitly specify
                  the name/type of the column(s) it produces. If any of the
                  names already exist in the datatable, an exception will be
                  raised.
                - a Reducer object, bound to one (or more) columns of the
                  current datatable. This object is a callable, taking a list
                  (or list of lists) of values for each row of the current
                  datatable, and returning a single output (or a list of
                  outputs). The Reducer may also explicitly specify the name/
                  type of the column(s) it produces.
                - a list or tuple or dictionary of any of the above. A list or
                  a tuple will create multiple columns in the resulting
                  datatable having same names as in the current datatable. When
                  a dict is used, the columns will be renamed according to the
                  keys of the dictionary. Reducers cannot be combined with any
                  other selectors.
                - a function that takes a single argument -- the current
                  datatable -- and returns any of the selectors above. Within
                  the function any operations on the frame will be lazy.

        :param verbose:
            Lots of output, for debug purposes mainly.
        """
        """
        :param update:
            When this parameter is specified, it causes an in-place
            modification of the current datatable. This parameter is exclusive
            with ``select``. Possible values:

                - a dictionary ``{str: Mapper}``, where each ``Mapper`` is
                  bound to one or more columns of the current datatable. The
                  mapper must return a single value (list of values is not
                  allowed), and it will be stored in the column given by the
                  corresponding key in the dictionary. If a column with same
                  name already exists, it will be replaced; otherwise a new
                  column will be added.
                - a list of ``Mapper``s each bound to one or more columns of
                  the current datatable. These mappers will operate on the
                  datatable row-by-row, producing one or more outputs (in case
                  a list of outputs is returned, multiple columns will be
                  created by each mapper). The results will be appended to the
                  current datatable with automatically generated column names.
                  The mappers may also explicitly specify the name(s)/type(s)
                  of columns produce; if any of these names already exist in
                  the datatble, these columns will be replaced.
                - a list of ``Reducer``s (or single reducer), which will
                  produce a constant column having the value produced by the
                  reducer after running on all rows of the current datatable.
                - a function that takes a single argument -- the current
                  datatable -- and returns any of the selectors above. Within
                  the function any operations on the frame will be lazy.

        :param groupby:
            When this parameter is specified, it will perform a "group-by"
            operation on the datatable. The ``select``/``update`` clauses in
            this case may contain only ``Reducer``s, or the columns specified
            in the groupby, or mappers bound to the columns specified in the
            groupby. Then each reducer will be executed within the subset of
            rows for each group. When used with a select clause, the produced
            datatable will contain as many rows as there are distinct groups
            in the current datatable. When used with an update clause, the
            new columns will have constant reduced value within each group.
            Possible values for the parameter:

                - an integer, specifying column's index
                - a string, selecting a single column by name
                - a Mapper object bound to one or more columns of the current
                  datatable -- the mapped values will be used to produce the
                  groupby values.
                - a list or a tuple or a dict of the above. If a dictionary is
                  given, then it specifies how to rename the columns within
                  the groupby.
                - a function taking the current datatable as an argument, and
                  producing any of the groupby selectors listed above. Within
                  this function all datatable operations are lazy.

        :param join:
            Specifies another datatable to join with. If this parameter is
            given, then the "function" argument within ``rows``, ``select``
            and ``update`` will be passed two parameters instead of one: the
            current datatable, and the ``join`` datatable. The join condition
            should be expressed in the ``rows`` parameter.

        :param sort:
            When specified, the datatable will be sorted. If used with
            ``select``, it will sort the resulting datatable. If there is no
            ``select`` or ``update``, it will sort the current datatable
            in-place. Cannot be used together with ``update``.

            Possible values are same as for the ``groupby`` parameter. The
            ``sort`` argument may refer to the names of the columns being
            produced by the select/update clauses. Additionally, every column
            specified may be wrapped in a ``dt.reverse()`` call, reversing the
            sorting direction for that column.

        :param limit:
            If an integer, then no more than that many rows will be returned by
            the ``select`` clause. This can also be a slice, which effectively
            applies that slice to the resulting datatable.
        """
        if select is None:
            select = Ellipsis

        rows_selector = self._rows_selector(rows)
        cols = self._cols_selector(select)

        # Number of columns that are referenced from the parent datatable
        n_col_refs = sum(isinstance(col[0], (ColSelectorExpr, int))
                         for col in cols)
        if verbose:
            print("Selecting %d columns, where %d of them are references" %
                  (len(cols), n_col_refs))

        if n_col_refs == len(cols):
            colidxs = [
                col[0].colid if isinstance(col[0], ColSelectorExpr) else col[0]
                for col in cols
            ]
            colmapping = c.colmapping_from_array(colidxs, self._dt)

            if isinstance(rows_selector, DataTable):
                rowmapping = c.rowmapping_from_column(rows_selector)
            elif isinstance(rows_selector, ExprNode):
                mod = EvaluationModule(rows=rows_selector)
                mod.run(verbose)
                rowmapping = mod.rowmapping
            else:
                rowmapping = rows_selector

            res = DataTable()
            res._dt = self._dt(rowmapping, colmapping)
            res._ncols = res._dt.ncols
            res._nrows = res._dt.nrows
            res._names = tuple(name for _, name in cols)
            res._inames = {n: i for i, n in enumerate(res._names)}
            res._types = self._types
            return res

        raise NotImplementedError


    def _rows_selector(self, arg, nested=False):
        """
        Normalize the selector given by ``arg`` and ensure its correctness.

        :param arg: same as parameter ``rows`` in self.__call__
        :param nested:
        :return: one of:
            * a :class:`RowMapping` object,
            * a single-boolean-column :class:`DataTable`,
            * an :class:`ExprNode` with boolean `stype`.
        """
        if arg is Ellipsis:
            return c.rowmapping_from_slice(0, self.nrows, 1)

        if isinstance(arg, (int, slice, range)):
            arg = [arg]

        from_generator = False
        if isinstance(arg, types.GeneratorType):
            # If an iterator is given, materialize it first. Otherwise there
            # is no way of telling whether the produced indices are valid.
            arg = list(arg)
            from_generator = True

        if isinstance(arg, (list, tuple, set)):
            nrows = self._nrows
            bases = []
            counts = []
            strides = []
            for i, elem in enumerate(arg):
                if isinstance(elem, int):
                    if -nrows <= elem < nrows:
                        if elem < 0:
                            elem += nrows
                        bases.append(elem)
                    else:
                        raise ValueError(
                            "datatable contains %s; row number %d is invalid"
                            % (plural(nrows, "row"), elem))
                elif isinstance(elem, (range, slice)):
                    if elem.step == 0:
                        raise ValueError("In %r step must not be 0" % elem)
                    if not all(x is None or isinstance(x, int)
                               for x in (elem.start, elem.stop, elem.step)):
                        self.__slice = elem
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
                            strides += [1] * (len(bases) - len(strides))
                        bases.append(start)
                        counts.append(count)
                        strides.append(step)
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
                return c.rowmapping_from_array(bases)
            elif len(bases) == 1:
                return c.rowmapping_from_slice(bases[0], counts[0], strides[0])
            else:
                return c.rowmapping_from_slicelist(bases, counts, strides)

        if isinstance(arg, DataTable):
            if arg.ncols != 1:
                raise ValueError("`rows` argument should be a single-column "
                                 "datatable, got %r" % arg)
            col0type = arg.types[0]
            if col0type != "bool":
                raise TypeError("`rows` datatable should be a boolean column, "
                                "however it has type %s" % col0type)
            if arg.nrows != self.nrows:
                s1rows = plural(arg.nrows, "row")
                s2rows = plural(self.nrows, "row")
                raise ValueError("`rows` datatable has %s, but applied to a "
                                 "datatable with %s" % (s1rows, s2rows))
            return c.rowmapping_from_column(arg._dt)

        if isinstance(arg, types.FunctionType) and not nested:
            dtexpr = DatatableExpr(self)
            res = arg(dtexpr)
            return self._rows_selector(res, True)

        if isinstance(arg, ExprNode):
            return arg

        if nested:
            raise TypeError("Unexpected result produced by the `rows` "
                            "function: %r" % arg)
        else:
            raise TypeError("Unexpected `rows` argument: %r" % arg)


    def _cols_selector(self, arg, nested=False):
        """
        Normalize the column selector ``arg`` and ensure its correctness.

        :param arg: same as parameter ``select`` in self.__call__
        :return: a list where each element is a tuple (column definition,
            column name), and "definition" can be one of:
                * an integer column index, indicating that the column with this
                  index should be copied from the source;
                * an :class:`ExprNode` object describing how the column should
                  be computed.
        """
        if arg is Ellipsis:
            return [(i, self._names[i]) for i in range(self.ncols)]

        if isinstance(arg, (int, str, slice, ExprNode)):
            arg = [arg]

        if isinstance(arg, (list, tuple)):
            ncols = self._ncols
            out = []
            for col in arg:
                if isinstance(col, int):
                    if -ncols <= col < ncols:
                        if col < 0:
                            col += ncols
                        out.append((col, self._names[col]))
                    else:
                        n_columns = plural(ncols, "column")
                        raise ValueError(
                            "datatable has %s; column number %d is invalid"
                            % (n_columns, col))
                elif isinstance(col, str):
                    if col in self._inames:
                        out.append((self._inames[col], col))
                    else:
                        raise ValueError(
                            "Column %r not found in the datatable" % col)
                elif isinstance(col, slice):
                    start = col.start
                    stop = col.stop
                    step = col.step
                    if isinstance(start, str) or isinstance(stop, str):
                        if start is None:
                            col0 = 0
                        elif isinstance(start, str):
                            if start in self._inames:
                                col0 = self._inames[start]
                            else:
                                raise ValueError(
                                    "Column name %r not found in the datatable"
                                    % start)
                        else:
                            raise ValueError(
                                "The slice should start with a column name: %s"
                                % col)
                        if stop is None:
                            col1 = ncols
                        elif isinstance(stop, str):
                            if stop in self._inames:
                                col1 = self._inames[stop] + 1
                            else:
                                raise ValueError("Column name %r not found in "
                                                 "the datatable" % stop)
                        else:
                            raise ValueError("The slice should end with a "
                                             "column name: %r" % col)
                        if step is None or step == 1:
                            step = 1
                        else:
                            raise ValueError("Column name slices cannot use "
                                             "strides: %r" % col)
                        if col1 <= col0:
                            col1 -= 2
                            step = -1
                        for i in range(col0, col1, step):
                            out.append((i, self._names[i]))
                    else:
                        if not all(x is None or isinstance(x, int)
                                   for x in (start, stop, step)):
                            raise ValueError("%r is not integer-valued" % col)
                        col0, count, step = normalize_slice(col, ncols)
                        for i in range(count):
                            j = col0 + i * step
                            out.append((j, self._names[j]))

                elif isinstance(col, ExprNode):
                    out.append((col, str(col)))

            return out

        if isinstance(arg, types.FunctionType) and not nested:
            dtexpr = DatatableExpr(self)
            res = arg(dtexpr)
            return self._cols_selector(res, True)

        raise ValueError("Unknown `select` argument: %r" % arg)


    def __getitem__(self, item):
        """Simpler version than __call__, but allows slice literals."""
        if isinstance(item, tuple):
            if len(item) == 1:
                return self(rows=..., select=item[0])
            if len(item) == 2:
                return self(rows=item[0], select=item[1])
            if len(item) == 3:
                return self(rows=item[0], select=item[1], groupby=item[2])
            raise ValueError("Selector %r is not supported" % item)
        else:
            return self(rows=..., select=item)


    @typed(name=str)
    def colindex(self, name):
        """
        Return index of the column ``name``.

        :param str name: name of the column to find the index for.
        :raises ValueError: if such column does not exist.
        """
        if name in self._inames:
            return self._inames[name]
        else:
            raise ValueError("Column %r does not exist in %r" % (name, self))
