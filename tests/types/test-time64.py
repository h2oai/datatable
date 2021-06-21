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
    assert DT.type == dt.Type.time64
    assert DT.to_list() == [src]


def test_time64_create_from_mixed_list1():
    from datetime import date
    DT = dt.Frame([d(2001, 1, 1, 8, 30, 0), date(1999, 12, 31)])
    assert_equals(DT, dt.Frame([d(2001, 1, 1, 8, 30, 0),
                                d(1999, 12, 31, 0, 0, 0)]))


def test_time64_create_from_mixed_list2():
    from datetime import date
    DT = dt.Frame([None, date(2001, 1, 1), d(1999, 12, 31, 8, 30, 50)])
    assert_equals(DT, dt.Frame([None,
                                d(2001, 1, 1, 0, 0, 0),
                                d(1999, 12, 31, 8, 30, 50)]))



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
    assert DT.type == dt.Type.time64
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
    assert DT.type == dt.Type.time64
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


def test_convert_to_numpy_virtual(np):
    # See issue 2989
    DT = dt.Frame(range(10))
    DT[0] = dt.Type.time64
    arr = DT.to_numpy()
    assert isinstance(arr, np.ndarray)
    expected = np.array(range(10), dtype='datetime64[ns]', ndmin=2).T
    assert np.array_equal(arr, expected)


def test_convert_from_numpy(np):
    arr = np.array(['2000-01-15 17:03:01',
                    '1985-09-17 01:59:59.999999999',
                    'NaT',
                    '2021-06-01 23:18:30'],
                   dtype='datetime64[ns]')
    DT = dt.Frame(arr)
    assert DT.type == dt.Type.time64
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
    assert DT.type == dt.Type.time64
    assert DT.to_list() == [[
        d(1969, 12, 31, 23, 59, 59, 999999),
        d(1970, 1, 1, 0, 0, 0, 5),
        d(1970, 1, 1, 0, 0, 0, 9),
    ]]


def test_convert_from_numpy_ms(np):
    arr = np.array([-1, 5, 9], dtype='datetime64[ms]')
    DT = dt.Frame(arr)
    assert DT.type == dt.Type.time64
    assert DT.to_list() == [[
        d(1969, 12, 31, 23, 59, 59, 999000),
        d(1970, 1, 1, 0, 0, 0, 5000),
        d(1970, 1, 1, 0, 0, 0, 9000),
    ]]


def test_convert_from_numpy_s(np):
    arr = np.array([-1, 5, 9], dtype='datetime64[s]')
    DT = dt.Frame(arr)
    assert DT.type == dt.Type.time64
    assert DT.to_list() == [[
        d(1969, 12, 31, 23, 59, 59),
        d(1970, 1, 1, 0, 0, 5),
        d(1970, 1, 1, 0, 0, 9),
    ]]


def test_convert_from_numpy_min(np):
    arr = np.array([-1, 5, 9], dtype='datetime64[m]')
    DT = dt.Frame(arr)
    assert DT.type == dt.Type.time64
    assert DT.to_list() == [[
        d(1969, 12, 31, 23, 59, 0),
        d(1970, 1, 1, 0, 5, 0),
        d(1970, 1, 1, 0, 9, 0),
    ]]


def test_convert_from_numpy_hour(np):
    arr = np.array([-1, 5, 9], dtype='datetime64[h]')
    DT = dt.Frame(arr)
    assert DT.type == dt.Type.time64
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



#-------------------------------------------------------------------------------
# Convert to/from arrow
#-------------------------------------------------------------------------------

