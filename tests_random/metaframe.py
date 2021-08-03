#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2021 H2O.ai
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
import datatable as dt
from datatable.internal import frame_integrity_check
from tests_random.utils import (
    assert_equals,
    random_column,
    random_names,
    random_type,
    repr_data,
    repr_row,
    repr_types
)


class MetaFrame:
    COUNTER = 1

    def __init__(self):
        self.df = None
        self.data = None
        self.names = None
        self.types = None
        self.nkeys = 0
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
        print(f"{frame.name} = dt.fread({repr(filename)}")
        return frame


    def save_to_jay(self, filename):
        self.df.to_jay(filename)


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
        self.check_shape()
        self.check_types()
        self.check_keys()
        if self.df.names != tuple(self.names):
            print("ERROR: df.names=%r ≠\n"
                  "       py.names=%r" % (self.df.names, tuple(self.names)))
            sys.exit(1)
        self.check_data()

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



_ltype_to_pytype = {
    dt.ltype.bool: bool,
    dt.ltype.int:  int,
    dt.ltype.real: float,
    dt.ltype.str: str,
    dt.ltype.time: datetime.datetime,
    dt.ltype.obj: object,
    dt.ltype.void: None,
}
