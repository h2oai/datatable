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
import datatable as dt
import math
import pytest
from tests import (assert_equals, list_equals, frame_integrity_check)

pandas_test = pytest.mark.usefixtures("pandas")


@pandas_test
def test_topandas():
    d0 = dt.Frame({"A": [1, 5], "B": ["hello", "you"], "C": [True, False]})
    p0 = d0.to_pandas()
    assert p0.shape == (2, 3)
    assert list_equals(p0.columns.tolist(), ["A", "B", "C"])
    assert p0["A"].values.tolist() == [1, 5]
    assert p0["B"].values.tolist() == ["hello", "you"]
    assert p0["C"].values.tolist() == [True, False]


@pandas_test
def test_topandas_view():
    d0 = dt.Frame([[1, 5, 2, 0, 199, -12],
                   ["alpha", "beta", None, "delta", "epsilon", "zeta"],
                   [.3, 1e2, -.5, 1.9, 2.2, 7.9]], names=["A", "b", "c"])
    d1 = d0[::-2, :]
    p1 = d1.to_pandas()
    assert p1.shape == d1.shape
    assert p1.columns.tolist() == ["A", "b", "c"]
    assert p1.values.T.tolist() == d1.to_list()


@pandas_test
def test_topandas_nas():
    d0 = dt.Frame([[True, None, None, None, False],
                   [1, 5, None, -16, 100],
                   [249, None, 30000, 1, None],
                   [4587074, None, 109348, 1394, -343],
                   [None, None, None, None, 134918374091834]])
    frame_integrity_check(d0)
    assert d0.stypes == (dt.bool8, dt.int32, dt.int32, dt.int32, dt.int64)
    p0 = d0.to_pandas()
    # Check that each column in Pandas DataFrame has the correct number of NAs
    assert p0.count().tolist() == [2, 4, 3, 4, 1]


@pandas_test
def test_topandas_view_mixed():
    d0 = dt.Frame(A=range(100))
    d1 = d0[7:17, :]
    d2 = dt.Frame(B=['foo', 'bar', 'buzz'] * 3 + ['finale'])
    d3 = dt.Frame(V=[2.2222])
    d3.nrows = 10
    dd = dt.cbind(d1, d2, d3)
    pp = dd.to_pandas()
    assert pp.columns.tolist() == ["A", "B", "V"]
    assert pp["A"].tolist() == list(range(7, 17))
    assert pp["B"].tolist() == d2.to_list()[0]
    assert pp["V"].tolist()[0] == 2.2222
    assert all(math.isnan(x) for x in pp["V"].tolist()[1:])


@pandas_test
def test_topandas_bool_nas():
    d0 = dt.Frame(A=[True, False, None, True])
    pf = d0.to_pandas()
    pf_values = pf["A"].values.tolist()
    assert pf_values[0] is True
    assert pf_values[1] is False
    assert pf_values[2] is None or (isinstance(pf_values[2], float) and
                                    math.isnan(pf_values[2]))
    d1 = dt.Frame(pf)
    assert_equals(d0, d1)
    d2 = dt.Frame(d0.to_numpy(), names=["A"])
    assert_equals(d0, d2)


@pandas_test
def test_topandas_strings_nas():
    DT = dt.Frame(["alpha", None, None, "omega"])
    pf = DT.to_pandas()
    assert pf.values.tolist() == [["alpha"], [None], [None], ["omega"]]


@pandas_test
def test_topandas_objects_nas():
    DT = dt.Frame([dt, None, None, [1,2,3]], type=object)
    pf = DT.to_pandas()
    assert pf.values.tolist() == [[dt], [None], [None], [[1, 2, 3]]]


@pandas_test
def test_topandas_keyed(pd):
    DT = dt.Frame(A=[3, 5, 9, 11], B=[7, 14, 1, 0], R=['va', 'dfkjv', 'q', '...'])
    DT.key = 'A'
    pf = DT.to_pandas()
    assert pf.shape == (4, 2)
    assert list(pf.columns) == ["B", "R"]
    assert pf['B'].tolist() == [7, 14, 1, 0]
    assert pf['R'].tolist() == ['va', 'dfkjv', 'q', '...']
    assert isinstance(pf.index, pd.core.indexes.numeric.Int64Index)
    assert pf.index.name == "A"
    assert pf.index.tolist() == [3, 5, 9, 11]


@pandas_test
def test_topandas_keyed2(pd):
    DT = dt.Frame(V=[2, 9, 11, 15, 99], W=[2, 4, 8, 7, 1])
    DT.key = ['V', 'W']
    pf = DT.to_pandas()
    assert pf.shape == (5, 0)
    assert list(pf.columns) == []
    assert isinstance(pf.index, pd.core.indexes.multi.MultiIndex)
    assert pf.index.names == ['V', 'W']
    assert pf.index.tolist() == [(2, 2), (9, 4), (11, 8), (15, 7), (99, 1)]


@pandas_test
def test_topandas_keyed_dates(pd):
    DT = dt.Frame(A=[0, 4, 9]/dt.stype.date32, B=['first', 'fifth', 'tenth'])
    DT.key = 'A'
    pf = DT.to_pandas()
    assert pf.shape == (3, 1)
    assert list(pf.columns) == ['B']
    assert pf['B'].tolist() == ['first', 'fifth', 'tenth']
    assert isinstance(pf.index, pd.core.indexes.datetimes.DatetimeIndex)
    assert pf.index.name == 'A'
    assert pf.index.tolist() == [
                pd.Timestamp(1970, 1, 1),
                pd.Timestamp(1970, 1, 5),
                pd.Timestamp(1970, 1, 10)]