# Note: this test requires pandas only because arrow's `.to_pydict()`
# converts time stamps into pandas Timestamp objects
def test_convert_to_arrow(pa, pd):
    src = [d(1901, 12, 13, 0, 11, 59),
           d(2001, 2, 17, 0, 30),
           None,
           d(2077, 5, 17, 23, 59, 1, 2345),
           None,
           d(1911, 11, 11, 11, 11, 11, 11)]
    DT = dt.Frame(XYZ=src)
    assert DT.type == dt.Type.time64
    arr = DT.to_arrow()
    assert arr.shape == DT.shape
    assert arr.column_names == ["XYZ"]
    assert arr.to_pydict() == {"XYZ": [
        pd.Timestamp('1901-12-13 00:11:59'),
        pd.Timestamp('2001-02-17 00:30:00'),
        None,
        pd.Timestamp('2077-05-17 23:59:01.002345'),
        None,
        pd.Timestamp('1911-11-11 11:11:11.000011')
    ]}


def test_from_arrow_timestamp_ns(pa):
    src = [1, 1000, 20000, None, -700000]
    a = pa.array(src, type=pa.timestamp('ns'))
    tbl = pa.Table.from_arrays([a], names=["T"])
    DT = dt.Frame(tbl)
    assert DT.type == dt.Type.time64
    assert DT.names == ("T",)
    assert str(DT) == (
        "   | T                            \n"
        "   | time64                       \n"
        "-- + -----------------------------\n"
        " 0 | 1970-01-01T00:00:00.000000001\n"
        " 1 | 1970-01-01T00:00:00.000001   \n"
        " 2 | 1970-01-01T00:00:00.00002    \n"
        " 3 | NA                           \n"
        " 4 | 1969-12-31T23:59:59.9993     \n"
        "[5 rows x 1 column]\n"
    )


def test_from_arrow_timestamp_xs(pa):
    arr_s = pa.array([1348713949, -348901459, 298472245], type=pa.timestamp('s'))
    arr_ms = pa.array([3847103487310, 248702475, -243720452542], type=pa.timestamp('ms'))
    arr_us = pa.array([1384710348731230, 2487870247582475, -28599437204525], type=pa.timestamp('us'))
    assert arr_s.to_string() == ("[\n"
        "  2012-09-27 02:45:49,\n"
        "  1958-12-11 18:55:41,\n"
        "  1979-06-17 12:57:25\n]"
    )
    assert arr_ms.to_string() == ("[\n"
        "  2091-11-28 15:51:27.310,\n"
        "  1970-01-03 21:05:02.475,\n"
        "  1962-04-12 03:52:27.458\n]"
    )
    assert arr_us.to_string() == ("[\n"
        "  2013-11-17 17:45:48.731230,\n"
        "  2048-11-01 19:04:07.582475,\n"
        "  1969-02-03 23:42:42.795475\n]"
    )
    tbl = pa.Table.from_arrays([arr_s, arr_ms, arr_us], names=["s", "ms", "us"])
    DT = dt.Frame(tbl)
    assert DT.type == dt.Type.time64
    assert DT.to_dict() == {
        's' : [d(2012, 9, 27, 2, 45, 49),
               d(1958, 12, 11, 18, 55, 41),
               d(1979, 6, 17, 12, 57, 25)],
        'ms': [d(2091, 11, 28, 15, 51, 27, 310000),
               d(1970, 1, 3, 21, 5, 2, 475000),
               d(1962, 4, 12, 3, 52, 27, 458000)],
        'us': [d(2013, 11, 17, 17, 45, 48, 731230),
               d(2048, 11, 1, 19, 4, 7, 582475),
               d(1969, 2, 3, 23, 42, 42, 795475)]
    }




#-------------------------------------------------------------------------------
# Type casts into `time64` type
#-------------------------------------------------------------------------------

def test_cast_void_column_to_time64():
    DT = dt.Frame([None] * 5)
    assert DT.type == dt.Type.void
    DT[0] = dt.Type.time64
    assert DT.type == dt.Type.time64
    assert DT.to_list() == [[None] * 5]


