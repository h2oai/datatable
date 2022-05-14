#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2022 H2O.ai
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
import random
from datatable import stype
from tests import (assert_equals, list_equals)

numpy_test = pytest.mark.usefixtures("numpy")



#-------------------------------------------------------------------------------
# .to_numpy() basic
#-------------------------------------------------------------------------------

@numpy_test
def test_empty_frame():
    DT = dt.Frame()
    assert DT.shape == (0, 0)
    a = DT.to_numpy()
    assert a.shape == (0, 0)
    assert a.tolist() == []


@numpy_test
def test_tonumpy0(np):
    d0 = dt.Frame([1, 3, 5, 7, 9])
    assert d0.stype == dt.int32
    a0 = d0.to_numpy()
    assert a0.shape == d0.shape
    assert a0.dtype == np.dtype("int32")
    assert a0.tolist() == [[1], [3], [5], [7], [9]]
    a1 = np.array(d0)
    assert (a0 == a1).all()
    a2 = d0.to_numpy(c_contiguous = True)
    assert (a0 == a2).all()


@numpy_test
def test_tonumpy_incompatible_types(np):
    d0 = dt.Frame({"A": [1, 5],
                   "B": ["helo", "you"],
                   "C": [True, False],
                   "D": [3.4, None]})
    msg = "Frame .* columns of incompatible types"
    with pytest.raises(TypeError, match=msg):
        a0 = d0.to_numpy()
    a0 = d0.to_numpy(type=object)
    assert a0.shape == d0.shape
    assert a0.dtype == np.dtype("object")
    assert list_equals(a0.T.tolist(), d0.to_list())


@numpy_test
def test_tonumpy_void(np):
    DT = dt.Frame([[None]*10] * 3)
    assert DT.shape == (10, 3)
    a = DT.to_numpy()
    assert a.shape == DT.shape
    assert a.dtype == np.dtype('float64')
    assert all(len(row) == 3 and all(math.isnan(x) for x in row)
               for row in a.tolist())


@numpy_test
@pytest.mark.parametrize('src_type',
    [dt.bool8, dt.int8, dt.int16, dt.int32, dt.int64, dt.float32, dt.float64])
def test_tonumpy_numerics(np, src_type):
    DT = dt.Frame([-3, 3, 5, 0, 2], stype=src_type)
    a = DT.to_numpy()
    assert dt.Type(a.dtype) == DT.types[0]
    assert a.shape == DT.shape
    assert a.T.tolist() == DT.to_list()


@numpy_test
@pytest.mark.parametrize('src_type', [dt.str32, dt.str64])
def test_tonumpy_strings(np, src_type):
    DT = dt.Frame(["veni", "vidi", "vici"], stype=src_type)
    a = DT.to_numpy()
    assert a.dtype == np.dtype('object')
    assert a.shape == DT.shape
    assert a.T.tolist() == DT.to_list()


@numpy_test
def test_tonumpy_date32(np):
    from datetime import date as d
    DT = dt.Frame([d(2001, 1, 1), d(2002, 3, 5), d(2012, 2, 7), d(2020, 3, 9)])
    assert DT.types[0] == dt.Type.date32
    a = DT.to_numpy()
    assert a.dtype == np.dtype('datetime64[D]')
    assert a.shape == DT.shape
    assert a.T.tolist() == DT.to_list()


@numpy_test
def test_tonumpy_time64(np, pd):
    from datetime import datetime as d
    DT = dt.Frame([d(2001, 1, 1, 10, 10, 10, 1), d(2002, 3, 5, 0, 0, 0, 5),
                   d(2012, 2, 7, 15, 5, 5), d(2020, 3, 9, 6, 3, 2, 133)])
    assert DT.types[0] == dt.Type.time64
    a = DT.to_numpy()
    assert a.dtype == np.dtype('datetime64[ns]')
    assert a.shape == DT.shape
    # use pandas to convert to datetime
    assert [[pd.Timestamp(ts).to_pydatetime() for ts in a.T[0]]] == DT.to_list()


@numpy_test
def test_tonumpy_with_upcast():
    from math import nan
    DT = dt.Frame(A=[3, 7, 8], B=[True, False, False], C=[2.1, 7.7, 9.1],
                  D=[None]*3)
    a = DT.to_numpy()
    assert a.shape == DT.shape
    assert list_equals(
                a.tolist(),
                [[3.0, 1.0, 2.1, nan],
                 [7.0, 0.0, 7.7, nan],
                 [8.0, 0.0, 9.1, nan]]
            )




#-------------------------------------------------------------------------------
# Convert data with NAs
#-------------------------------------------------------------------------------

@numpy_test
def test_tonumpy_ints_with_NAs(np):
    src = [1, 5, None, 187, None, 103948]
    d0 = dt.Frame(src)
    a0 = d0.to_numpy()
    assert isinstance(a0, np.ma.core.MaskedArray)
    assert a0.dtype == np.dtype('int32')
    assert a0.T.tolist() == [src]


