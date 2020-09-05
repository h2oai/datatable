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
from datatable import dt, f, math
from tests import assert_equals


#-------------------------------------------------------------------------------
# Errors
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("type", [dt.str32, dt.str64, dt.obj64])
def test_mod_error_wrong_one_type(type):
    DT = dt.Frame(A=[]/type, B=[]/dt.int32)
    type_str = str(type)[6:]
    msg = r"Operator \*\* cannot be applied to columns of types %s and int32" % \
          (type_str,)
    with pytest.raises(TypeError, match=msg):
        DT[:, f.A ** f.B]


@pytest.mark.parametrize("type", [dt.str32, dt.str64, dt.obj64])
def test_mod_error_wrong_both_types(type):
    DT = dt.Frame(A=[]/type, B=[]/type)
    type_str = str(type)[6:]
    msg = r"Operator \*\* cannot be applied to columns of types %s and %s" % \
          (type_str, type_str)
    with pytest.raises(TypeError, match=msg):
        DT[:, f.A ** f.B]


#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

def test_pow_stringify():
    assert str(f.A ** 10) == "FExpr<f.A ** 10>"
    assert str(f.A ** f.B) == "FExpr<f.A ** f.B>"
    assert str(f.A ** f.B ** f.C) == "FExpr<f.A ** (f.B ** f.C)>"
    assert str((f.A ** f.B) ** f.C) == "FExpr<(f.A ** f.B) ** f.C>"
    assert str(f.A ** (f.B ** f.C)) == "FExpr<f.A ** (f.B ** f.C)>"
    assert str((f.A + f.B) ** f.C) == "FExpr<(f.A + f.B) ** f.C>"
    assert str(f.A ** (f.B / f.C)) == "FExpr<f.A ** (f.B / f.C)>"
    assert str(f.A ** (f[1]*3) ** f.Z) == "FExpr<f.A ** ((f[1] * 3) ** f.Z)>"


def test_pow_boolean_columns():
    DT = dt.Frame(x=[True, True, True, False, False, False, None, None, None],
                  y=[True, False, None] * 3)
    RES = DT[:, f.x ** f.y]
    EXP = dt.Frame([1, 1, None, 0, 1, None, None, None, None]/dt.float64)
    assert_equals(RES, EXP)


def test_pow_boolean_column_scalar():
    DT = dt.Frame(x=[True, False, None]/dt.bool8)
    RES = DT[:, f.x ** 2]
    EXP = dt.Frame([1, 0, None]/dt.float64)
    assert_equals(RES, EXP)


def test_pow_integers():
    DT = dt.Frame(range(10))
    R1 = DT[:, f[0]**2]
    assert_equals(R1, dt.Frame([i**2 for i in range(10)]/dt.float64))


def test_pow_integers2():
    # Note: large integers may overflow producing an undefined result
    DT = dt.Frame(range(10))
    R2 = DT[:, f[0]**f[0]]
    assert_equals(R2, dt.Frame([i**i for i in range(10)]/dt.float64))


def test_pow_integer_negative():
    # Raising to negative integer power produces a float64 column
    DT = dt.Frame(range(1, 10))
    RES = DT[:, f[0] ** -1]
    assert_equals(RES, dt.Frame([1/i for i in range(1, 10)]))


def test_pow_float_columns():
    DT = dt.Frame(x=[3, .14, 15.92, 6, 23.53], y=range(5))
    RES = DT[:, f.x**f.y]
    assert_equals(RES, dt.Frame([1, .14, 15.92**2, 216, 23.53**4]))


def test_pow_float_column_scalar():
    src = [0, 3, .14, 15.92, 6, 23.53]
    DT = dt.Frame(x=src)
    RES = DT[:, [f.x**2, f.x**-1]]
    assert_equals(RES,
                  dt.Frame([
                    [x**2 for x in src],
                    [math.inf if x == 0 else 1/x for x in src]
                  ]))


@pytest.mark.parametrize("seed", [random.getrandbits(63)])
def test_pow_booleans_integers_floats_random(seed):
    random.seed(seed)
    n = 1000
    src1 = [random.randint(-100, 100) for _ in range(n)]
    src2 = [random.randint(0, 1) for _ in range(n)]
    src3 = [random.random() * 10 - 5 for _ in range(n)]

    DT = dt.Frame(x=src1, y=src2/dt.bool8, z=src3)
    RES = DT[:, [f.x ** f.y, f.y ** math.abs(f.x),
                 f.y ** math.abs(f.z), f.z ** f.y,
                 f.z ** math.abs(f.x), math.abs(f.x) ** math.abs(f.z)]]
    EXP = dt.Frame([
            [src1[i] ** src2[i] for i in range(n)]/dt.float64,
            [src2[i] ** abs(src1[i]) for i in range(n)]/dt.float64,
            [src2[i] ** abs(src3[i]) for i in range(n)],
            [src3[i] ** src2[i] for i in range(n)],
            [src3[i] ** abs(src1[i]) for i in range(n)],
            [abs(src1[i]) ** abs(src3[i]) for i in range(n)],
          ])
    assert_equals(RES, EXP)
