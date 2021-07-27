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
from datatable.internal import frame_integrity_check
from tests import assert_equals, list_equals

@pytest.mark.parametrize('src', ['float32', 'float64'])
def test_query_methods(src):
    tfloat = dt.Type(src)
    assert not tfloat.is_array
    assert not tfloat.is_boolean
    assert not tfloat.is_compound
    assert     tfloat.is_float
    assert not tfloat.is_integer
    assert     tfloat.is_numeric
    assert not tfloat.is_object
    assert not tfloat.is_string
    assert not tfloat.is_temporal
    assert not tfloat.is_void



#-------------------------------------------------------------------------------
# Create float32 columns
#-------------------------------------------------------------------------------

def test_float32_autodetect_from_nplist(np):
    f32 = np.float32
    src = [f32(2.5), None, f32(3.25), f32(2.125), f32(None)]
    DT = dt.Frame(src)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.float32
    assert DT.to_list()[0] == [2.5, None, 3.25, 2.125, None]


def test_float32_force():
    DT = dt.Frame([1, 5, 2.6, "7.7777", -1.2e+50, 1.3e-50],
                  type=dt.Type.float32)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.float32
    assert DT.shape == (6, 1)
    # Apparently, float "inf" converts into double "inf" when cast. Good!
    assert list_equals(DT.to_list(), [[1, 5, 2.6, 7.7777, -math.inf, 0]])




#-------------------------------------------------------------------------------
# Create float64 columns
#-------------------------------------------------------------------------------

def test_float64_autodetect_1():
    # "Normal" case: the source is a list of floats
    src = [4.5, 17.9384, -34.222, None, 4.5e-76]
    DT = dt.Frame(src)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.float64
    assert DT.to_list()[0] == src


def test_float64_autodetect_2():
    # First, the list looks like it might be int8, but then converts to float64
    src = [None, None, 0, 1, 0, 0, 1.0]
    DT = dt.Frame(src)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.float64
    assert DT.to_list()[0] == src


def test_float64_autodetect_3():
    # First, the list looks like int32, but then becomes float64
    src = [5, 12, 5.2, None, 11, 0.998, -1]
    DT = dt.Frame(src)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.float64
    assert DT.to_list()[0] == src


def test_float64_autodetect_4():
    # First, the list looks like int64, but then becomes float64
    src = [50000000000000, None, 113401871340, 0.998, -1]
    DT = dt.Frame(src)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.float64
    assert DT.to_list()[0] == src


def test_float64_autodetect_5():
    # The list contains only integers, but they are too large to fit
    # even into int64
    DT = dt.Frame([10**n for n in range(30)])
    frame_integrity_check(DT)
    assert DT.type == dt.Type.float64
    # Note: writing 10.0**n causes imprecise results on windows
    assert DT.to_list()[0] == [float("1e%d" % n) for n in range(30)]


def test_float64_overflow():
    # If the input contains integers that are too large even for float64,
    # they are transformed into +inf
    DT = dt.Frame([1, 1000000, 10**500, -10**380])
    frame_integrity_check(DT)
    assert DT.type == dt.Type.float64
    assert DT.to_list()[0] == [1.0, 1e6, dt.math.inf, -dt.math.inf]


def test_float64_autodetect_from_nplist(np):
    f64 = np.float64
    src = [f64(2.4), None, f64(3.7), f64(2.11), f64(None)]
    DT = dt.Frame(src)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.float64
    assert DT.to_list()[0] == [2.4, None, 3.7, 2.11, None]


def test_create_from_numpy_floats_mixed(np):
    msg = "Cannot create column: element at index 3 is of type "\
          "<class 'numpy.float64'>, whereas previous elements were np.float32"
    with pytest.raises(TypeError, match=msg):
        DT = dt.Frame([np.float32(4), np.float16(3.5), None, np.float64(9.33)])