@numpy_test
def test_tonumpy_floats_with_NAs(np):
    from math import nan, inf
    src = [[2.3, 11.89, None, inf], [4, None, nan, -12]]
    d0 = dt.Frame(src)
    a0 = d0.to_numpy()
    assert isinstance(a0, np.ndarray)
    assert a0.dtype == np.dtype('float64')
    assert list_equals(a0.T.tolist(), src)


@numpy_test
def test_tonumpy_strings_with_NAs(np):
    src = ["faa", None, "", "hooray", None]
    d0 = dt.Frame(src)
    a0 = d0.to_numpy()
    assert isinstance(a0, np.ndarray)
    assert a0.dtype == np.dtype('object')
    assert a0.T.tolist() == [src]


@numpy_test
def test_tonumpy_booleans_with_NAs(np):
    src = [True, False, None]
    d0 = dt.Frame(src)
    a0 = d0.to_numpy()
    assert a0.dtype == np.dtype("bool")
    assert a0.T.tolist() == [src]


@numpy_test
@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_tonumpy_with_NAs_random(seed, numpy):
    random.seed(seed)
    n = int(random.expovariate(0.001) + 1)
    m = int(random.expovariate(0.2) + 1)
    data = [None] * m
    for j in range(m):
        threshold = 0.1 * (m + 1);
        vec = [random.random() for i in range(n)]
        for i, x in enumerate(vec):
            if x < threshold:
                vec[i] = None
        data[j] = vec
    DT = dt.Frame(data, stype=dt.float64)
    NP0 = DT.to_numpy()
    assert list_equals(NP0.T.tolist(), data)
    assert NP0.flags["F_CONTIGUOUS"] == True
    NP1 = DT.to_numpy(c_contiguous = True)
    assert list_equals(NP1.T.tolist(), data)
    assert NP1.flags["C_CONTIGUOUS"] == True



@numpy_test
def test_tonumpy_date32_with_nas(np):
    from datetime import date as d
    DT = dt.Frame([d(1990, 5, 28), d(980, 3, 1), None, d(2096, 11, 30)])
    assert DT.types[0] == dt.Type.date32
    a = DT.to_numpy()
    assert a.dtype == np.dtype('datetime64[D]')
    assert a.shape == DT.shape
    assert a.T.tolist() == DT.to_list()




#-------------------------------------------------------------------------------
# np.array(DT) constructor
#-------------------------------------------------------------------------------

@numpy_test
def test_numpy_constructor_simple(np):
    tbl = [[1, 4, 27, 9, 22], [-35, 5, 11, 2, 13], [0, -1, 6, 100, 20]]
    d0 = dt.Frame(tbl)
    assert d0.shape == (5, 3)
    assert d0.stype == dt.int32
    assert d0.to_list() == tbl
    n0 = np.array(d0)
    assert n0.shape == d0.shape
    assert n0.dtype == np.dtype("int32")
    assert n0.T.tolist() == tbl


@numpy_test
def test_numpy_constructor_empty(numpy):
    d0 = dt.Frame()
    assert d0.shape == (0, 0)
    n0 = numpy.array(d0)
    assert n0.shape == (0, 0)
    assert n0.tolist() == []


@numpy_test
def test_numpy_constructor_multi_types(numpy):
    # Test that multi-types datatable will be promoted into a common type
    tbl = [[1, 5, 10],
           [True, False, False],
           [30498, 1349810, -134308],
           [1.454, 4.9e-23, 10000000]]
    d0 = dt.Frame(tbl)
    assert d0.stypes == (dt.int32, dt.bool8, dt.int32, dt.float64)
    n0 = numpy.array(d0)
    assert n0.dtype == numpy.dtype("float64")
    assert n0.T.tolist() == [[1.0, 5.0, 10.0],
                             [1.0, 0, 0],
                             [30498, 1349810, -134308],
                             [1.454, 4.9e-23, 1e7]]
    assert (d0.to_numpy() == n0).all()


@numpy_test
def test_numpy_constructor_view(numpy):
    d0 = dt.Frame([range(100), range(0, 1000000, 10000)])
    d1 = d0[::-2, :]
    n1 = numpy.array(d1)
    assert n1.dtype == numpy.dtype("int32")
    assert n1.T.tolist() == [list(range(99, 0, -2)),
                             list(range(990000, 0, -20000))]
    assert (d1.to_numpy() == n1).all()


@numpy_test
def test_numpy_constructor_single_col(numpy):
    d0 = dt.Frame([1, 1, 3, 5, 8, 13, 21, 34, 55])
    assert d0.stype == dt.int32
    n0 = numpy.array(d0)
    assert n0.shape == d0.shape
    assert n0.dtype == numpy.dtype("int32")
    assert (n0 == d0.to_numpy()).all()


