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

@pytest.mark.parametrize("type", [dt.float32, dt.float64, dt.str32, dt.str64, dt.obj64])
def test_mod_error_wrong_one_type(type):
    DT = dt.Frame(A=[]/type, B=[]/dt.int32)
    type_str = str(type)[6:]
    msg = "Operator %% cannot be applied to columns of types %s and int32" % \
          (type_str,)
    with pytest.raises(TypeError, match=msg):
        DT[:, f.A % f.B]


@pytest.mark.parametrize("type", [dt.float32, dt.float64, dt.str32, dt.str64, dt.obj64])
def test_mod_error_wrong_both_types(type):
    DT = dt.Frame(A=[]/type, B=[]/type)
    type_str = str(type)[6:]
    msg = "Operator %% cannot be applied to columns of types %s and %s" % \
          (type_str, type_str)
    with pytest.raises(TypeError, match=msg):
        DT[:, f.A % f.B]


#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

def test_mod_stringify():
    assert str(f.A % 10) == "FExpr<f.A % 10>"
    assert str(f.A % f.B) == "FExpr<f.A % f.B>"
    assert str(f.A % f.B % f.C) == "FExpr<f.A % f.B % f.C>"
    assert str((f.A % f.B) % f.C) == "FExpr<f.A % f.B % f.C>"
    assert str((f.A + f.B) % f.C) == "FExpr<(f.A + f.B) % f.C>"
    assert str(f.A % (f.B % f.C)) == "FExpr<f.A % (f.B % f.C)>"
    assert str(f.A % (f.B / f.C)) == "FExpr<f.A % (f.B / f.C)>"
    assert str(f.A % (f[1]*3) % f.Z) == "FExpr<f.A % (f[1] * 3) % f.Z>"


def test_mod_boolean_columns():
    DT = dt.Frame(x=[True, True, True, False, False, False, None, None, None],
                  y=[True, False, None] * 3)
    RES = DT[:, f.x % f.y]
    EXP = dt.Frame([0, None, None, 0, None, None, None, None, None]/dt.int32)
    assert_equals(RES, EXP)


def test_mod_boolean_column_scalar():
    DT = dt.Frame(x=[True, False, None])
    RES = DT[:, [f.x % True, f.x % False, f.x % None]]
    EXP = dt.Frame([[0, 0, None]/dt.int32,
                   [None, None, None]/dt.int32,
                   [None, None, None]/dt.bool8])
    assert_equals(RES, EXP)


@pytest.mark.parametrize("seed", [random.getrandbits(63)])
def test_mod_booleans_integers(seed):
    random.seed(seed)
    n = 1000
    src1 = [random.randint(-100, 100) for _ in range(n)]
    src2 = [random.randint(-10, 10) for _ in range(n)]
    src3 = [random.randint(0, 1) for _ in range(n)]

    DT = dt.Frame(x=src1, y=src2, z=src3)
    RES = DT[:, [f.x % 100, f.x % f.y, f.y % f.z, f.z % f.x]]
    EXP = dt.Frame([
            [src1[i] % 100 for i in range(n)],
            [None if src2[i] == 0 else src1[i] % src2[i] for i in range(n)],
            [None if src3[i] == False else src2[i] % src3[i] for i in range(n)]/dt.int32,
            [None if src1[i] == 0 else src3[i] % src1[i] for i in range(n)]
          ])
    assert_equals(RES, EXP)
