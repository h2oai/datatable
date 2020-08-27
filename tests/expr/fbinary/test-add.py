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
from datatable import dt, f
from tests import assert_equals


#-------------------------------------------------------------------------------
# Errors
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("type", [dt.obj64])
def test_add_error_wrong_one_type(type):
    DT = dt.Frame(A=[]/type, B=[]/dt.int32)
    type_str = str(type)[6:]
    msg = r"Operator \+ cannot be applied to columns of types %s and int32" % \
          (type_str,)
    with pytest.raises(TypeError, match=msg):
        DT[:, f.A + f.B]


@pytest.mark.parametrize("type", [dt.obj64])
def test_add_error_wrong_both_types(type):
    DT = dt.Frame(A=[]/type, B=[]/type)
    type_str = str(type)[6:]
    msg = r"Operator \+ cannot be applied to columns of types %s and %s" % \
          (type_str, type_str)
    with pytest.raises(TypeError, match=msg):
        DT[:, f.A + f.B]


#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

def test_add_stringify():
  assert str(f.A + 1) == "FExpr<f.A + 1>"
  assert str(3 + f.A) == "FExpr<3 + f.A>"
  assert str(f.A + f.B) == "FExpr<f.A + f.B>"
  assert str(f.A + f.B + f.C) == "FExpr<f.A + f.B + f.C>"
  assert str((f.A + f.B) + f.C) == "FExpr<f.A + f.B + f.C>"
  assert str(f.A + (f.B + f.C)) == "FExpr<f.A + (f.B + f.C)>"
  assert str(f.A + f[1]*f[2] + f.Z) == "FExpr<f.A + f[1] * f[2] + f.Z>"


def test_add_booleans():
    DT = dt.Frame(A=[True, True, True, False, False, False, None, None, None],
                  B=[True, False, None] * 3)
    assert_equals(DT[:, f.A + f.B],
                  dt.Frame([2, 1, None, 1, 0, None, None, None, None]))


def test_add_integers():
    DT = dt.Frame(A=[3, 12, 724, None], B=[0, -11, 2, 9])
    assert_equals(DT[:, f.A + f.B],
                  dt.Frame([3, 1, 726, None]))


def test_add_integers_with_upcast():
    # int8 + int8 = int32
    DT = dt.Frame(A=[17, 111]/dt.int8, B=[-17, 28]/dt.int8)
    assert_equals(DT[:, f.A + f.B],
                  dt.Frame([0, 139]/dt.int32))


def test_integer_add_scalar():
    DT = dt.Frame(i8=[77, 101, 127]/dt.int8,
                  i32=[14, None, 99]/dt.int32,
                  i64=[7923, -121, 903]/dt.int64)
    assert_equals(DT[:, f[:] + 1],
                  dt.Frame([[78, 102, 128]/dt.int32,
                            [15, None, 100]/dt.int32,
                            [7924, -120, 904]/dt.int64]))


def test_add_floats():
    DT = dt.Frame(A=[2.55, 6.14, -7.2e3, None],
                  B=[2.46, -3.14, 24011.1, 12e100])
    assert_equals(DT[:, f.A + f.B],
                  dt.Frame([5.01, 3.0, 24011.1 - 7200, None]))


def test_add_strings():
    DT = dt.Frame(A=["one", "two", "3", None, "FIVE!", "", ""],
                  B=[".", "..", None, "test", "yes", "", None])
    assert_equals(DT[:, f.A + f.B],
                  dt.Frame(["one.", "two..", None, None, "FIVE!yes", "", None]))


def test_add_strings_with_scalar():
    DT = dt.Frame(A=["one", "two", "3", None, "FIVE!", ""])
    assert_equals(DT[:, f.A + "?"],
                  dt.Frame(["one?", "two?", "3?", None, "FIVE!?", "?"]))
    assert_equals(DT[:, f.A + f.A + "!"],
                  dt.Frame(["oneone!", "twotwo!", "33!", None, "FIVE!FIVE!!", "!"]))
    assert_equals(DT[:, "<" + f.A + ">"],
                  dt.Frame(["<one>", "<two>", "<3>", None, "<FIVE!>", "<>"]))


def test_add_strings_with_integers():
    DT = dt.Frame(A=range(5), B=list('abcde'))
    assert_equals(DT[:, f.A + f.B],
                  dt.Frame(["0a", "1b", "2c", "3d", "4e"]))


def test_add_strings_with_booleans():
    DT = dt.Frame(A=["aye", 'nay', "", '/'], B=[True, False, True, False])
    assert_equals(DT[:, f.A + f.B],
                  dt.Frame(["ayeTrue", "nayFalse", "True", "/False"]))
