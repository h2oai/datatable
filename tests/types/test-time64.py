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
from datetime import datetime as d


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
    assert dt.Type.time64.min == d(1677, 9, 22, 0, 12, 43, 145225)
    assert dt.Type.time64.max == d(2262, 4, 11, 23, 47, 16, 854775)




#-------------------------------------------------------------------------------
# time64 convert to/from list of primitives
#-------------------------------------------------------------------------------

def test_time64_create_from_python():
    src = [d(2000, 10, 18, 3, 30),
           d(2010, 11, 13, 15, 11, 59),
           d(2020, 2, 29, 20, 20, 20, 20), None]
    DT = dt.Frame(src)
    assert DT.types == [dt.Type.time64]
    assert DT.to_list() == [src]



#-------------------------------------------------------------------------------
# Basic properties
#-------------------------------------------------------------------------------

def test_time64_repr():
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


def test_time64_minmax():
    src = [None,
           d(2000, 10, 18, 3, 30),
           d(2010, 11, 13, 15, 11, 59),
           d(2020, 2, 29, 20, 20, 20, 20), None]
    DT = dt.Frame(src)
    assert DT.min1() == d(2000, 10, 18, 3, 30)
    assert DT.max1() == d(2020, 2, 29, 20, 20, 20, 20)
    assert DT.countna1() == 2



#-------------------------------------------------------------------------------
# time64 convert to/from CSV
#-------------------------------------------------------------------------------

def test_time64_read_from_csv():
    DT = dt.fread("timestamp\n"
                  "2001-05-12T12:00:00\n"
                  "2013-06-24T14:00:01\n"
                  "2023-11-02T23:59:59.999999\n")
    assert DT.types == [dt.Type.time64]
    assert DT.shape == (3, 1)
    assert DT.to_list()[0] == [d(2001, 5, 12, 12),
                               d(2013, 6, 24, 14, 0, 1),
                               d(2023, 11, 2, 23, 59, 59, 999999)]


def test_time64_to_csv():
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
    src = [d(1901, 2, 3, 4, 5, 6), d(2001, 12, 13, 0, 30), d(2026, 5, 9, 12),
           None, d(1956, 11, 11, 11, 11, 11)]
    DT = dt.Frame(src)
    DT.to_jay(tempfile_jay)
    del DT
    DT2 = dt.fread(tempfile_jay)
    assert DT2.shape == (5, 1)
    assert DT2.types == [dt.Type.time64]
    assert DT2.to_list()[0] == src


def test_with_stats(tempfile_jay):
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



#-------------------------------------------------------------------------------
# Convert to/from numpy
#-------------------------------------------------------------------------------

def test_convert_to_numpy(np):
    src = [d(1901, 12, 13, 0, 11, 59),
           d(2001, 2, 17, 0, 30),
           None,
           d(2077, 5, 17, 23, 59, 1, 2345),
           None,
           d(1911, 11, 11, 11, 11, 11, 11)]
    DT = dt.Frame(src)
    assert DT.types == [dt.Type.time64]
    arr = DT.to_numpy()
    assert isinstance(arr, np.ndarray)
    assert arr.shape == DT.shape
    assert arr.dtype == np.dtype('datetime64[ns]')
    assert arr[0, 0] == np.datetime64('1901-12-13T00:11:59.000000000')
    assert arr[1, 0] == np.datetime64('2001-02-17T00:30:00.000000000')
    assert repr(arr[2, 0]) == repr(np.datetime64('NaT'))
    assert arr[3, 0] == np.datetime64('2077-05-17T23:59:01.002345000')
    assert repr(arr[4, 0]) == repr(np.datetime64('NaT'))
    assert arr[5, 0] == np.datetime64('1911-11-11T11:11:11.000011000')


def test_convert_from_numpy(np):
    arr = np.array(['2000-01-15 17:03:01',
                    '1985-09-17 01:59:59.999999999',
                    'NaT',
                    '2021-06-01 23:18:30'],
                   dtype='datetime64[ns]')
    DT = dt.Frame(arr)
    assert DT.types == [dt.Type.time64]
    # we use stringification here only because converting to python
    # literals would have lost the nanosecond precision
    assert str(DT) == (
        "   | C0                           \n"
        "   | time64                       \n"
        "-- + -----------------------------\n"
        " 0 | 2000-01-15T17:03:01          \n"
        " 1 | 1985-09-17T01:59:59.999999999\n"
        " 2 | NA                           \n"
        " 3 | 2021-06-01T23:18:30          \n"
        "[4 rows x 1 column]\n"
    )


