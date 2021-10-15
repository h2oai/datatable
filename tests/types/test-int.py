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
from tests import assert_equals


@pytest.mark.parametrize('src', ['int8', 'int16', 'int32', 'int64'])
def test_query_methods(src):
    tint = dt.Type(src)
    assert not tint.is_array
    assert not tint.is_boolean
    assert not tint.is_compound
    assert not tint.is_float
    assert     tint.is_integer
    assert     tint.is_numeric
    assert not tint.is_object
    assert not tint.is_string
    assert not tint.is_temporal
    assert not tint.is_void



#-------------------------------------------------------------------------------
# Create int8 columns
#-------------------------------------------------------------------------------

def test_int8_autodetect_1():
    src = [0, 1, 0, None, 0]
    DT = dt.Frame(src)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.int8
    assert DT.to_list() == [src]


def test_int8_autodetect_2():
    src = [None, None, None, None, 1, 0, 0, 1, None, 1, 1]
    DT = dt.Frame(src)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.int8
    assert DT.to_list() == [src]


def test_int8_autodetect_3(np):
    i8 = np.int8
    src = [i8(3), i8(5), None, i8(1), i8(100), None, i8(-1)]
    DT = dt.Frame(src)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.int8
    assert DT.to_list() == [[3, 5, None, 1, 100, None, -1]]


def test_int8_mixed_1():
    msg = "Cannot create column: element at index 4 is of type <class 'bool'>" \
          ", whereas previous elements were int8"
    with pytest.raises(TypeError, match=msg):
        dt.Frame([0, 1, 1, 0, False, True])


def test_int8_forced():
    DT = dt.Frame([1, None, -1, 1000, 2.7, "123", "boo"], type='int8')
    frame_integrity_check(DT)
    assert DT.type == dt.Type.int8
    assert DT.shape == (7, 1)
    assert DT.to_list() == [[1, None, -1, -24, 2, 123, None]]




#-------------------------------------------------------------------------------
# Create int16 columns
#-------------------------------------------------------------------------------

def test_int16_autodetect_1(np):
    i16 = np.int16
    src = [None, i16(50), i16(2303), None, i16(-45)]
    DT = dt.Frame(src)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.int16
    assert DT.to_list() == [[None, 50, 2303, None, -45]]


def test_int16_autodetect_2(np):
    i16 = np.int16
    src = [i16(1), i16(0)]
    DT = dt.Frame(src)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.int16
    assert DT.to_list() == [[1, 0]]


def test_int16_autodetect_3(np):
    i16 = np.int16
    src = [i16(1), i16(0), i16(1000), None, i16(1), i16(0), None]
    DT = dt.Frame(src)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.int16
    assert DT.to_list() == [[1, 0, 1000, None, 1, 0, None]]


def test_int16_mixed_1(np):
    i16 = np.int16
    msg = "Cannot create column: element at index 2 is of type <class 'numpy.int8'>" \
          ", whereas previous elements were np.int16"
    with pytest.raises(TypeError, match=msg):
        dt.Frame([i16(99), i16(0), np.int8(0), None])


def test_int16_forced():
    DT = dt.Frame([1e50, 1000, None, "27", "?", True], type=dt.Type.int16)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.int16
    assert DT.shape == (6, 1)
    # int(1e50) = 2407412430484045 * 2**115, which is ≡0 (mod 2**16)
    assert DT.to_list() == [[0, 1000, None, 27, None, 1]]




#-------------------------------------------------------------------------------
# Create int32 columns
#-------------------------------------------------------------------------------

def test_int32_autodetect_1():
    DT = dt.Frame([0, 1, 2])
    frame_integrity_check(DT)
    assert DT.type == dt.Type.int32
    assert DT.to_list() == [[0, 1, 2]]


def test_int32_autodetect_2():
    src = [None, None, None, 111, 0, 1, 44, 9548, 428570247, -12, None]
    DT = dt.Frame(src)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.int32
    assert DT.to_list()[0] == src


def test_int32_autodetect_3(np):
    i32 = np.int32
    src = [None, i32(100), i32(1000), i32(-1)]
    DT = dt.Frame(src)
    assert_equals(DT, dt.Frame([None, 100, 1000, -1]))


def test_int32_mixed_1(np):
    i32 = np.int32
    msg = "Cannot create column: element at index 3 is of type " \
          "<class 'numpy.int16'>, whereas previous elements were np.int32"
    with pytest.raises(TypeError, match=msg):
        dt.Frame([i32(99), i32(0), None, np.int16(1)])


def test_int32_mixed_2(np):
    msg = "Cannot create column: element at index 5 is of type " \
          "<class 'numpy.int32'>, whereas previous elements were int32"
    with pytest.raises(TypeError, match=msg):
        dt.Frame([None, 1, 5, 14, 1000, np.int32(1), 999])




#-------------------------------------------------------------------------------
# Create int64 columns
#-------------------------------------------------------------------------------

def test_int64_autodetect_1():
    src = [None, 0, 1, 44, 9548, 145928450, 2245982454589145, 333, 2]
    DT = dt.Frame(src)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.int64
    assert DT.to_list()[0] == src


def test_int64_autodetect_3(np):
    i64 = np.int64
    src = [None, i64(1), i64(2486), i64(-1), None]
    DT = dt.Frame(src)
    assert_equals(DT, dt.Frame([None, 1, 2486, -1, None], type='int64'))
