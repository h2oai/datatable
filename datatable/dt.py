#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import collections
import time
import sys
from types import GeneratorType
from typing import Tuple, Dict

# noinspection PyUnresolvedReferences
import datatable.lib._datatable as c
import datatable
from .widget import DataFrameWidget

from datatable.dt_append import rbind as dt_rbind, cbind as dt_cbind
from datatable.utils.misc import plural_form as plural
from datatable.utils.misc import load_module
from datatable.utils.typechecks import (
    TTypeError, TValueError, typed, U, is_type, DataTable_t,
    PandasDataFrame_t, PandasSeries_t, NumpyArray_t, NumpyMaskedArray_t)
from datatable.graph import make_datatable
from datatable.csv import write_csv
from datatable.expr.consts import CStats

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

    __slots__ = ("_id", "_ncols", "_nrows", "_ltypes", "_stypes", "_names",
                 "_inames", "_dt")

    def __init__(self, src=None, colnames=None):
        DataTable._id_counter_ += 1
        self._id = DataTable._id_counter_  # type: int
        self._ncols = 0      # type: int
        self._nrows = 0      # type: int
        self._ltypes = None  # type: Tuple[ltype]
        self._stypes = None  # type: Tuple[stype]
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
        if self._ltypes is None:
            self._ltypes = self._dt.ltypes
        return self._ltypes

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


    #---------------------------------------------------------------------------
    # Initialization helpers
    #---------------------------------------------------------------------------

    def _fill_from_source(self, src, colnames):
        if isinstance(src, list):
            if src and isinstance(src[0], list):
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
            self._fill_from_pandas(src, colnames)
        elif is_type(src, PandasSeries_t):
            self._fill_from_pandas(src, colnames)
        elif is_type(src, NumpyArray_t):
            self._fill_from_numpy(src, names=colnames)
        elif src is None:
            self._fill_from_list([])
        elif is_type(src, DataTable_t):
            if colnames is None:
                colnames = src.names
            self._fill_from_dt(src.internal, names=colnames)
        else:
            raise TTypeError("Cannot create DataTable from %r" % src)


    def _fill_from_list(self, src, names=None):
        self._fill_from_dt(c.datatable_from_list(src), names=names)


    def _fill_from_dt(self, _dt, names=None):
        self._dt = _dt
        self._ncols = _dt.ncols
        self._nrows = _dt.nrows
        if not names:
            names = tuple("C%d" % (i + 1) for i in range(self._ncols))
        if not isinstance(names, tuple):
            names = tuple(names)
        assert len(names) == self._ncols
        self._names = names
        self._inames = {n: i for i, n in enumerate(names)}


    def _fill_from_pandas(self, pddf, colnames=None):
        if is_type(pddf, PandasDataFrame_t):
            if colnames is None:
                colnames = [str(c) for c in pddf.columns]
            colarrays = [pddf[c].values for c in pddf.columns]
        elif is_type(pddf, PandasSeries_t):
            colnames = None
            colarrays = [pddf.values]
        else:
            raise TTypeError("Unexpected type of parameter %r" % pddf)
        for i in range(len(colarrays)):
            if not colarrays[i].dtype.isnative:
                # Array has wrong endianness -- coerce into native byte-order
                colarrays[i] = colarrays[i].byteswap().newbyteorder()
                assert colarrays[i].dtype.isnative
        dt = c.datatable_from_buffers(colarrays)
        self._fill_from_dt(dt, names=colnames)


    def _fill_from_numpy(self, arr, names):
        dim = len(arr.shape)
        if dim > 2:
            raise TValueError("Cannot create DataTable from a %d-D numpy "
                              "array %r" % (dim, arr))
        if dim == 0:
            arr = arr.reshape((1, 1))
        if dim == 1:
            arr = arr.reshape((len(arr), 1))
        if not arr.dtype.isnative:
            arr = arr.byteswap().newbyteorder()

        ncols = arr.shape[1]
        if is_type(arr, NumpyMaskedArray_t):
            dt = c.datatable_from_buffers([arr.data[:, i]
                                           for i in range(ncols)])
            mask = c.datatable_from_buffers([arr.mask[:, i]
                                             for i in range(ncols)])
            dt.apply_na_mask(mask)
        else:
            dt = c.datatable_from_buffers([arr[:, i] for i in range(ncols)])

        if names is None:
            names = ["C%d" % i for i in range(1, ncols + 1)]
        self._fill_from_dt(dt, names=names)



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
                  the datatable, these columns will be replaced.
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
        res = make_datatable(self, rows, select, sort)
        if timeit:
            print("Time taken: %d ms" % (1000 * (time.time() - time0)))
        return res


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
    to_csv = write_csv


    @typed(by=U(str, int))
    def sort(self, by):
        """
        Sort datatable by the specified column.

        Parameters
        ----------
        by: str or int
            Name or index of the column to sort by.

        Returns
        -------
        New datatable sorted by the provided column. The target datatable
        remains unmodified.
        """
        idx = self.colindex(by)
        ri = self._dt.sort(idx)
        cs = c.columns_from_slice(self._dt, 0, self._ncols, 1)
        dt = c.datatable_assemble(ri, cs)
        return DataTable(dt, colnames=self.names)


    def min(self):
        """
        Get the minimum value of each column.

        Returns
        -------
        A new datatable of shape (1, ncols) containing the computed minimum
        values for each column (or NA if not applicable).
        """
        return DataTable(self._dt.get_min(),
                         colnames=self.names)

    def max(self):
        """
        Get the maximum value of each column.

        Returns
        -------
        A new datatable of shape (1, ncols) containing the computed maximum
        values for each column (or NA if not applicable).
        """
        return DataTable(self._dt.get_max(),
                         colnames=self.names)

    def sum(self):
        """
        Get the sum of each column.

        Returns
        -------
        A new datatable of shape (1, ncols) containing the computed sums
        for each column (or NA if not applicable).
        """
        return DataTable(self._dt.get_sum(),
                         colnames=self.names)

    def mean(self):
        """
        Get the mean of each column.

        Returns
        -------
        A new datatable of shape (1, ncols) containing the computed mean
        values for each column (or NA if not applicable).
        """
        return DataTable(self._dt.get_mean(),
                         colnames=self.names)

    def sd(self):
        """
        Get the standard deviation of each column.

        Returns
        -------
        A new datatable of shape (1, ncols) containing the computed standard
        deviation values for each column (or NA if not applicable).
        """
        return DataTable(self._dt.get_sd(),
                         colnames=self.names)

    def countna(self):
        """
        Get the number of NA values in each column.

        Returns
        -------
        A new datatable of shape (1, ncols) containing the counted number of NA
        values in each column.
        """
        return DataTable(self._dt.get_countna(),
                         colnames=self.names)



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



    #---------------------------------------------------------------------------
    # Converters
    #---------------------------------------------------------------------------

    def topandas(self):
        """
        Convert DataTable to a pandas DataFrame, or raise an error if `pandas`
        module is not installed.
        """
        pandas = load_module("pandas")
        numpy = load_module("numpy")
        nas = {"i1b": -128,
               "i1i": -128,
               "i2i": -32768,
               "i4i": -2147483648,
               "i8i": -9223372036854775808}
        srcdt = self._dt
        if srcdt.isview:
            srcdt = srcdt.materialize()
        srccols = collections.OrderedDict()
        for i in range(self._ncols):
            name = self._names[i]
            column = srcdt.column(i)
            dtype = datatable.stype(column.stype).dtype
            if dtype == numpy.dtype("object"):
                # Variable-width types can only be represented in Numpy as
                # dtype='object'. However Numpy cannot ingest a buffer of
                # PyObject types -- getting error
                #   ValueError: cannot create an OBJECT array from memory buffer
                # Thus, the only alternative remaining is to convert such column
                # into plain Python list and pass it to Pandas like that.
                x = srcdt.window(0, self.nrows, i, i + 1).data[0]
            else:
                x = numpy.frombuffer(column, dtype=dtype)
                na = nas.get(column.stype)
                if na is not None:
                    x = numpy.ma.masked_equal(x, na, copy=False)
            srccols[name] = x

        pd = pandas.DataFrame(srccols)
        return pd


    def tonumpy(self, stype=None):
        """
        Convert DataTable into a numpy array, optionally forcing it into a
        specific stype/dtype.

        Parameters
        ----------
        stype: datatable.stype, numpy.dtype or str
            Cast datatable into this dtype before converting it into a numpy
            array.
        """
        numpy = load_module("numpy")
        st = 0
        if stype:
            st = datatable.stype(stype).value
        self.internal.use_stype_for_buffers(st)
        res = numpy.array(self.internal)
        self.internal.use_stype_for_buffers(0)
        return res


    def topython(self):
        return self._dt.window(0, self.nrows, 0, self.ncols).data


    def __sizeof__(self):
        """
        Return the size of this DataTable in memory.

        The function attempts to compute the total memory size of the DataTable
        as precisely as possible. In particular, it takes into account not only
        the size of data in columns, but also sizes of all auxiliary internal
        structures.

        Special cases: if DataTable is a view (say, `d2 = d[:1000, :]`), then
        the reported size will not contain the size of the data, because that
        data "belongs" to the original datatable and is not copied. However if
        a DataTable selects only a subset of columns (say, `d3 = d[:, :5]`),
        then a view is not created and instead the columns are copied by
        reference. DataTable `d3` will report the "full" size of its columns,
        even though they do not occupy any extra memory compared to `d`. This
        behavior may be changed in the future.

        This function is not intended for manual use. Instead, in order to get
        the size of a datatable `d`, call `sys.getsizeof(d)`.
        """
        # This is somewhat tricky to get right, so here are general
        # considerations:
        #   * We want to add sizes of all internal fields, recursively if they
        #     are containers.
        #   * Integer fields are ignored, because they are usually heavily
        #     shared with other objects in the system. Of course we could have
        #     used `sys.getrefcount()` to check whether any particular field
        #     is shared, but that creates an undesirable effect that the size
        #     of the DataTable apparently depends on external variables...
        #   * The contents of `types` and `stypes` are not counted, because
        #     these strings are shared globally within datatable module.
        #   * Column names are added to the total sum.
        #   * The keys in `self._inames` are skipped, since they are the same
        #     objects as elements of `self._names`, the values are skipped
        #     because they are integers.
        #   * The sys.getsizeof() automatically adds 24 to the final answer,
        #     which is the size of the DataTable object itself.
        size = 0
        for s in self.__class__.__slots__:
            attr = getattr(self, s)
            if not isinstance(attr, int):
                size += sys.getsizeof(attr)
        for n in self._names:
            size += sys.getsizeof(n)
        size += self._dt.alloc_size
        return size


def column_hexview(col, dt, colidx):
    hexdigits = ["%02X" % i for i in range(16)] + [""]

    def data_viewer(row0, row1, col0, col1):
        view = c.DataWindow(dt, row0, row1, col0, col1, colidx)
        l = len(hex(row1))
        return {
            "names": hexdigits[col0:col1],
            "types": view.types,
            "stypes": view.stypes,
            "columns": view.data,
            "indices": ["%0*X" % (l, 16 * r) for r in range(row0, row1)],
        }

    print("Column %d" % colidx)
    print("Ltype: %s, Stype: %s, Mtype: %s"
          % (col.ltype, col.stype, col.mtype))
    datasize = col.data_size
    print("Bytes: %d" % datasize)
    print("Meta: %s" % col.meta)
    print("Refcnt: %d" % col.refcount)
    widget = DataFrameWidget((datasize + 15) // 16, 17, data_viewer)
    widget.render()


c.register_function(1, column_hexview)
c.install_buffer_hooks(DataTable())
