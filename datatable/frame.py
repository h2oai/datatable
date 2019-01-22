#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------
import collections
import time
import warnings

from datatable.lib import core
import datatable
from .widget import DataFrameWidget

from datatable.dt_append import _rbind
from datatable.nff import save as dt_save
from datatable.utils.misc import plural_form as plural
from datatable.utils.misc import load_module
from datatable.utils.terminal import term
from datatable.utils.typechecks import (TTypeError, TValueError)
from datatable.csv import write_csv
from datatable.options import options
from datatable.types import stype

__all__ = ("Frame", )



class Frame(core.Frame):
    """
    Two-dimensional column-oriented table of data. Each column has its own name
    and type. Types may vary across columns (unlike in a Numpy array) but cannot
    vary within each column (unlike in Pandas DataFrame).

    Internally the data is stored as C primitives, and processed using
    multithreaded native C++ code.

    This is a primary data structure for datatable module.
    """

    #---------------------------------------------------------------------------
    # Display
    #---------------------------------------------------------------------------

    def __repr__(self):
        srows = plural(self.nrows, "row")
        scols = plural(self.ncols, "col")
        return "<Frame [%s x %s]>" % (srows, scols)

    def _display_in_terminal_(self):  # pragma: no cover
        # This method is called from the display hook set from .utils.terminal
        self.view()

    def _repr_pretty_(self, p, cycle):
        # Called by IPython terminal when displaying the datatable
        if not term.jupyter:
            self.view()

    def _data_viewer(self, row0, row1, col0, col1):
        view = self[row0:row1, col0:col1]
        length = max(2, len(str(row1)))
        nk = len(self.key)
        return {
            "names": self.names[:nk] + self.names[col0 + nk:col1 + nk],
            "types": view.ltypes,
            "stypes": view.stypes,
            "columns": view.to_list(),
            "rownumbers": ["%*d" % (length, x) for x in range(row0, row1)],
        }

    def view(self, interactive=True):
        widget = DataFrameWidget(self.nrows, self.ncols, len(self.key),
                                 self._data_viewer, interactive)
        widget.render()


    #---------------------------------------------------------------------------
    # Main processor function
    #---------------------------------------------------------------------------

    def __call__(self, rows=None, select=None, verbose=False, timeit=False,
                 groupby=None, join=None, sort=None, engine=None
                 ):
        """DEPRECATED, use DT[i, j, ...] instead."""
        warnings.warn(
            "`DT(rows, select, ...)` is deprecated and will be removed in "
            "version 0.9.0. Please use `DT[i, j, ...]` instead",
            category=FutureWarning)
        time0 = time.time() if timeit else 0
        function = type(lambda: None)
        if isinstance(rows, function):
            rows = rows(datatable.f)
        if isinstance(select, function):
            select = select(datatable.f)

        res = self[rows, select, datatable.by(groupby), datatable.sort(sort)]
        if timeit:
            print("Time taken: %d ms" % (1000 * (time.time() - time0)))
        return res


    # Methods defined externally
    append = _rbind
    rbind = _rbind
    to_csv = write_csv
    save = dt_save


    def sort(self, *cols):
        """
        Sort datatable by the specified column(s).

        Parameters
        ----------
        cols: List[str | int]
            Names or indices of the columns to sort by. If no columns are given,
            the Frame will be sorted on all columns.

        Returns
        -------
        New datatable sorted by the provided column(s). The target datatable
        remains unmodified.
        """
        if not cols:
            indexes = list(range(self.ncols))
        elif len(cols) == 1 and isinstance(cols[0], list):
            indexes = [self.colindex(col) for col in cols[0]]
        else:
            indexes = [self.colindex(col) for col in cols]
        ri = self._dt.sort(*indexes)[0]
        cs = core.columns_from_slice(self._dt, ri, 0, self.ncols, 1)
        return cs.to_frame(self.names)


    #---------------------------------------------------------------------------
    # Stats
    #---------------------------------------------------------------------------

    def min(self):
        """
        Get the minimum value of each column.

        Returns
        -------
        A new datatable of shape (1, ncols) containing the computed minimum
        values for each column (or NA if not applicable).
        """
        return self._dt.get_min()

    def max(self):
        """
        Get the maximum value of each column.

        Returns
        -------
        A new datatable of shape (1, ncols) containing the computed maximum
        values for each column (or NA if not applicable).
        """
        return self._dt.get_max()

    def mode(self):
        """
        Get the modal value of each column.

        Returns
        -------
        A new datatable of shape (1, ncols) containing the computed count of
        most frequent values for each column.
        """
        return self._dt.get_mode()

    def sum(self):
        """
        Get the sum of each column.

        Returns
        -------
        A new datatable of shape (1, ncols) containing the computed sums
        for each column (or NA if not applicable).
        """
        return self._dt.get_sum()

    def mean(self):
        """
        Get the mean of each column.

        Returns
        -------
        A new datatable of shape (1, ncols) containing the computed mean
        values for each column (or NA if not applicable).
        """
        return self._dt.get_mean()

    def sd(self):
        """
        Get the standard deviation of each column.

        Returns
        -------
        A new datatable of shape (1, ncols) containing the computed standard
        deviation values for each column (or NA if not applicable).
        """
        return self._dt.get_sd()

    def countna(self):
        """
        Get the number of NA values in each column.

        Returns
        -------
        A new datatable of shape (1, ncols) containing the counted number of NA
        values in each column.
        """
        return self._dt.get_countna()

    def nunique(self):
        """
        Get the number of unique values in each column.

        Returns
        -------
        A new datatable of shape (1, ncols) containing the counted number of
        unique values in each column.
        """
        return self._dt.get_nunique()

    def nmodal(self):
        """
        Get the number of modal values in each column.

        Returns
        -------
        A new datatable of shape (1, ncols) containing the counted number of
        most frequent values in each column.
        """
        return self._dt.get_nmodal()

    def min1(self):
        return self._dt.min1()

    def max1(self):
        return self._dt.max1()

    def mode1(self):
        return self._dt.mode1()

    def sum1(self):
        return self._dt.sum1()

    def mean1(self):
        return self._dt.mean1()

    def sd1(self):
        return self._dt.sd1()

    def countna1(self):
        return self._dt.countna1()

    def nunique1(self):
        return self._dt.nunique1()

    def nmodal1(self):
        return self._dt.nmodal1()


    #---------------------------------------------------------------------------
    # Converters
    #---------------------------------------------------------------------------

    def to_pandas(self):
        """
        Convert Frame to a pandas DataFrame, or raise an error if `pandas`
        module is not installed.
        """
        pandas = load_module("pandas")
        numpy = load_module("numpy")
        if not hasattr(pandas, "DataFrame"):  # pragma: no cover
            raise ImportError("Unsupported pandas version: `%s`"
                              % (getattr(pandas, "__version__", "???"), ))
        nas = {stype.bool8: -128,
               stype.int8: -128,
               stype.int16: -32768,
               stype.int32: -2147483648,
               stype.int64: -9223372036854775808}
        self.materialize()
        srcdt = self._dt
        srccols = collections.OrderedDict()
        for i in range(self.ncols):
            name = self.names[i]
            column = srcdt.column(i)
            dtype = self.stypes[i].dtype
            if dtype == numpy.bool:
                dtype = numpy.int8
            if dtype == numpy.dtype("object"):
                # Variable-width types can only be represented in Numpy as
                # dtype='object'. However Numpy cannot ingest a buffer of
                # PyObject types -- getting error
                #   ValueError: cannot create an OBJECT array from memory buffer
                # Thus, the only alternative remaining is to convert such column
                # into plain Python list and pass it to Pandas like that.
                x = self[:, i].to_list()[0]
            else:
                x = numpy.frombuffer(column, dtype=dtype)
                na = nas.get(self.stypes[i])
                if na is not None:
                    x = numpy.ma.masked_equal(x, na, copy=False)
            srccols[name] = x

        pd = pandas.DataFrame(srccols)
        return pd


    def to_numpy(self, stype=None):
        """
        Convert Frame into a numpy array, optionally forcing it into a
        specific stype/dtype.

        Parameters
        ----------
        stype: datatable.stype, numpy.dtype or str
            Cast datatable into this dtype before converting it into a numpy
            array.
        """
        numpy = load_module("numpy")
        if not hasattr(numpy, "array"):  # pragma: no cover
            raise ImportError("Unsupported numpy version: `%s`"
                              % (getattr(numpy, "__version__", "???"), ))
        st = 0
        if stype:
            st = datatable.stype(stype).value
        self.internal.use_stype_for_buffers(st)
        res = numpy.array(self.internal)
        self.internal.use_stype_for_buffers(0)
        return res


    def topython(self):  # DEPRECATED
        warnings.warn(
            "Method `Frame.topython()` is deprecated (will be removed in "
            "0.9.0), please use `Frame.to_list()` instead",
            category=FutureWarning)
        return self.to_list()

    def topandas(self):
        warnings.warn(
            "Method `Frame.topandas()` is deprecated (will be removed in "
            "0.9.0), please use `Frame.to_pandas()` instead",
            category=FutureWarning)
        return self.to_pandas()

    def tonumpy(self, stype=None):
        warnings.warn(
            "Method `Frame.tonumpy()` is deprecated (will be removed in "
            "0.9.0), please use `Frame.to_numpy()` instead",
            category=FutureWarning)
        return self.to_numpy(stype)


    def scalar(self):
        """
        For a 1x1 Frame return its content as a python object.

        Raises an error if the shape of the Frame is not 1x1.
        """
        return self._dt.to_scalar()


    def materialize(self):
        if self._dt.isview:
            self._dt.materialize()


    def __sizeof__(self):
        """
        Return the size of this Frame in memory.

        The function attempts to compute the total memory size of the Frame
        as precisely as possible. In particular, it takes into account not only
        the size of data in columns, but also sizes of all auxiliary internal
        structures.

        Special cases: if Frame is a view (say, `d2 = d[:1000, :]`), then
        the reported size will not contain the size of the data, because that
        data "belongs" to the original datatable and is not copied. However if
        a Frame selects only a subset of columns (say, `d3 = d[:, :5]`),
        then a view is not created and instead the columns are copied by
        reference. Frame `d3` will report the "full" size of its columns,
        even though they do not occupy any extra memory compared to `d`. This
        behavior may be changed in the future.

        This function is not intended for manual use. Instead, in order to get
        the size of a datatable `d`, call `sys.getsizeof(d)`.
        """
        return self._dt.alloc_size




