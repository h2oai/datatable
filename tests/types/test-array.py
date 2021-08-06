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

def test_type_array_repr():
    assert repr(dt.Type.arr32(int)) == "Type.arr32(int64)"
    assert repr(dt.Type.arr32(dt.float32)) == "Type.arr32(float32)"
    assert repr(dt.Type.arr64(str)) == "Type.arr64(str32)"
    assert repr(dt.Type.arr32(dt.Type.arr64(dt.str64))) == \
        "Type.arr32(arr64(str64))"


def test_type_array_name():
    assert dt.Type.arr32(int).name == "arr32(int64)"
    assert dt.Type.arr64(None).name == "arr64(void)"
    assert dt.Type.arr32(dt.int8).name == "arr32(int8)"
    assert dt.Type.arr32(dt.Type.arr64(dt.str64)).name == \
        "arr32(arr64(str64))"


def test_type_array_properties():
    t = dt.Type.arr32(bool)
    assert t.min is None
    assert t.max is None


def test_type_array_equality():
    t1 = dt.Type.arr32(int)
    t2 = dt.Type.arr64(int)
    assert t1 != t2
    assert t1 == dt.Type.arr32(dt.int64)
    assert t1 != dt.Type.arr32(dt.int32)
    assert t1 != dt.Type.arr32(dt.Type.void)
    assert t1 != dt.Type.int64
    assert t2 == dt.Type.arr64(int)
    assert t2 != dt.Type.arr32(int)
    assert t2 != dt.Type.arr64(str)
    assert dt.Type.arr32(dt.Type.arr32(int)) != \
           dt.Type.arr32(dt.Type.arr32(float))


def test_type_array_hashable():
    store = {dt.Type.arr32(str): 1, dt.Type.arr64('float32'): 2,
             dt.Type.arr64(str): 3}
    assert dt.Type.arr32(str) in store
    assert dt.Type.arr64(dt.float32) in store
    assert dt.Type.arr64(str) in store
    assert store[dt.Type.arr32("str")] == 1
    assert store[dt.Type.arr64('float32')] == 2
    assert store[dt.Type.arr64(str)] == 3
    assert dt.Type.arr32(int) not in store
    assert dt.Type.arr64(int) not in store
    assert dt.Type.arr64(float) not in store
    assert dt.Type.arr32(dt.Type.arr32(bool)) not in store


@pytest.mark.parametrize('src', [0, 1])
def test_query_methods(src):
    tarr = dt.Type.arr32(int) if src else \
           dt.Type.arr64(str)
    assert     tarr.is_array
    assert not tarr.is_boolean
    assert     tarr.is_compound
    assert not tarr.is_float
    assert not tarr.is_integer
    assert not tarr.is_numeric
    assert not tarr.is_object
    assert not tarr.is_string
    assert not tarr.is_temporal
    assert not tarr.is_void




#-------------------------------------------------------------------------------
# Create from (any)
#-------------------------------------------------------------------------------

def test_create_from_arrow(pa):
    src = [[1, 3, 8, -14, 5], [2, 0], None, [4], [], [1, -1, 1]]
    arr = pa.array(src, type=pa.list_(pa.int32()))
    tbl = pa.Table.from_arrays([arr], names=["A"])
    DT = dt.Frame(tbl)
    assert DT.shape == (6, 1)
    assert DT.type == dt.Type.arr32(dt.Type.int32)
    assert DT.names == ("A",)
    assert DT.to_list() == [src]


def test_create_from_python1():
    src = [[1, 2, 3], [], [4, 5], [6], None, [7, 8, 10, -1]]
    DT = dt.Frame(A=src)
    assert DT.shape == (6, 1)
    assert DT.type == dt.Type.arr32(dt.Type.int32)
    assert DT.names == ("A",)
    assert DT.to_list() == [src]


def test_create_from_python2():
    src = [None, [1.5, 2, 3], [], None, [7, 8.99, 10, None, -1]]
    DT = dt.Frame(B=src)
    assert DT.shape == (5, 1)
    assert DT.type == dt.Type.arr32(dt.Type.float64)
    assert DT.names == ("B",)
    assert DT.to_list() == [src]


def test_create_from_python3():
    src = [[], [], [], []]
    DT = dt.Frame(D=src)
    assert DT.shape == (4, 1)
    assert DT.type == dt.Type.arr32(dt.Type.void)
    assert DT.names == ("D",)
    assert DT.to_list() == [src]


def test_create_from_python4():
    src = [None, [], [], [None] * 5, [], None, []]
    DT = dt.Frame(E=src)
    assert DT.shape == (7, 1)
    assert DT.type == dt.Type.arr32(dt.Type.void)
    assert DT.names == ("E",)
    assert DT.to_list() == [src]


def test_create_from_python5():
    src = [["a", "b", "c"], None, ["d"], ["efg", None]]
    DT = dt.Frame(F=src)
    assert DT.shape == (4, 1)
    assert DT.type == dt.Type.arr32(dt.Type.str32)
    assert DT.names == ("F",)
    assert DT.to_list() == [src]


def test_create_from_python_array_of_arrays():
    src = [
            [[1], [2, 3]],
            [],
            None,
            [[0, 4, 111], None, [15, -2]]
          ]
    DT = dt.Frame(N=src)
    assert DT.shape == (4, 1)
    assert DT.type == dt.Type.arr32(dt.Type.arr32(dt.Type.int32))
    assert DT.names == ("N",)
    assert DT.to_list() == [src]


def test_create_from_python_nested():
    src = [[[[[[]]]]]]
    DT = dt.Frame(Q=src)
    a = dt.Type.arr32
    assert DT.shape == (1, 1)
    assert DT.type == a(a(a(a(a(dt.Type.void)))))
    assert DT.to_list() == [src]


def test_create_from_python_array_incompatible_child_types():
    src = [[1, 2, 3], ["one"]]
    msg = "Cannot create column: element at index 3 is of type " \
          "<class 'str'>, whereas previous elements were int32"
    with pytest.raises(TypeError, match=msg):
        DT = dt.Frame(E=src)
