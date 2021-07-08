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
import math
import pytest
from datatable import dt, f
from tests import assert_equals


#-------------------------------------------------------------------------------
# Create bool column
#-------------------------------------------------------------------------------

def test_bool_create_column_auto():
    DT = dt.Frame([True, False, True, None, False, False])
    assert DT.type == dt.Type.bool8
    assert DT.to_list() == [[True, False, True, None, False, False]]


def test_create_column01():
    # Creating column from 0s and 1s does not produce bool8
    DT = dt.Frame([0, 1, 0, 0, 1])
    assert DT.type != dt.Type.bool8


def test_bool_create_column_forced():
    # When the column is forced into type bool, we effectively apply
    # bool(x) to each value in the list,
    # with the only exception being `math.nan`, which is converted into None.
    DT = dt.Frame([True, False, None, 0, 1, 12, 0.0, -0.0, "", "hi", math.nan],
                  type=bool)
    assert DT.type == dt.Type.bool8
    assert DT.to_list() == [[True, False, None, False, True, True, False, False,
                             False, True, None]]


def test_bool_create_force_from_exceptional():
    # Here we check what happens if the objects that we are trying to
    # cast into bools throw an exception. An FExpr is just that kind of
    # object.
    # Two things are expected to happen here: the exception-raising objects
    # become `None`s, and exception objects are discarded.
    with pytest.raises(TypeError):
        assert dt.f.A

    DT = dt.Frame([None, dt.f.A, math.nan, dt.f[:], TypeError], type=bool)
    assert DT.to_list() == [[None, None, None, None, True]]




#-------------------------------------------------------------------------------
# Cast various types into bool
#-------------------------------------------------------------------------------

def test_cast_bool_to_bool():
    DT = dt.Frame(B=[None, True, False, None, True, True, False, False])
    assert DT.stypes == (dt.bool8,)
    RES = DT[:, {"B": dt.bool8(f.B)}]
    assert_equals(DT, RES)


@pytest.mark.parametrize("source_stype", [dt.int8, dt.int16, dt.int32, dt.int64])
def test_cast_int_to_bool(source_stype):
    DT = dt.Frame(N=[-11, -1, None, 0, 1, 11, 127], stype=source_stype)
    assert DT.type == dt.Type(source_stype)
    RES = DT[:, dt.bool8(f.N)]
    assert_equals(RES, dt.Frame(N=[True, True, None, False, True, True, True]))


def test_cast_m127_to_bool():
    DT = dt.Frame([-128, -127, 0, 127, 128, 256])
    RES = DT[:, dt.bool8(f[0])]
    assert_equals(RES, dt.Frame([True, True, False, True, True, True]))


@pytest.mark.parametrize("source_type", [dt.float32, dt.float64])
def test_cast_float_to_bool(source_type):
    DT = dt.Frame(G=[-math.inf, math.inf, math.nan, 0.0, 13.4, 1.0, -1.0, -128],
                  type=source_type)
    assert DT.type == dt.Type(source_type)
    RES = DT[:, dt.bool8(f.G)]
    assert_equals(RES, dt.Frame(G=[True, True, None, False, True, True, True, True]))


def test_cast_str_to_bool():
    DT = dt.Frame(['True', "False", "bah", None, "true"])
    DT[0] = bool
    assert_equals(DT, dt.Frame([1, 0, None, None, None] / dt.bool8))


def test_cast_obj_to_bool():
    DT = dt.Frame([True, False, None, 1, 3.2, "True"] / dt.obj64)
    DT[0] = bool
    assert_equals(DT, dt.Frame([True, False, None, True, True, True]))


