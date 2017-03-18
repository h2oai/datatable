#!/usr/bin/env python
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
from __future__ import division, print_function, unicode_literals

import types
from typing import Dict, Tuple

# noinspection PyUnresolvedReferences
import _datatable as c
from .widget import DataFrameWidget

from datatable.lazy import _LazyDataTable
from datatable.utils.misc import normalize_slice, normalize_range, plural_form
from datatable.utils.typechecks import is_type, TypeError, ValueError

__all__ = ("DataTable", )


class DataTable(object):
    _id_counter_ = 0
    _dt_types = ["auto", "real", "int", "str", "bool", "obj"]

    def __init__(self, src=None):
        DataTable._id_counter_ += 1
        self._id = DataTable._id_counter_  # type: int
        self._ncols = 0      # type: int
        self._nrows = 0      # type: int
        self._types = None   # type: Tuple[str]
        self._names = None   # type: Tuple[str]
        # Mapping of column names to their indices
        self._inames = None  # type: Dict[str, int]
        self._dt = None      # type: c.DataTable
        self._fill_from_source(src)


    #---------------------------------------------------------------------------
    # Basic properties
    #---------------------------------------------------------------------------

    @property
    def ncols(self):
        """Number of columns in the datatable."""
        return self._ncols

    @property
    def nrows(self):
        """Number of rows in the datatable."""
        return self._nrows

    @property
    def shape(self):
        """Tuple (number of rows, number of columns) in the datatable."""
        return (self._nrows, self._ncols)

    @property
    def names(self):
        """Tuple of column names."""
        return self._names


    #---------------------------------------------------------------------------
    # Display
    #---------------------------------------------------------------------------

    def __repr__(self):
        srows = plural_form(self._nrows, "row")
        scols = plural_form(self._ncols, "col")
        return "<DataTable #%d (%s x %s)>" % (self._id, srows, scols)

    def _display_in_terminal_(self):
        self.view()

    def _data_viewer(self, row0, row1, col0, col1):
        view = self._dt.window(row0, row1, col0, col1)
        return {
            "names": self._names[col0:col1],
            "types": [DataTable._dt_types[t] for t in view.types],
            "columns": view.data,
        }

    def view(self):
        widget = DataFrameWidget(self._nrows, self._ncols, self._data_viewer)
        widget.render()


    #---------------------------------------------------------------------------
    # Initialization helpers
    #---------------------------------------------------------------------------

    def _fill_from_source(self, src):
        if isinstance(src, list):
            self._fill_from_list(src)
        elif isinstance(src, (tuple, set)):
            self._fill_from_list(list(src))
        elif isinstance(src, dict):
            self._fill_from_list(list(src.values()), names=tuple(src.keys()))
        else:
            self._dt = c.DataTable()


    def _fill_from_list(self, src, names=None):
        self._dt = c.datatable_from_list(src)
        self._ncols = self._dt.ncols
        self._nrows = self._dt.nrows
        if not names:
            names = tuple("C%d" % (i + 1) for i in range(self._ncols))
        if not isinstance(names, tuple):
            names = tuple(names)
        assert len(names) == self._ncols
        self._names = names
        self._inames = {n: i for i, n in enumerate(names)}


    def __call__(self, rows=None, select=None, update=None, groupby=None,
                 join=None, sort=None, limit=None):
        """
        Perform computation on a datatable, and return the result.

        :param rows:
            Which rows to operate upon. Could be one of the following:

                - ... (ellipsis), representing all rows of the datatable
                - an integer, representing a single row
                - a list of integers, representing several rows
                - a slice / range, representing some range of rows
                - a list of slices / ranges
                - a ``DataTable`` with a single boolean column and having same
                  number of rows as the current datatable, this will select
                  only those rows in the current datatable where the ``rows``
                  datatable has truthful value
                - a function that takes a single parameter -- the current
                  datatable -- and returns any of the selectors mentioned
                  above. Within this function, the frame behaves lazily.

        :param select:
            When this parameter is specified, a new datatable will be computed
            and returned from this call. This parameter cannot be combined with
            ``update``. Possible values:

                - ..., to select all columns in the current frame
                - dt.GROUP symbol, which can be used only when a ``groupby`` is
                  requested. This symbol selects all columns that were grouped
                  upon.
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
        rows_selector = self._rows_selector(rows)
        if rows_selector.__class__.__name__ == "RowIndex":
            res = DataTable()
            res._dt = self._dt.omni(rows_selector)
            res._ncols = res._dt.ncols
            res._nrows = res._dt.nrows
            res._names = self._names
            res._inames = self._inames
            res._types = self._types
            return res

        print(rows_selector)


    def _rows_selector(self, arg, nested=False):
        """
        Normalize the selector given by ``arg`` and ensure its correctness.
        """
        nrows = self._nrows
        if arg is Ellipsis:
            return c.select_row_slice(self._dt, 0, nrows, 1)

        if isinstance(arg, (int, slice, range)):
            arg = [arg]

        if isinstance(arg, types.GeneratorType):
            # If an iterator is provided, materialize it first. Otherwise there
            # is no way of testing whether the produced indices are valid
            arg = list(arg)

        if isinstance(arg, list):
            bases = []
            counts = []
            strides = []
            for i, elem in enumerate(arg):
                if isinstance(elem, int):
                    if elem < -nrows or elem >= nrows:
                        s = "datatable contains %s; row index %d is invalid" \
                            % (plural_form(nrows, "row"), elem)
                        raise ValueError(s)
                    if elem < 0:
                        elem += nrows
                    bases.append(elem)
                elif isinstance(elem, (range, slice)):
                    # If range/slice extends beyond the bounds of the datatable,
                    # we coerce it into the correct range without raising an
                    # error. This mimics the default Python's behavior, eg
                    #     "test"[-100:100] == "test"
                    if elem.step == 0:
                        raise ValueError("In %r step must not be 0" % elem)
                    if not (is_type(elem.start, int, None) and
                            is_type(elem.stop, int, None) and
                            is_type(elem.step, int, None)):
                        raise ValueError("%r is not integer-valued" % elem)
                    if isinstance(elem, range):
                        res = normalize_range(elem, nrows)
                        if res is None:
                            s = "Invalid %r for a datatable with %s" \
                                % (elem, plural_form(nrows, "row"))
                            raise ValueError(s)
                        start, count, step = res
                    else:
                        start, count, step = normalize_slice(elem, nrows)
                    if count == 1:
                        bases.append(start)
                    elif count > 1:
                        if len(counts) < len(bases):
                            counts += [1] * (len(bases) - len(counts))
                            strides += [1] * (len(bases) - len(strides))
                        bases.append(start)
                        counts.append(count)
                        strides.append(step)
                    # if count == 0 then don't do anything
                else:
                    raise TypeError("Invalid row specifier %r at element "
                                    "%d of the `rows` list" % (elem, i))
            if counts:
                if len(bases) == 1:
                    return c.select_row_slice(self._dt,
                                             bases[0], counts[0], strides[0])
                else:
                    return (bases, counts, strides)
            else:
                return c.select_row_indices(self._dt, bases)

        if isinstance(arg, DataTable):
            if arg.ncols != 1:
                raise ValueError("`rows` argument should be a single-column "
                                 "datatable, got %r" % arg)
            if arg.nrows != self.nrows:
                raise ValueError("`rows` datatable has %s, which is incompatible "
                                 "with current datatable having %s"
                                 % (plural_form(arg.nrows, "row"),
                                    plural_form(self.nrows, "row")))
            col0type = arg._types[0]
            if col0type != "bool":
                raise TypeError("`rows` datatable should contain a boolean "
                                "column, however it has type %s" % col0type)
            return arg

        if isinstance(arg, types.FunctionType) and not nested:
            ss = arg(_LazyDataTable(src=self))
            return self._rows_selector(ss, True)

        if isinstance(arg, _LazyDataTable):
            return arg

        if nested:
            raise TypeError("Unexpected result produced by the `rows` "
                            "function: %r" % arg)
        else:
            raise TypeError("Unexpected `rows` argument: %r" % arg)


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
