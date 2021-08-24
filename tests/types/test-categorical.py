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
from datatable import dt, f
from tests import assert_equals


#-------------------------------------------------------------------------------
# Type object
#-------------------------------------------------------------------------------

@pytest.mark.parametrize('t', [dt.Type.cat8,
                               dt.Type.cat16,
                               dt.Type.cat32])
def test_type_categorical_wrong(t):
    msg = r"Categories are not allowed to be of a categorical type"
    with pytest.raises(TypeError, match=msg):
        t(t(dt.str64))


def test_type_categorical_repr():
    assert repr(dt.Type.cat8(int)) == "Type.cat8(int64)"
    assert repr(dt.Type.cat16(dt.float32)) == "Type.cat16(float32)"
    assert repr(dt.Type.cat32(str)) == "Type.cat32(str32)"


def test_type_categorical_name():
    assert dt.Type.cat8(int).name == "cat8(int64)"
    assert dt.Type.cat16(dt.int8).name == "cat16(int8)"
    assert dt.Type.cat32(None).name == "cat32(void)"


@pytest.mark.parametrize('t', [dt.Type.cat8(int),
                               dt.Type.cat16(str),
                               dt.Type.cat32(float)])
def test_type_categorical_properties(t):
    assert t.min is None
    assert t.max is None


def test_type_categorical_equality():
    t1 = dt.Type.cat8(int)
    t2 = dt.Type.cat32(int)
    assert t1 != t2
    assert t1 == dt.Type.cat8(dt.int64)
    assert t1 != dt.Type.cat8(dt.int32)
    assert t1 != dt.Type.cat8(dt.Type.void)
    assert t1 != dt.Type.int64
    assert t2 == dt.Type.cat32(int)
    assert t2 != dt.Type.cat8(int)
    assert t2 != dt.Type.cat32(str)
    assert dt.Type.cat8(int) != \
           dt.Type.cat8(float)


def test_type_categorical_hashable():
    store = {dt.Type.cat8(str): 1, dt.Type.cat32('float32'): 2,
             dt.Type.cat16(str): 3}
    assert dt.Type.cat8(str) in store
    assert dt.Type.cat32(dt.float32) in store
    assert dt.Type.cat16(str) in store
    assert store[dt.Type.cat8("str")] == 1
    assert store[dt.Type.cat32('float32')] == 2
    assert store[dt.Type.cat16(str)] == 3
    assert dt.Type.cat8(int) not in store
    assert dt.Type.cat32(int) not in store
    assert dt.Type.cat32(float) not in store


@pytest.mark.parametrize('t', [dt.Type.cat8(int),
                               dt.Type.cat16(str),
                               dt.Type.cat32(float)])
def test_type_categorical_query_methods(t):
    assert     t.is_categorical
    assert not t.is_boolean
    assert     t.is_compound
    assert not t.is_float
    assert not t.is_integer
    assert not t.is_numeric
    assert not t.is_object
    assert not t.is_string
    assert not t.is_temporal
    assert not t.is_void

