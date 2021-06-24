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
import pytest
import random
from datatable import dt, f, FExpr
from tests import assert_equals


@pytest.mark.parametrize('ttype',
    [dt.bool8, dt.int8, dt.int16, dt.int32, dt.int64, dt.float32, dt.float64,
     dt.Type.date32, dt.Type.time64, dt.obj64])
def test_slice_wrong_types(ttype):
    msg = "Slice expression can only be applied to a column of string type"
    with pytest.raises(TypeError, match=msg):
        DT = dt.Frame(A=[None], type=ttype)
        DT[:, f.A[:1]]


@pytest.mark.parametrize('ttype',
    [dt.bool8, dt.float32, dt.float64, dt.str32, dt.str64,
     dt.Type.date32, dt.Type.time64, dt.obj64])
def test_slice_wrong_types2(ttype):
    DT = dt.Frame(A=[None], B=["Hello"], types={"A": ttype})
    msg = 'Non-integer expressions cannot be used inside a slice'
    with pytest.raises(TypeError, match=msg):
        DT[:, f.B[f.A:]]
    with pytest.raises(TypeError, match=msg):
        DT[:, f.B[:f.A]]
    with pytest.raises(TypeError, match=msg):
        DT[:, f.B[::f.A]]


@pytest.mark.parametrize('slice',
    [slice(None, 5), slice(3, None), slice(None, None, 1), slice(-1, None),
     slice(2, 7), slice(3, -1), slice(5, -8), slice(-8, 5), slice(None, -2)])
def test_slice_normal(slice):
    src = ["Hydrogen", "Helium", "Lithium", "Berillium", "Boron"]
    DT = dt.Frame(A=src)
    RES = DT[:, f.A[slice]]
    EXP = dt.Frame(A=[s[slice] for s in src])
    assert_equals(RES, EXP)