@numpy_test
def test_numpy_constructor_single_string_col(np):
    d = dt.Frame(["adf", "dfkn", "qoperhi"])
    assert d.shape == (3, 1)
    assert d.stypes == (stype.str32, )
    a = np.array(d)
    assert a.shape == d.shape
    assert a.dtype == np.dtype("object")
    assert a.T.tolist() == d.to_list()
    assert (a == d.to_numpy()).all()


@numpy_test
def test_numpy_constructor_view_1col(numpy):
    d0 = dt.Frame({"A": [1, 2, 3, 4], "B": [True, False, True, False]})
    d2 = d0[::2, "B"]
    a = d2.to_numpy()
    assert a.T.tolist() == [[True, True]]
    assert (a == numpy.array(d2)).all()




#-------------------------------------------------------------------------------
# .to_numpy(type=...)
#-------------------------------------------------------------------------------

@numpy_test
def test_tonumpy_with_type(np):
    src = [[1.5, 2.6, 5.4], [3, 7, 10]]
    DT = dt.Frame(src)
    assert DT.types == [dt.Type.float64, dt.Type.int32]
    a1 = DT.to_numpy(type="float32")
    a2 = DT.to_numpy()
    del DT
    # Check using list_equals(), which allows a tolerance of 1e-6, because
    # conversion to float32 resulted in loss of precision
    assert list_equals(a1.T.tolist(), src)
    assert a2.T.tolist() == src
    assert a1.dtype == np.dtype("float32")
    assert a2.dtype == np.dtype("float64")


@numpy_test
def test_tonumpy_stype(np):
    # stype= is a synonym for type=
    DT = dt.Frame([3, 14, 12])
    assert DT.types == [dt.Type.int32]
    a = DT.to_numpy(stype='int8')
    assert a.dtype == np.dtype('int8')
    assert a.T.tolist() == [[3, 14, 12]]


@numpy_test
def test_tonumpy_valid_types(np):
    DT = dt.Frame([7.4, 2.1, 0.0, 9.3])
    types = [dt.Type.int64, dt.int64, 'int64', int, np.int64, np.dtype('int64')]
    for src_type in types:
        a = DT.to_numpy(type=src_type)
        assert a.dtype == np.dtype('int64')
        assert a.tolist() == [[7], [2], [0], [9]]


@numpy_test
def test_tonumpy_invalid_types(np):
    msg = "Cannot create Type object"
    DT = dt.Frame(range(10))
    types = [1, "whatever", np, dt.Frame]
    for src_type in types:
        with pytest.raises(ValueError, match=msg):
            DT.to_numpy(type=src_type)




#-------------------------------------------------------------------------------
# .to_numpy(column=...)
#-------------------------------------------------------------------------------

@numpy_test
def test_tonumpy_column():
    DT = dt.Frame(A=[1, 2, 3], B=['a', 'b', 'c'], C=[2.3, 4.5, 6.9])
    a0 = DT.to_numpy(column=0)
    a1 = DT.to_numpy(column=1)
    a2 = DT.to_numpy(column=2)
    assert a0.shape == a1.shape == a2.shape == (3,)
    assert a0.tolist() == [1, 2, 3]
    assert a1.tolist() == ['a', 'b', 'c']
    assert a2.tolist() == [2.3, 4.5, 6.9]
    alast = DT.to_numpy(column=-1)
    assert alast.tolist() == a2.tolist()


@numpy_test
def test_tonumpy_column_invalid():
    DT = dt.Frame(A=[1, 2, 3], B=['a', 'b', 'c'], C=[2.3, 4.5, 6.9])
    msg = "Column index -5 is invalid"
    with pytest.raises(IndexError, match=msg):
        DT.to_numpy(column=-5)
    msg = "Argument column in .* should be an integer"
    with pytest.raises(TypeError, match=msg):
        DT.to_numpy(column='first')


@numpy_test
def  test_tonumpy_column_with_type(np):
    DT = dt.Frame(A=[1, 2, 3], B=['a', 'b', 'c'], C=[2.3, 4.5, 6.9])
    a = DT.to_numpy(type='int32', column=-1)
    assert a.shape == (3,)
    assert a.dtype == np.dtype('int32')
    assert a.tolist() == [2, 4, 6]




#-------------------------------------------------------------------------------
# Issues
#-------------------------------------------------------------------------------

@numpy_test
def test_tonumpy_with_NAs_view():
    # See issue #1738
    X = dt.Frame(A=[5.7, 2.3, None, 4.4, 9.8, None])[1:, :]
    a = X.to_numpy()
    assert list_equals(a.tolist(), [[2.3], [None], [4.4], [9.8], [None]])


@numpy_test
def test_tonumpy_issue2050():
    n = 1234
    DT = dt.Frame(A=[1,2,None,4,5], B=range(5), C=[4, None, None, None, 4], stype=int)
    DT = dt.repeat(DT, n)
    assert DT.sum().to_list() == [[12*n], [10*n], [8*n]]
    assert DT.to_numpy().sum() == 30*n