@pytest.mark.parametrize('ttype', [dt.bool8, dt.int8, dt.int16])
def test_cast_column_to_time64_invalid(ttype):
    DT = dt.Frame([1, 0, None], stype=ttype)
    msg = "Unable to cast column of type %s into time64" % ttype.name
    with pytest.raises(TypeError, match=msg):
        DT[0] = dt.Type.time64


@pytest.mark.parametrize('ttype', [dt.int32, dt.int64])
def test_cast_int_column_to_time64(ttype):
    DT = dt.Frame([0, 10000, 10000000, 1000000000], stype=ttype)
    DT[0] = dt.Type.time64
    assert_equals(DT, dt.Frame([d(1970, 1, 1, 0, 0, 0),
                                d(1970, 1, 1, 0, 0, 0, 10),
                                d(1970, 1, 1, 0, 0, 0, 10000),
                                d(1970, 1, 1, 0, 0, 1)]))


@pytest.mark.parametrize('ttype', [dt.float32, dt.float64])
def test_cast_float_column_to_time64(ttype):
    DT = dt.Frame([0, 1e6, 1e9, None], stype=ttype)
    DT[0] = dt.Type.time64
    assert_equals(DT, dt.Frame([d(1970, 1, 1, 0, 0, 0),
                                d(1970, 1, 1, 0, 0, 0, 1000),
                                d(1970, 1, 1, 0, 0, 1),
                                None]))


def test_cast_date32_to_time64():
    from datetime import date
    DT = dt.Frame([date(2001, 3, 17)])
    assert DT.type == dt.Type.date32
    DT[0] = dt.Type.time64
    assert_equals(DT, dt.Frame([d(2001, 3, 17, 0, 0, 0)]))


def test_cast_object_to_time64():
    from datetime import date
    DT = dt.Frame([d(2001, 1, 1, 12, 0, 0), date(2021, 10, 15), 6, None, dict],
                  stype=object)
    DT[0] = dt.Type.time64
    assert_equals(DT, dt.Frame([d(2001, 1, 1, 12, 0, 0),
                                d(2021, 10, 15, 0, 0, 0),
                                None, None, None]))


@pytest.mark.parametrize('ttype', [dt.str32, dt.str64])
def test_cast_string_to_time64(ttype):
    DT = dt.Frame(["2001-07-11 12:05:59.999",
                   "2002-10-04T23:00:01",
                   "noise",
                   "2003-10-11-12"], stype=ttype)
    DT[0] = dt.Type.time64
    assert_equals(DT, dt.Frame([d(2001, 7, 11, 12, 5, 59, 999000),
                                d(2002, 10, 4, 23, 0, 1),
                                None, None]))




#-------------------------------------------------------------------------------
# Type casts from `time64` type
#-------------------------------------------------------------------------------

def test_cast_time64_to_void():
    DT = dt.Frame([d(2111, 11, 11, 11, 11, 11, 111111)])
    DT[0] = dt.Type.void
    assert_equals(DT, dt.Frame([None]))


def test_cast_time64_to_int64():
    DT = dt.Frame([d(1970, 1, 1, 0, 0, 0),
                   d(1970, 1, 1, 0, 0, 1),
                   d(1970, 1, 2, 0, 0, 0, 1),
                   d(1969, 12, 31, 23, 59, 59, 999999),
                   None])
    DT[0] = dt.Type.int64
    assert_equals(DT, dt.Frame([0,
                                1000000000,
                                86400000001000,
                                -1000,
                                None]))


def test_cast_time64_to_double():
    DT = dt.Frame([d(2013, 10, 5, 11, 23, 45, 157839)])
    DT[0] = float
    assert_equals(DT, dt.Frame([1380972225157839000.0]))


