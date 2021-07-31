#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2020 H2O.ai
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
import copy
import datetime
import pytest
import random
import re
import sys
import warnings
from datatable import dt, f, join
from datatable.internal import frame_integrity_check
from tests_random.utils import (
    assert_equals,
    random_column,
    random_names,
    random_type,
    repr_data,
    repr_row,
    repr_types,
    traced)


class MetaFrame:
    COUNTER = 1

    def __init__(self):
        self.df = None
        self.data = None
        self.names = None
        self.types = None
        self.nkeys = 0
        self.np_data = []
        self.np_data_deepcopy = []
        self.df_shallow_copy = None
        self.df_deep_copy = None
        self._name = "DT" + str(MetaFrame.COUNTER)
        MetaFrame.COUNTER += 1


    @staticmethod
    def random(ncols=None, nrows=None, types=None, names=None,
               missing_fraction=None):
        if ncols is None:
            if types:
                ncols = len(types)
            elif names:
                ncols = len(names)
            else:
                ncols = int(random.expovariate(0.2)) + 1
        if nrows is None:
            nrows = int(random.expovariate(0.01)) + 1
        if not types:
            types = [random_type() for _ in range(ncols)]
        if not names:
            names = random_names(ncols)
        if missing_fraction is None:
            missing_fraction = random.random()**10
            if missing_fraction < 0.05 or nrows == 1:
                missing_fraction = 0.0
        assert isinstance(ncols, int)
        assert isinstance(nrows, int)
        assert isinstance(types, list) and len(types) == ncols
        assert isinstance(names, list) and len(names) == ncols
        assert isinstance(missing_fraction, float)
        tt = {bool: 0, int: 0, float: 0, str: 0, None: 0}
        for t in types:
            tt[t] += 1
        assert len(tt) == 5
        print("# Making a frame with nrows=%d, ncols=%d" % (nrows, ncols))
        print("#   types: bool=%d, int=%d, float=%d, str=%d"
              % (tt[bool], tt[int], tt[float], tt[str]))
        print("#   missing values: %.3f" % missing_fraction)
        data = [random_column(nrows, types[i], missing_fraction)[0]
                for i in range(ncols)]
        frame = MetaFrame()
        frame.data = data
        frame.names = names
        frame.types = types
        frame.nkeys = 0
        frame.df = dt.Frame(data, names=names, stypes=types)
        print(f"{frame.name} = dt.Frame(")
        print(f"    {repr_data(data, 4)},")
        print(f"    names={names},")
        print(f"    types={repr_types(types)}")
        print(f")")
        return frame


    @staticmethod
    def load_from_jay(filename):
        DT = dt.fread(filename)
        frame = MetaFrame()
        frame.df = DT
        frame.data = DT.to_list()
        frame.names = list(DT.names)
        frame.types = [_ltype_to_pytype[lt] for lt in DT.ltypes]
        frame.nkeys = len(DT.key)
        print(f"{frame.name} = <loaded from {filename}>")
        return frame



    #---------------------------------------------------------------------------
    # Properties
    #---------------------------------------------------------------------------

    def __repr__(self):
        return self._name

    @property
    def name(self):
        return self._name

    @property
    def nrows(self):
        nrows = len(self.data[0]) if self.data else 0
        assert self.df.nrows == nrows
        return nrows

    @property
    def ncols(self):
        ncols = len(self.data)
        assert self.df.ncols == ncols
        return ncols


    #---------------------------------------------------------------------------
    # Validity checks
    #---------------------------------------------------------------------------

    def check(self):
        frame_integrity_check(self.df)
        if self.df_shallow_copy:
            frame_integrity_check(self.df_shallow_copy)
            frame_integrity_check(self.df_deep_copy)
            assert_equals(self.df_shallow_copy, self.df_deep_copy)
        self.check_shape()
        self.check_types()
        self.check_keys()
        if self.df.names != tuple(self.names):
            print("ERROR: df.names=%r ≠\n"
                  "       py.names=%r" % (self.df.names, tuple(self.names)))
            sys.exit(1)
        self.check_data()
        self.check_np_data()

    def check_shape(self):
        df_nrows = self.df.nrows
        py_nrows = len(self.data[0]) if self.data else 0
        if df_nrows != py_nrows:
            print("ERROR: df.nrows=%r != py.nrows=%r" % (df_nrows, py_nrows))
            sys.exit(1)
        df_ncols = self.df.ncols
        py_ncols = len(self.data)
        if df_ncols != py_ncols:
            print("ERROR: df.ncols=%r != py.ncols=%r" % (df_ncols, py_ncols))
            sys.exit(1)

    def check_keys(self):
        dt_nkeys = len(self.df.key)
        py_nkeys = self.nkeys
        if dt_nkeys != py_nkeys:
            print("ERROR: number of key columns doesn't match")
            print("  dt keys: %r" % dt_nkeys)
            print("  py keys: %r" % py_nkeys)
            sys.exit(1)

    def check_types(self):
        df_ltypes = self.df.ltypes
        py_ltypes = tuple(dt.ltype(x) for x in self.types)
        if df_ltypes != py_ltypes:
            print("ERROR: types mismatch")
            print("  dt types: %r" % (df_ltypes, ))
            print("  py types: %r" % (py_ltypes, ))
            sys.exit(1)

    def check_data(self):
        df_data = self.df.to_list()
        py_data = self.data
        if df_data != py_data:
            assert len(df_data) == len(py_data), "Shape check failed..."
            for i, dfcol in enumerate(df_data):
                pycol = py_data[i]
                if dfcol == pycol:
                    continue
                print("ERROR: data mismatch in column %d (%r)"
                      % (i, self.df.names[i]))
                for j, dfval in enumerate(dfcol):
                    pyval = pycol[j]
                    if dfval == pyval:
                        continue
                    print("  first difference: dt[%d]=%r != py[%d]=%r"
                          % (j, dfval, j, pyval))
                    print("  dt data: %s" % repr_row(dfcol, j))
                    print("  py data: %s" % repr_row(pycol, j))
                    sys.exit(1)
            assert False, "Data check failed..."


    def check_np_data(self):
        import numpy as np
        np_data1 = self.np_data
        np_data2 = self.np_data_deepcopy

        assert len(np_data1) == len(np_data2), "Numpy data shape check failed..."
        for i, np_col1 in enumerate(np_data1):
            np_col2 = np_data2[i]
            assert len(np_col1) == len(np_col2), "Numpy column shape check failed..."
            if np.array_equal(np_col1, np_col2):
                continue
            print("ERROR: numpy data mismatch at column %d" % i)
            for j, np_val1 in enumerate(np_col1):
                np_val2 = np_col2[j]
                if np_val1 == np_val2:
                    continue
                print("  first difference: np_col1[%d]=%r != np_col2[%d]=%r"
                      % (j, np_val1, j, np_val2))
                print("  np_col1 data: %s" % repr_row(list(np_col1), j))
                print("  np_col2 data: %s" % repr_row(list(np_col2), j))
                sys.exit(1)
            assert False, "Numpy data check failed..."

    #---------------------------------------------------------------------------
    # Operations
    #---------------------------------------------------------------------------

    @traced
    def slice_rows(self, s):
        self.df = self.df[s, :]
        if isinstance(s, slice):
            for i in range(self.ncols):
                self.data[i] = self.data[i][s]
        else:
            assert isinstance(s, list)
            for i in range(self.ncols):
                col = self.data[i]
                self.data[i] = [col[j] for j in s]
        self.nkeys = 0


    @traced
    def delete_rows(self, s):
        assert isinstance(s, slice) or isinstance(s, list)
        nrows = self.nrows
        del self.df[s, :]
        if isinstance(s, slice):
            s = list(range(nrows))[s]
        index = sorted(set(range(nrows)) - set(s))
        for i in range(self.ncols):
            col = self.data[i]
            self.data[i] = [col[j] for j in index]


    @traced
    def delete_columns(self, s):
        assert isinstance(s, slice) or isinstance(s, list)
        if isinstance(s, slice):
            s = list(range(self.ncols))[s]
        set_keys = set(range(self.nkeys))
        set_delcols = set(s)
        nkeys_remove = len(set_keys.intersection(set_delcols))

        if (nkeys_remove > 0 and nkeys_remove < self.nkeys and self.nrows > 0):
            msg = "Cannot delete a column that is a part of a multi-column key"
            with pytest.raises(ValueError, match=msg):
                del self.df[:, s]
        else:
            ncols = self.ncols
            del self.df[:, s]
            new_column_ids = sorted(set(range(ncols)) - set(s))
            self.data = [self.data[i] for i in new_column_ids]
            self.names = [self.names[i] for i in new_column_ids]
            self.types = [self.types[i] for i in new_column_ids]
            self.nkeys -= nkeys_remove


    @traced
    def cbind(self, frames):
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", dt.exceptions.DatatableWarning)
            self.df.cbind(*[iframe.df for iframe in frames])
        newdata = copy.deepcopy(self.data)
        newnames = self.names.copy()
        newtypes = self.types.copy()
        for iframe in frames:
            newdata += copy.deepcopy(iframe.data)
            newnames += iframe.names
            newtypes += iframe.types
        self.data = newdata
        self.names = newnames
        self.types = newtypes
        self.dedup_names()


    @traced
    def rbind(self, frames):
        if (self.nkeys > 0) and (self.nrows > 0):
            msg = "Cannot rbind to a keyed frame"
            with pytest.raises(ValueError, match=msg):
                self.df.rbind(*[iframe.df for iframe in frames])

        else:
            self.df.rbind(*[iframe.df for iframe in frames])

            newdata = [col.copy() for col in self.data]
            for iframe in frames:
                assert iframe.ncols == self.ncols
                for j in range(self.ncols):
                    assert self.types[j] == iframe.types[j]
                    assert self.names[j] == iframe.names[j]
                    newdata[j] += iframe.data[j]
            self.data = newdata


    @traced
    def filter_on_bool_column(self, icol):
        assert self.types[icol] is bool
        filter_col = self.data[icol]
        self.data = [[value for i, value in enumerate(column) if filter_col[i]]
                     for column in self.data]
        self.df = self.df[f[icol], :]
        self.nkeys = 0


    @traced
    def replace_nas_in_column(self, icol, replacement_value):
        assert 0 <= icol < self.ncols
        assert isinstance(replacement_value, self.types[icol])

        if icol < self.nkeys:
            msg = 'Cannot change values in a key column %s' % self.names[icol]
            msg = re.escape(msg)
            with pytest.raises(ValueError, match=msg):
                self.df[f[icol] == None, f[icol]] = replacement_value

        else:
            self.df[f[icol] == None, f[icol]] = replacement_value
            column = self.data[icol]
            for i, value in enumerate(column):
                if value is None:
                    column[i] = replacement_value


    @traced
    def sort_columns(self, a):
        self.df = self.df.sort(a)
        if self.nrows:
            data = list(zip(*self.data))
            data.sort(key=lambda x: [(x[i] is not None, x[i]) for i in a])
            self.data = list(map(list, zip(*data)))
        self.nkeys = 0


    @traced
    def cbind_numpy_column(self):
        import numpy as np
        # Numpy has no concept of "void" column (at least, not similar to ours),
        # so avoid that random type:
        coltype = None
        while coltype is None:
            coltype = random_type()
        mfraction = random.random()
        data, mmask = random_column(self.nrows, coltype, mfraction, False)

        # On Linux/Mac numpy's default int type is np.int64,
        # while on Windows it is np.int32. Here we force it to be
        # np.int64 for consistency.
        np_dtype = np.int64 if coltype == int else np.dtype(coltype)
        np_data = np.ma.array(data, mask=mmask, dtype=np_dtype)

        # Save random numpy arrays to make sure they don't change with
        # munging. Arrays that are not saved here will be eventually deleted
        # by Python, in such a case we also test datatable behaviour.
        if random.random() > 0.5:
            self.np_data += [np_data]
            self.np_data_deepcopy += [copy.deepcopy(np_data)]

        names = random_names(1)
        df = dt.Frame(np_data.T, names=names)

        for i in range(self.nrows):
            if mmask[i]: data[i] = None

        self.df.cbind(df)
        self.data += [data]
        self.types += [coltype]
        self.names += names
        self.dedup_names()


    @traced
    def add_range_column(self, name, rangeobj):
        self.data += [list(rangeobj)]
        self.names += [name]
        self.types += [int]
        self.dedup_names()
        self.df.cbind(dt.Frame(rangeobj, names=[name]))


    @traced
    def set_key_columns(self, keys, names):
        key_data = [self.data[i] for i in keys]
        unique_rows = set(zip(*key_data))
        if len(unique_rows) == self.nrows:
            self.df.key = names

            nonkeys = sorted(set(range(self.ncols)) - set(keys))
            new_column_order = keys + nonkeys

            self.types = [self.types[i] for i in new_column_order]
            self.names = [self.names[i] for i in new_column_order]
            self.nkeys = len(keys)

            if self.nrows:
                data = list(zip(*self.data))
                data.sort(key=lambda x: [(x[i] is not None, x[i]) for i in keys])
                self.data = list(map(list, zip(*data)))
                self.data = [self.data[i] for i in new_column_order]

        else:
            msg = "Cannot set a key: the values are not unique"
            with pytest.raises(ValueError, match=msg):
                self.df.key = names


    @traced
    def join_self(self):
        ncols = self.ncols
        if self.nkeys:
            self.df = self.df[:, :, join(self.df)]

            s = slice(self.nkeys, ncols)
            join_data = copy.deepcopy(self.data[s])
            join_types = self.types[s].copy()
            join_names = self.names[s].copy()

            self.data += join_data
            self.types += join_types
            self.names += join_names
            self.nkeys = 0
            self.dedup_names()

        else:
            msg = "The join frame is not keyed"
            with pytest.raises(ValueError, match=msg):
                self.df = self.df[:, :, join(self.df)]


    @traced
    def shallow_copy(self):
        # This is a noop for the python data
        self.df_shallow_copy = self.df.copy()
        self.df_deep_copy = copy.deepcopy(self.df)


    @traced
    def save_to_jay(self, filename):
        self.df.to_jay(filename)



    #---------------------------------------------------------------------------
    # Helpers
    #---------------------------------------------------------------------------

    def dedup_names(self):
        seen_names = set()
        for i, name in enumerate(self.names):
            if name in seen_names:
                base = name
                while base[-1].isdigit():
                    base = base[:-1]
                if base == name:
                    if base[-1] != '.':
                        base += "."
                    num = -1  # will be incremented to 0 in the while-loop
                else:
                    num = int(name[len(base):])
                while name in seen_names:
                    num += 1
                    name = base + str(num)
                self.names[i] = name
            seen_names.add(name)


_ltype_to_pytype = {
    dt.ltype.bool: bool,
    dt.ltype.int:  int,
    dt.ltype.real: float,
    dt.ltype.str: str,
    dt.ltype.time: datetime.datetime,
    dt.ltype.obj: object,
    dt.ltype.void: None,
}
