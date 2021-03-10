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
import random
from datatable import stype
from tests import (assert_equals, isview, list_equals)

numpy_test = pytest.mark.usefixtures("numpy")



#-------------------------------------------------------------------------------
# .to_numpy() basic
#-------------------------------------------------------------------------------

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


@numpy_test
def test_tonumpy1(np):
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
def test_empty_frame():
    DT = dt.Frame()
    assert DT.shape == (0, 0)
    a = DT.to_numpy()
    assert a.shape == (0, 0)
    assert a.tolist() == []




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
    assert isview(d1)
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


@numpy_test
def test_tonumpy_with_stype(numpy):
    """
    Test using dt.to_numpy() with explicit `stype` argument.
    """
    src = [[1.5, 2.6, 5.4], [3, 7, 10]]
    d0 = dt.Frame(src)
    assert d0.stypes == (stype.float64, stype.int32)
    a1 = d0.to_numpy("float32")
    a2 = d0.to_numpy()
    del d0
    # Check using list_equals(), which allows a tolerance of 1e-6, because
    # conversion to float32 resulted in loss of precision
    assert list_equals(a1.T.tolist(), src)
    assert a2.T.tolist() == src
    assert a1.dtype == numpy.dtype("float32")
    assert a2.dtype == numpy.dtype("float64")


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
    assert isinstance(a0, np.ma.core.MaskedArray)
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
    ar = DT.to_numpy()
    assert list_equals(ar.T.tolist(), data)


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