def test_cast_time64_to_date32():
    from datetime import date
    DT = dt.Frame([d(2091, 11, 28, 15, 51, 27, 310000),
                   d(1970, 1, 3, 21, 5, 2, 475000),
                   d(1969, 12, 31, 23, 59, 59, 999999),
                   d(1969, 12, 31, 0, 0, 0),
                   d(1962, 4, 12, 3, 52, 27, 458000),
                   None])
    assert DT.type == dt.Type.time64
    DT[0] = dt.Type.date32
    assert_equals(DT, dt.Frame([date(2091, 11, 28),
                                date(1970, 1, 3),
                                date(1969, 12, 31),
                                date(1969, 12, 31),
                                date(1962, 4, 12),
                                None]))


@pytest.mark.parametrize('st', [dt.str32, dt.str64])
def test_cast_time64_to_string(st):
    DT = dt.Frame([d(2091, 11, 28, 15, 51, 27, 310000),
                   d(1970, 1, 3, 21, 5, 2, 475000),
                   d(1969, 12, 31, 23, 59, 59, 999999),
                   d(1969, 12, 31, 0, 0, 0),
                   d(1962, 4, 12, 3, 52, 27, 458000),
                   None])
    DT[0] = st
    assert_equals(DT, dt.Frame(["2091-11-28T15:51:27.31",
                                "1970-01-03T21:05:02.475",
                                "1969-12-31T23:59:59.999999",
                                "1969-12-31T00:00:00",
                                "1962-04-12T03:52:27.458",
                                None], stype=st))


def test_cast_time64_to_obj():
    DT = dt.Frame([d(2000, 1, 2, 3, 4, 55)])
    assert DT.type == dt.Type.time64
    DT[0] = object
    assert DT.type == dt.Type.obj64
    assert DT.to_list() == [[d(2000, 1, 2, 3, 4, 55)]]




#-------------------------------------------------------------------------------
# Miscellaneous functionality
#-------------------------------------------------------------------------------

def test_rbind():
    src = [d(2030, 12, 1, 13, 43, 17)]
    DT = dt.Frame(src)
    assert DT.type == dt.Type.time64
    RES = dt.rbind(DT, DT)
    assert_equals(RES, dt.Frame(src * 2))


def test_rbind2():
    DT = dt.Frame([5, 7, 9])
    DT[0] = dt.Type.time64
    RES = dt.rbind(DT, DT)
    EXP = dt.Frame([5, 7, 9] * 2)
    EXP[0] = dt.Type.time64
    assert_equals(RES, EXP)


def test_materialize():
    DT = dt.Frame([d(2021, 4, 1, i, 0, 0) for i in range(24)])
    assert DT.type == dt.Type.time64
    RES = DT[::2, :]
    EXP = dt.Frame([d(2021, 4, 1, i, 0, 0) for i in range(24)[::2]])
    assert_equals(RES, EXP)
    RES.materialize()
    assert_equals(RES, EXP)


def test_sort():
    DT = dt.Frame([d(2010, 11, 5, i*17 % 23, 0, 0) for i in range(23)])
    assert_equals(DT.sort(0),
                  dt.Frame([d(2010, 11, 5, i, 0, 0) for i in range(23)]))


def test_compare():
    DT = dt.Frame(A=[d(2010, 11, 5, i*17 % 11, 0, 0) for i in range(11)])
    DT['B'] = DT.sort(0)
    RES = DT[:, {"EQ": (f.A == f.B),
                 "NE": (f.A != f.B),
                 "LT": (f.A < f.B),
                 "LE": (f.A <= f.B),
                 "GE": (f.A >= f.B),
                 "GT": (f.A > f.B)}]
    assert_equals(RES, dt.Frame(EQ = [1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
                                NE = [0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
                                LT = [0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1],
                                LE = [1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1],
                                GE = [1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0],
                                GT = [0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0],
                                type=dt.bool8))


def test_repeat():
    DT = dt.Frame(A=[d(2001, 10, 12, 0, 0, 0)])
    DT = dt.repeat(DT, 5)
    assert_equals(DT, dt.Frame(A=[d(2001, 10, 12, 0, 0, 0)] * 5))