#-------------------------------------------------------------------------------
# Global settings
#-------------------------------------------------------------------------------

core.register_function(4, TTypeError)
core.register_function(5, TValueError)
core.register_function(7, Frame)
core.install_buffer_hooks(Frame())


options.register_option(
    "core_logger", xtype=callable, default=None,
    doc="If you set this option to a Logger object, then every call to any "
        "core function will be recorded via this object.")

options.register_option(
    "sort.insert_method_threshold", xtype=int, default=64,
    doc="Largest n at which sorting will be performed using insert sort "
        "method. This setting also governs the recursive parts of the "
        "radix sort algorithm, when we need to sort smaller sub-parts of "
        "the input.")

options.register_option(
    "sort.thread_multiplier", xtype=int, default=2)

options.register_option(
    "sort.max_chunk_length", xtype=int, default=1024)

options.register_option(
    "sort.max_radix_bits", xtype=int, default=12)

options.register_option(
    "sort.over_radix_bits", xtype=int, default=8)

options.register_option(
    "sort.nthreads", xtype=int, default=4)

options.register_option(
    "frame.names_auto_index", xtype=int, default=0,
    doc="When Frame needs to auto-name columns, they will be assigned "
        "names C0, C1, C2, ... by default. This option allows you to "
        "control the starting index in this sequence. For example, setting "
        "options.frame.names_auto_index=1 will cause the columns to be "
        "named C1, C2, C3, ...")

options.register_option(
    "frame.names_auto_prefix", xtype=str, default="C",
    doc="When Frame needs to auto-name columns, they will be assigned "
        "names C0, C1, C2, ... by default. This option allows you to "
        "control the prefix used in this sequence. For example, setting "
        "options.frame.names_auto_prefix='Z' will cause the columns to be "
        "named Z0, Z1, Z2, ...")
