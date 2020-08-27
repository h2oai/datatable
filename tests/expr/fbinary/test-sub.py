#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2020 H2O.ai
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
from datatable import dt, f
from tests import assert_equals


#-------------------------------------------------------------------------------
# Errors
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("type", [dt.str32, dt.str64, dt.obj64])
def test_sub_error_wrong_one_type(type):
    DT = dt.Frame(A=[]/type, B=[]/dt.int32)
    type_str = str(type)[6:]
    msg = r"Operator \- cannot be applied to columns of types %s and int32" % \
          (type_str,)
    with pytest.raises(TypeError, match=msg):
        DT[:, f.A - f.B]


@pytest.mark.parametrize("type", [dt.str32, dt.str64, dt.obj64])
def test_sub_error_wrong_both_types(type):
    DT = dt.Frame(A=[]/type, B=[]/type)
    type_str = str(type)[6:]
    msg = r"Operator \- cannot be applied to columns of types %s and %s" % \
          (type_str, type_str)
    with pytest.raises(TypeError, match=msg):
        DT[:, f.A - f.B]


#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

def test_sub_stringify():
    assert str(f.A - 1) == "FExpr<f.A - 1>"
    assert str(f.A - f.B) == "FExpr<f.A - f.B>"
    assert str(f.A - f.B - f.C) == "FExpr<f.A - f.B - f.C>"
    assert str((f.A - f.B) - f.C) == "FExpr<f.A - f.B - f.C>"
    assert str(f.A - (f.B - f.C)) == "FExpr<f.A - (f.B - f.C)>"
    assert str(f.A - f[1]*3 - f.Z) == "FExpr<f.A - f[1] * 3 - f.Z>"


def test_sub_booleans():
    DT = dt.Frame(A=[True, True, True, False, False, False, None, None, None],
                  B=[True, False, None] * 3)
    assert_equals(DT[:, f.A - f.B],
                  dt.Frame([0, 1, None, -1, 0, None, None, None, None]))


def test_sub_integers():
    DT = dt.Frame(A=[3, 12, 724, None], B=[0, -11, 2, 9])
    assert_equals(DT[:, f.A - f.B],
                  dt.Frame([3, 23, 722, None]))


def test_sub_integers_with_upcast():
    # int8 + int8 = int32
    DT = dt.Frame(A=[17, 111]/dt.int8, B=[-17, -28]/dt.int8)
    assert_equals(DT[:, f.A - f.B],
                  dt.Frame([34, 139]/dt.int32))


def test_integer_sub_scalar():
    DT = dt.Frame(i8=[77, 101, 127]/dt.int8,
                  i32=[14, None, 99]/dt.int32,
                  i64=[7923, -121, 903]/dt.int64)
    assert_equals(DT[:, f[:] - 1],
                  dt.Frame([[76, 100, 126]/dt.int32,
                            [13, None, 98]/dt.int32,
                            [7922, -122, 902]/dt.int64]))


@pytest.mark.parametrize("seed", [random.getrandbits(63)])
def test_sub_booleans_integers_floats_random(seed):
    random.seed(seed)
    n = 1000
    src1 = [random.randint(-100, 100) for _ in range(n)]
    src2 = [random.randint(0, 1) for _ in range(n)]
    src3 = [random.random() * 1000 - 500 for _ in range(n)]

    DT = dt.Frame(x=src1, y=src2/dt.bool8, z=src3)
    RES = DT[:, [f.x - f.y, f.y - f.x,
                 f.y - f.z, f.z - f.y,
                 f.z - f.x, f.x - f.z]]
    EXP = dt.Frame(
            C0=[src1[i] - src2[i] for i in range(n)],
            C2=[src2[i] - src3[i] for i in range(n)],
            C4=[src3[i] - src1[i] for i in range(n)]
          )
    assert_equals(RES[:, [0, 2, 4]], EXP)
    assert_equals(RES[:, [0, 2, 4]],
                  RES[:, {"C0":-f[1], "C2":-f[3], "C4":-f[5]}])