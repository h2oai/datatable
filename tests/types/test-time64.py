#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2021 H2O.ai
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
import datetime
import pytest
import random
from datatable import dt, f
from tests import assert_equals



#-------------------------------------------------------------------------------
# Type.time64 and its basic properties
#-------------------------------------------------------------------------------

def test_time64_name():
    assert repr(dt.Type.time64) == "Type.time64"
    assert dt.Type.time64.name == "time64"


def test_time64_type_from_basic():
    assert dt.Type("time") == dt.Type.time64
    assert dt.Type("time64") == dt.Type.time64
    assert dt.Type(datetime.datetime) == dt.Type.time64


def test_time64_type_from_numpy(np):
    assert dt.Type(np.dtype("datetime64[s]")) == dt.Type.time64
    assert dt.Type(np.dtype("datetime64[ms]")) == dt.Type.time64
    assert dt.Type(np.dtype("datetime64[us]")) == dt.Type.time64
    assert dt.Type(np.dtype("datetime64[ns]")) == dt.Type.time64


def test_time64_type_from_pyarrow(pa):
    # pyarrow.date64: ms since unix epoch
    assert dt.Type(pa.date64()) == dt.Type.time64


def test_time64_type_minmax():
    d = datetime.datetime
    assert dt.Type.time64.min == d(1677, 9, 22, 0, 12, 43, 145225)
    assert dt.Type.time64.max == d(2262, 4, 11, 23, 47, 16, 854775)




#-------------------------------------------------------------------------------
# time64 column: create
#-------------------------------------------------------------------------------

def test_time64_create_from_python():
    d = datetime.datetime
    src = [d(2000, 10, 18, 3, 30),
           d(2010, 11, 13, 15, 11, 59),
           d(2020, 2, 29, 20, 20, 20, 20), None]
    DT = dt.Frame(src)
    assert DT.types == [dt.Type.time64]
    assert DT.to_list() == [src]


def test_time64_read_from_csv():
    d = datetime.datetime
    DT = dt.fread("timestamp\n"
                  "2001-05-12T12:00:00\n"
                  "2013-06-24T14:00:01\n"
                  "2023-11-02T23:59:59.999999\n")
    assert DT.types == [dt.Type.time64]
    assert DT.shape == (3, 1)
    assert DT.to_list()[0] == [d(2001, 5, 12, 12),
                               d(2013, 6, 24, 14, 0, 1),
                               d(2023, 11, 2, 23, 59, 59, 999999)]




#-------------------------------------------------------------------------------
# time64 convert to ...
#-------------------------------------------------------------------------------

def test_time64_repr():
    d = datetime.datetime
    src = [d(2000, 10, 18, 3, 30),
           d(2010, 11, 13, 15, 11, 59),
           d(2020, 2, 29, 20, 20, 20, 20), None]
    DT = dt.Frame(src)
    assert str(DT) == (
        "   | C0                       \n"
        "   | time64                   \n"
        "-- + -------------------------\n"
        " 0 | 2000-10-18T03:30:00      \n"
        " 1 | 2010-11-13T15:11:59      \n"
        " 2 | 2020-02-29T20:20:20.00002\n"
        " 3 | NA                       \n"
        "[4 rows x 1 column]\n"
    )


def test_time64_to_csv():
    d = datetime.datetime
    src = [d(2000, 10, 18, 3, 30, 0),
           d(2010, 11, 13, 15, 11, 59),
           d(2020, 2, 29, 20, 20, 20, 20),
           None,
           d(1690, 10, 17, 12, 0, 0, 1)]
    DT = dt.Frame(src)
    assert DT.to_csv() == (
        "C0\n"
        "2000-10-18T03:30:00\n"
        "2010-11-13T15:11:59\n"
        "2020-02-29T20:20:20.00002\n"
        "\n"
        "1690-10-17T12:00:00.000001\n"
    )



#-------------------------------------------------------------------------------
# Convert to/from Jay
#-------------------------------------------------------------------------------

def test_save_to_jay(tempfile_jay):
    d = datetime.datetime
    src = [d(1901, 2, 3, 4, 5, 6), d(2001, 12, 13, 0, 30), d(2026, 5, 9, 12),
           None, d(1956, 11, 11, 11, 11, 11)]
    DT = dt.Frame(src)
    DT.to_jay(tempfile_jay)
    del DT
    DT2 = dt.fread(tempfile_jay)
    assert DT2.shape == (5, 1)
    assert DT2.types == [dt.Type.time64]
    assert DT2.to_list()[0] == src


@pytest.mark.xfail
def test_with_stats(tempfile_jay):
    d = datetime.datetime
    src = [d(1901, 12, 13, 0, 11, 59),
           d(2001, 2, 17, 0, 30),
           None,
           d(2026, 5, 19, 12, 0, 1, 1111),
           None,
           d(1956, 11, 11, 11, 11, 11)]
    DT = dt.Frame(src)
    # precompute stats so that they get stored in the Jay file
    assert DT.countna1() == 2
    assert DT.min1() == d(1901, 12, 13, 0, 11, 59)
    assert DT.max1() == d(2026, 5, 19, 12, 0, 1, 1111)
    DT.to_jay(tempfile_jay)
    DTnew = dt.fread(tempfile_jay)
    # assert_equals() also checks frame integrity, including validity of stats
    assert_equals(DTnew, DT)
    assert DTnew.countna1() == 2
    assert DTnew.min1() == d(1901, 12, 13, 0, 11, 59)
    assert DTnew.max1() == d(2026, 5, 19, 12, 0, 1, 1111)
