#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import collections
import time
from types import GeneratorType
from typing import Tuple, Dict

# noinspection PyUnresolvedReferences
import _datatable as c
import datatable
from .widget import DataFrameWidget

from datatable.dt_append import rbind as dt_rbind, cbind as dt_cbind
from datatable.utils.misc import plural_form as plural
from datatable.utils.misc import load_module
from datatable.utils.typechecks import (
    TTypeError, TValueError, typed, U, is_type, PandasDataFrame_t)
from datatable.graph import make_datatable

__all__ = ("DataTable", )


class DataTable(object):
    """
    Two-dimensional column-oriented table of data. Each column has its own name
    and type. Types may vary across columns (unlike in a Numpy array) but cannot
    vary within each column (unlike in Pandas DataFrame).

    Internally the data is stored as C primitives, and processed using
    multithreaded native C procedures.

    This is a primary data structure for datatable module.
    """
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

    def _display_in_terminal_(self):  # pragma: no cover
        # This method is called from the display hook set from .utils.terminal
        self.view()

    def _repr_pretty_(self, p, cycle):
        # Called by IPython terminal when displaying the datatable
        self.view()

    def _data_viewer(self, row0, row1, col0, col1):
        view = self._dt.window(row0, row1, col0, col1)
        l = max(2, len(str(row1)))
        return {
            "names": self._names[col0:col1],
            "types": view.types,
            "stypes": view.stypes,
            "columns": view.data,
            "indices": ["%*d" % (l, x) for x in range(row0, row1)],
        }

    def view(self, interactive=True):
        widget = DataFrameWidget(self._nrows, self._ncols, self._data_viewer,
                                 interactive)
        widget.render()


    @typed(colidx=int)
    def _hex(self, colidx):
        if not (-self.ncols <= colidx < self.ncols):
            raise TValueError("Invalid column index %d" % colidx)
        if colidx < 0:
            colidx += self.ncols
        col = self.internal.column(colidx)
        names = ["%02X" % i for i in range(16)] + [""]

        def data_viewer(row0, row1, col0, col1):
            view = c.DataWindow(self._dt, row0, row1, col0, col1, colidx)
            l = len(hex(row1))
            return {
                "names": names[col0:col1],
                "types": view.types,
                "stypes": view.stypes,
                "columns": view.data,
                "indices": ["%0*X" % (l, 16 * r) for r in range(row0, row1)],
            }

        print("Column %d, Name: %r" % (colidx, self._names[colidx]))
        print("Ltype: %s, Stype: %s, Mtype: %s"
              % (col.ltype, col.stype, col.mtype))
        datasize = col.data_size
        print("Bytes: %d" % datasize)
        print("Meta: %s" % col.meta)
        print("Refcnt: %d" % col.refcount)
        widget = DataFrameWidget((datasize + 15) // 16, 17, data_viewer)
        widget.render()


    #---------------------------------------------------------------------------
    # Initialization helpers
    #---------------------------------------------------------------------------

    def _fill_from_source(self, src, colnames):
        if isinstance(src, list):
            if not src or isinstance(src[0], list):
                self._fill_from_list(src, names=colnames)
            else:
                self._fill_from_list([src], names=colnames)
        elif isinstance(src, (tuple, set)):
            self._fill_from_list([list(src)], names=colnames)
        elif isinstance(src, dict):
            self._fill_from_list(list(src.values()), names=tuple(src.keys()))
        elif isinstance(src, c.DataTable):
            self._fill_from_dt(src, names=colnames)
        elif is_type(src, PandasDataFrame_t):
            self._fill_from_pandas(src)
        elif src is None:
            self._fill_from_list([])
        else:
            raise TTypeError("Cannot create DataTable from %r" % src)


    def _fill_from_list(self, src, names=None):
        self._fill_from_dt(c.datatable_from_list(src), names=names)


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


    def _fill_from_pandas(self, pddf):
        columns = []
        for colname in pddf.columns:
            nparray = pddf[colname].values
            if not nparray.dtype.isnative:
                # Array has wrong endianness -- coerce into native byte-order
                nparray = nparray.byteswap().newbyteorder()
                assert nparray.dtype.isnative
            columns.append(nparray)
        dt = c.datatable_from_buffers(columns)
        self._fill_from_dt(dt, names=list(pddf.columns))



    #---------------------------------------------------------------------------
    # Main processor function
    #---------------------------------------------------------------------------

    def __call__(self, rows=None, select=None, verbose=False, timeit=False,
                 sort=None
                 #update=None, groupby=None, join=None, limit=None
                 ):
        """
        Perform computation on a datatable, and return the result.

        :param rows:
            Which rows to operate upon. Could be one of the following:

                - ... or None, representing all rows of the datatable.
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

        :param limit:
            If an integer, then no more than that many rows will be returned by
            the ``select`` clause. This can also be a slice, which effectively
            applies that slice to the resulting datatable.
        """
        time0 = time.time() if timeit else 0
        res = make_datatable(self, rows, select)
        if timeit:
            print("Time taken: %d ms" % (1000 * (time.time() - time0)))
        return res
        # if sort is not None:
        #     idx = self.colindex(sort)
        #     rowindex = self._dt.sort(idx)
        #     columns = c.columns_from_slice(self._dt, 0, self.ncols, 1)
        #     res_dt = c.datatable_assemble(rowindex, columns)
        #     return datatable.DataTable(res_dt, colnames=self.names)


    def __getitem__(self, item):
        """
        Simpler version than __call__, but allows slice literals.

        Example:
            dt[5]        # 6-th column
            dt[5, :]     # 6-th row
            dt[:10, -1]  # first 10 rows of the last column
            dt[::-1]     # all rows of the datatable in reverse order
        etc.
        """
        if isinstance(item, tuple):
            if len(item) == 1:
                return self(select=item[0])
            if len(item) == 2:
                return self(rows=item[0], select=item[1])
            # if len(item) == 3:
            #     return self(rows=item[0], select=item[1], groupby=item[2])
            raise TValueError("Selector %r is not supported" % (item, ))
        else:
            return self(select=item)


    def __delitem__(self, item):
        """
        Delete columns / rows from the datatable.

        Example:
            del dt["colA"]
            del dt[:, ("A", "B")]
            del dt[::2]
            del dt["col5":"col9"]
            del dt[(i for i in range(dt.ncols) if i % 3 <= 1)]
        """
        if isinstance(item, (str, int, slice)):
            pcol = datatable.graph.cols_node.process_column(item, self)
            if isinstance(pcol, int):
                return self._delete_columns([pcol])
            if isinstance(pcol, tuple):
                start, count, step = pcol
                r = range(start, start + count * step, step)
                return self._delete_columns(list(r))

        elif isinstance(item, (GeneratorType, list, set)):
            cols = []
            for it in item:
                pcol = datatable.graph.cols_node.process_column(it, self)
                if isinstance(pcol, int):
                    cols.append(pcol)
                else:
                    raise TTypeError("Invalid column specifier %r" % it)
            return self._delete_columns(cols)

        raise TTypeError("Cannot delete %r from the datatable" % item)


    def _delete_columns(self, cols):
        cols = sorted(list(set(cols)))
        self._dt.delete_columns(cols)
        assert self._ncols - len(cols) == self._dt.ncols
        newnames = self.names[:cols[0]]
        for i in range(1, len(cols)):
            newnames += self.names[(cols[i - 1] + 1):cols[i]]
        newnames += self.names[cols[-1] + 1:]
        self._fill_from_dt(self._dt, names=newnames)



    @typed(name=U(str, int))
    def colindex(self, name):
        """
        Return index of the column ``name``.

        :param name: name of the column to find the index for. This can also
            be an index of a column, in which case the index is checked that
            it doesn't go out-of-bounds, and negative index is converted into
            positive.
        :raises ValueError: if the requested column does not exist.
        """
        if isinstance(name, str):
            if name in self._inames:
                return self._inames[name]
            else:
                raise TValueError("Column `%s` does not exist in %r"
                                  % (name, self))
        else:
            n = self._ncols
            if 0 <= name < n:
                return name
            elif -n <= name < 0:
                return name + n
            else:
                raise TValueError("Column index `%d` is invalid for a "
                                  "datatable with %s"
                                  % (name, plural(n, "column")))


    # Methods defined externally
    append = dt_rbind
    rbind = dt_rbind
    cbind = dt_cbind



    @typed(columns={U(str, int): str})
    def rename(self, columns):
        """
        Rename columns of the datatable.

        :param columns: dictionary of the {old_name: new_name} entries.
        :returns: None
        """
        names = list(self._names)
        for oldname, newname in columns.items():
            idx = self.colindex(oldname)
            names[idx] = newname
        self._fill_from_dt(self._dt, names=names)


    def topandas(self):
        pandas = load_module("pandas")
        numpy = load_module("numpy")
        dtypes = {"i1b": numpy.dtype("bool"),
                  "i1i": numpy.dtype("int8"),
                  "i2i": numpy.dtype("int16"),
                  "i4i": numpy.dtype("int32"),
                  "i8i": numpy.dtype("int64"),
                  "f4r": numpy.dtype("float32"),
                  "f8r": numpy.dtype("float64")}
        nas = {"i1b": -128,
               "i1i": -128,
               "i2i": -32768,
               "i4i": -2147483648,
               "i8i": -9223372036854775808}
        src = collections.OrderedDict()
        for i in range(self._ncols):
            name = self._names[i]
            column = self._dt.column(i)
            dtype = dtypes.get(column.stype)
            if dtype is None:
                # Variable-width types can only be represented in Numpy as
                # dtype='object'. However Numpy cannot ingest a buffer of
                # PyObject types -- getting error
                #   ValueError: cannot create an OBJECT array from memory buffer
                # Thus, the only alternative remaining is to convert such column
                # into plain Python list and pass it to Pandas like that.
                x = self._dt.window(0, self.nrows, i, i + 1).data[0]
            else:
                x = numpy.frombuffer(column, dtype=dtype)
                na = nas.get(column.stype)
                if na is not None:
                    x = numpy.ma.masked_equal(x, na, copy=False)
            src[name] = x

        pd = pandas.DataFrame(src)
        return pd


    def topython(self):
        return self._dt.window(0, self.nrows, 0, self.ncols).data
