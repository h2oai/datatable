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
import datatable as dt
import pytest



def test_date32_name():
    assert repr(dt.Type.date32) == "Type.date32"
    assert dt.Type.date32.name == "date32"


def test_date32_type_from_basic():
    assert dt.Type("date") == dt.Type.date32
    assert dt.Type("date32") == dt.Type.date32
    assert dt.Type(datetime.date) == dt.Type.date32


def test_date32_type_from_numpy(np):
    assert dt.Type(np.dtype("datetime64[D]")) == dt.Type.date32


def test_date32_type_from_pyarrow(pa):
    assert dt.Type(pa.date32()) == dt.Type.date32



def test_date32_create_from_python():
    d = datetime.date
    src = [d(2000, 1, 5), d(2010, 11, 23), d(2020, 2, 29), None]
    DT = dt.Frame(src)
    assert DT.types == [dt.Type.date32]
    assert DT.to_list() == [src]


def test_date32_create_from_python_force():
    d = datetime.date
    DT = dt.Frame([18675, -100000, None, 0.75, 'hello'], stype='date32')
    assert DT.types == [dt.Type.date32]
    assert DT.to_list() == [
        [d(2021, 2, 17), d(1696, 3, 17), None, d(1970, 1, 1), None]
    ]



def test_date32_repr():
    d = datetime.date
    src = [d(1, 1, 1), d(2001, 12, 13), d(5756, 5, 9)]
    DT = dt.Frame(src)
    assert str(DT) == (
        "   | C0        \n"
        "   | date32    \n"
        "-- + ----------\n"
        " 0 | 0001-01-01\n"
        " 1 | 2001-12-13\n"
        " 2 | 5756-05-09\n"
        "[3 rows x 1 column]\n"
    )


def test_date32_min():
    DT = dt.Type.date32.min
    assert isinstance(DT, dt.Frame)
    assert str(DT) == (
        "   | min           \n"
        "   | date32        \n"
        "-- + --------------\n"
        " 0 | -5877641-06-24\n"
        "[1 row x 1 column]\n"
    )


def test_date32_max():
    DT = dt.Type.date32.max
    assert isinstance(DT, dt.Frame)
    assert str(DT) == (
        "   | max          \n"
        "   | date32       \n"
        "-- + -------------\n"
        " 0 | 5879610-09-09\n"
        "[1 row x 1 column]\n"
    )


#-------------------------------------------------------------------------------
# Convert from numpy
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("scale", ["D", "W", "M", "Y"])
def test_date32_from_numpy_datetime64(np, scale):
    arr = np.array(range(-100, 1000), dtype=f'datetime64[{scale}]')
    DT = dt.Frame(arr)
    assert DT.types == [dt.Type.date32]
    assert DT.to_list() == [arr.tolist()]


def test_from_numpy_with_nats(np):
    arr = np.array(['2000-01-01', '2020-05-11', 'NaT'], dtype='datetime64[D]')
    DT = dt.Frame(arr)
    assert DT.types == [dt.Type.date32]
    assert DT.to_list() == [
        [datetime.date(2000, 1, 1), datetime.date(2020, 5, 11), None]
    ]



#-------------------------------------------------------------------------------
# Write to csv
#-------------------------------------------------------------------------------

def test_write_to_csv():
    d = datetime.date
    DT = dt.Frame([d(2001, 3, 15), d(1788, 6, 21), d(2030, 7, 1)])
    assert DT.to_csv() == "C0\n2001-03-15\n1788-06-21\n2030-07-01\n"


def test_write_dates_around_year_zero():
    day0 = -719528
    DT = dt.Frame([day0, day0 + 20, day0 + 1000, day0 + 10000, day0 + 100000,
                   day0 - 1, day0 - 1000, day0 - 10000, day0 - 100000],
                  stype='date32')
    assert DT.to_csv() == (
        "C0\n"
        "0000-01-01\n"
        "0000-01-21\n"
        "0002-09-27\n"
        "0027-05-19\n"
        "0273-10-16\n"
        "-001-12-31\n"
        "-003-04-06\n"
        "-028-08-15\n"
        "-274-03-18\n"
    )


def test_write_huge_dates():
    assert dt.Type.date32.min.to_csv() == "min\n-5877641-06-24\n"
    assert dt.Type.date32.max.to_csv() == "max\n5879610-09-09\n"

    
    
#-------------------------------------------------------------------------------
# Convert to pandas
#-------------------------------------------------------------------------------

def test_date32_to_pandas(pd):
    d = datetime.date
    DT = dt.Frame([d(2000, 1, 1), d(2005, 7, 12), d(2020, 2, 29),
                   d(1677, 9, 22), None])
    pf = DT.to_pandas()
    assert pf.shape == (5, 1)
    assert list(pf.columns) == ['C0']
    assert pf['C0'].dtype.name == 'datetime64[ns]'
    assert pf['C0'].to_list() == [
        pd.Timestamp(2000, 1, 1),
        pd.Timestamp(2005, 7, 12),
        pd.Timestamp(2020, 2, 29),
        pd.Timestamp(1677, 9, 22),
        pd.NaT
    ]


# Note: pandas stores dates as `datetime64[ns]` type, meaning they are
# indistinguishable from full `time64` columns.
# For now, we convert such columns to strings; but in the future when
# we add time64 type, we'd be able to write a function that checks
# whether a pandas datetime64 column contains dates only, and convert
# it to dates in such case.
