#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2019 H2O.ai
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
from datatable import f, dt
from datatable.internal import frame_integrity_check
from tests import assert_equals

stypes_int = dt.ltype.int.stypes
stypes_float = dt.ltype.real.stypes
stypes_str = dt.ltype.str.stypes



#-------------------------------------------------------------------------------
# Assign boolean values
#-------------------------------------------------------------------------------

def test_assign_boolean():
    DT = dt.Frame(A=[None])
    DT[:, "A"] = True
    assert_equals(DT, dt.Frame(A=[True]))


def test_assign_boolean2():
    DT = dt.Frame(A=[True, False, True])
    DT[:, "A"] = False
    assert_equals(DT, dt.Frame(A=[False] * 3))


@pytest.mark.parametrize("stype", stypes_int + stypes_float + stypes_str)
def test_assign_boolean_to_wrong_type(stype):
    DT = dt.Frame(A=[5], stype=stype)
    assert DT.stype == stype
    with pytest.raises(TypeError, match="Cannot assign boolean value to column "
                                        "`A` of type " + stype.name):
        DT[:, "A"] = False


def test_assign_boolean_partial():
    DT = dt.Frame(A=range(5))
    DT[2, "B"] = False
    assert_equals(DT, dt.Frame(A=range(5), B=[None, None, False, None, None]))




#-------------------------------------------------------------------------------
# Assign integer values
#-------------------------------------------------------------------------------

def test_assign_integer_out_of_range():
    DT = dt.Frame(A=[1, 2, 3], stype=dt.int8)
    assert DT.stype == dt.int8
    DT[:, "A"] = 5000000
    assert_equals(DT, dt.Frame(A=[5000000] * 3, stype=dt.int32))


def test_assign_integer_out_of_range_to_subset():
    DT = dt.Frame(A=range(10), stype=dt.int8)
    assert DT.stype == dt.int8
    DT[:3, "A"] = 999
    assert_equals(DT, dt.Frame(A=[999, 999, 999, 3, 4, 5, 6, 7, 8, 9],
                               stype=dt.int16))
    DT[-1, "A"] = 10**10
    assert_equals(DT, dt.Frame(A=[999, 999, 999, 3, 4, 5, 6, 7, 8, 10**10],
                               stype=dt.int64))


def test_assign_int_overflow():
    # When integer overflows, it becomes a float64 value
    DT = dt.Frame(A=range(5), B=[0.0]*5)
    with pytest.raises(TypeError, match="Cannot assign float value"):
        DT[:, "A"] = 10**100
    DT[:, "B"] = 10**100
    assert_equals(DT["B"], dt.Frame(B=[1e100] * 5))


@pytest.mark.parametrize("stype", [dt.bool8] + stypes_str)
def test_assign_integer_to_wrong_type(stype):
    DT = dt.Frame(A=[5], stype=stype)
    assert DT.stype == stype
    with pytest.raises(TypeError, match="Cannot assign integer value to column "
                                        "`A` of type " + stype.name):
        DT[:, "A"] = 777




#-------------------------------------------------------------------------------
# Assign float values
#-------------------------------------------------------------------------------

def test_assign_to_newcolumn_subset():
    DT = dt.Frame(A=range(5))
    DT[[1, 4], "B"] = 3.7
    assert_equals(DT, dt.Frame(A=range(5), B=[None, 3.7, None, None, 3.7]))


def test_assign_float_upcast():
    # When float32 overflows, it is converted to float64
    DT = dt.Frame(A=[1.3, 2.7], stype=dt.float32)
    DT[:, "A"] = 1.5e+100
    assert_equals(DT, dt.Frame(A=[1.5e100, 1.5e100]))


def test_assign_to_float32_column():
    DT = dt.Frame(A=range(5), stype=dt.float32)
    DT[:, "A"] = 3.14159
    assert_equals(DT, dt.Frame(A=[3.14159] * 5, stype=dt.float32))


@pytest.mark.parametrize("stype", [dt.bool8] + stypes_int + stypes_str)
def test_assign_float_to_wrong_type(stype):
    DT = dt.Frame(A=[5], stype=stype)
    assert DT.stype == stype
    with pytest.raises(TypeError, match="Cannot assign float value to column "
                                        "`A` of type " + stype.name):
        DT[:, "A"] = 3.14159265



#-------------------------------------------------------------------------------
# Assign string values
#-------------------------------------------------------------------------------

def test_assign_string_to_str64():
    DT = dt.Frame(A=["wonder", None, "ful", None], stype=dt.str64)
    DT[:, "A"] = "beep"
    assert_equals(DT, dt.Frame(A=["beep"] * 4, stype=dt.str64))


@pytest.mark.parametrize("stype", [dt.bool8] + stypes_int + stypes_float)
def test_assign_string_to_wrong_type(stype):
    DT = dt.Frame(A=[5], stype=stype)
    assert DT.stype == stype
    with pytest.raises(TypeError, match="Cannot assign string value to column "
                                        "`A` of type " + stype.name):
        DT[:, "A"] = 'what?'


def test_assign_str_to_empty_frame():
    # See issue #2043, the assignment used to deadlock
    DT = dt.Frame(W=[], stype=dt.str32)
    DT[f.W == '', f.W] = 'yay!'
    assert_equals(DT, dt.Frame(W=[], stype=dt.str32))