def test_convert_from_numpy_us(np):
    arr = np.array([-1, 5, 9], dtype='datetime64[us]')
    DT = dt.Frame(arr)
    assert DT.types == [dt.Type.time64]
    assert DT.to_list() == [[
        d(1969, 12, 31, 23, 59, 59, 999999),
        d(1970, 1, 1, 0, 0, 0, 5),
        d(1970, 1, 1, 0, 0, 0, 9),
    ]]


def test_convert_from_numpy_ms(np):
    arr = np.array([-1, 5, 9], dtype='datetime64[ms]')
    DT = dt.Frame(arr)
    assert DT.types == [dt.Type.time64]
    assert DT.to_list() == [[
        d(1969, 12, 31, 23, 59, 59, 999000),
        d(1970, 1, 1, 0, 0, 0, 5000),
        d(1970, 1, 1, 0, 0, 0, 9000),
    ]]


def test_convert_from_numpy_s(np):
    arr = np.array([-1, 5, 9], dtype='datetime64[s]')
    DT = dt.Frame(arr)
    assert DT.types == [dt.Type.time64]
    assert DT.to_list() == [[
        d(1969, 12, 31, 23, 59, 59),
        d(1970, 1, 1, 0, 0, 5),
        d(1970, 1, 1, 0, 0, 9),
    ]]


def test_convert_from_numpy_min(np):
    arr = np.array([-1, 5, 9], dtype='datetime64[m]')
    DT = dt.Frame(arr)
    assert DT.types == [dt.Type.time64]
    assert DT.to_list() == [[
        d(1969, 12, 31, 23, 59, 0),
        d(1970, 1, 1, 0, 5, 0),
        d(1970, 1, 1, 0, 9, 0),
    ]]


def test_convert_from_numpy_hour(np):
    arr = np.array([-1, 5, 9], dtype='datetime64[h]')
    DT = dt.Frame(arr)
    assert DT.types == [dt.Type.time64]
    assert DT.to_list() == [[
        d(1969, 12, 31, 23, 0, 0),
        d(1970, 1, 1, 5, 0, 0),
        d(1970, 1, 1, 9, 0, 0),
    ]]



#-------------------------------------------------------------------------------
# Convert to/from pandas
#-------------------------------------------------------------------------------

def test_convert_to_pandas(pd, np):
    src = [d(1901, 12, 13, 0, 11, 59),
           d(2001, 2, 17, 0, 30),
           None,
           d(2077, 5, 17, 23, 59, 1, 2345),
           None,
           d(1911, 11, 11, 11, 11, 11, 11)]
    DT = dt.Frame(A=src, B=[3, 987, 5, 7, -1, 0])
    assert DT.types == [dt.Type.time64, dt.Type.int32]
    pf = DT.to_pandas()
    assert pf.columns.tolist() == ['A', 'B']
    assert pf.dtypes.tolist() == [np.dtype('<M8[ns]'), np.dtype('int32')]
    assert pf.values.tolist() == [
        [pd.Timestamp('1901-12-13 00:11:59'), 3],
        [pd.Timestamp('2001-02-17 00:30:00'), 987],
        [pd.NaT, 5],
        [pd.Timestamp('2077-05-17 23:59:01.002345'), 7],
        [pd.NaT, -1],
        [pd.Timestamp('1911-11-11 11:11:11.000011'), 0]
    ]


def test_convert_from_pandas(pd):
    pf = pd.DataFrame({"A": pd.to_datetime(['2000-01-01 01:01:01',
                                            '2010-11-07 23:04:11'])})
    DT = dt.Frame(pf)
    assert_equals(DT, dt.Frame(A=[d(2000, 1, 1, 1, 1, 1),
                                  d(2010, 11, 7, 23, 4, 11)]))
