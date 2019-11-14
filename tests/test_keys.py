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
import datatable as dt
import pytest
import random
from datatable.internal import frame_integrity_check
from tests import isview


def test_keys_simple():
    dt0 = dt.Frame([["Joe", "Mary", "Leslie", "Adam", "Alice"],
                    [1, 5, 15, 12, 8],
                    [3.6, 9.78, 2.01, -4.23, 5.3819]],
                   names=["name", "sex", "avg"])
    assert dt0.key == tuple()
    dt0.key = "name"
    frame_integrity_check(dt0)
    assert dt0.key == ("name",)
    assert dt0.shape == (5, 3)
    assert dt0.names == ("name", "sex", "avg")
    assert dt0.to_list() == [["Adam", "Alice", "Joe", "Leslie", "Mary"],
                             [12, 8, 1, 15, 5],
                             [-4.23, 5.3819, 3.6, 2.01, 9.78]]
    dt0.key = "sex"
    frame_integrity_check(dt0)
    assert not isview(dt0)
    assert dt0.key == ("sex",)
    assert dt0.shape == (5, 3)
    assert dt0.names == ("sex", "name", "avg")
    assert dt0.to_list() == [[1, 5, 8, 12, 15],
                             ["Joe", "Mary", "Alice", "Adam", "Leslie"],
                             [3.6, 9.78, 5.3819, -4.23, 2.01]]
    dt0.key = None
    assert dt0.key == tuple()



def test_no_cache():
    # Check that assigning a key properly disposes of potentially cached
    # types / names of the Frame
    dt0 = dt.Frame([[1.1] * 4, list("ABCD"), [3, 5, 2, 1]],
                   names=["A", "B", "C"])
    assert dt0.names == ("A", "B", "C")
    assert dt0.ltypes == (dt.ltype.real, dt.ltype.str, dt.ltype.int)
    assert dt0.stypes == (dt.float64, dt.str32, dt.int32)
    assert dt0.colindex("B") == 1
    frame_integrity_check(dt0)
    dt0.key = "C"
    assert dt0.names == ("C", "A", "B")
    assert dt0.ltypes == (dt.ltype.int, dt.ltype.real, dt.ltype.str)
    assert dt0.stypes == (dt.int32, dt.float64, dt.str32)
    assert dt0.colindex("B") == 2
    frame_integrity_check(dt0)



def test_multi_key():
    dt0 = dt.Frame(D=range(6), A=[3, 7, 5, 2, 2, 3], B=[1, 2, 2, 3, 4, 4])
    dt0.key = ["A", "B"]
    frame_integrity_check(dt0)
    assert dt0.key == ("A", "B")
    assert dt0.names == ("A", "B", "D")
    assert dt0.to_list() == [[2, 2, 3, 3, 5, 7],
                             [3, 4, 1, 4, 2, 2],
                             [3, 4, 0, 5, 2, 1]]



def test_key_invalid1():
    dt0 = dt.Frame(A=range(5), B=[3] * 5)
    with pytest.raises(TypeError) as e:
        dt0.key = 0
    assert ("Key should be a column name, or a list/tuple of column names"
            in str(e.value))
    with pytest.raises(TypeError) as e:
        dt0.key = ["A", None]
    assert ("Key should be a list/tuple of column names, instead element 1 "
            "was a <class 'NoneType'>" == str(e.value))



def test_key_invalid2():
    dt0 = dt.Frame([["Joe", "Mary", "Leslie", "Adam", "Alice"],
                    [7, 9, 2, 2, 7],
                    [3, 4, 5, 3, 4]],
                   names=["name", "A", "B"])
    with pytest.raises(ValueError) as e:
        dt0.key = "A"
    assert ("the values are not unique" in str(e.value))

    dt0.key = ["A", "B"]
    assert dt0.key == ("A", "B")
    assert dt0.names == ("A", "B", "name")
    assert dt0.to_list() == [[2, 2, 7, 7, 9],
                             [3, 5, 3, 4, 4],
                             ["Adam", "Leslie", "Joe", "Alice", "Mary"]]

    with pytest.raises(ValueError) as e:
        dt0.key = "B"
    assert ("the values are not unique" in str(e.value))
    # Check that the columns don't get reordered during a bad key assignment
    assert dt0.key == ("A", "B")
    assert dt0.names == ("A", "B", "name")



def test_key_duplicate():
    dt0 = dt.Frame(A=range(5))
    with pytest.raises(ValueError) as e:
        dt0.key = ("A", "A")
    assert ("Column `A` is specified multiple times within the key" ==
            str(e.value))


def test_set_empty_key():
    dt0 = dt.Frame([range(5), [None] * 5], names=["A", "B"])
    dt0.key = []
    assert dt0.key == tuple()
    dt0.key = "A"
    assert dt0.key == ("A",)
    dt0.key = []
    frame_integrity_check(dt0)
    assert dt0.key == tuple()
    assert dt0.names == ("A", "B")


def test_key_save(tempfile_jay):
    dt0 = dt.Frame(D=range(6), A=[3, 7, 5, 2, 2, 3], B=[1, 2, 2, 3, 4, 4])
    dt0.key = ["A", "B"]
    frame_integrity_check(dt0)
    dt0.to_jay(tempfile_jay)
    dt1 = dt.fread(tempfile_jay)
    assert dt1.key == ("A", "B")
    frame_integrity_check(dt1)


def test_key_after_group():
    n = 1000
    DT = dt.Frame(A=[random.choice("abcd") for _ in range(n)])
    tmp = DT[:, dt.count(), dt.by(0)]
    frame_integrity_check(tmp)
    tmp.key = "A"
    assert tmp.to_list()[0] == ["a", "b", "c", "d"]
    assert sum(tmp.to_list()[1]) == n


#-------------------------------------------------------------------------------
# Test if a key was dropped or kept after certain operations
#-------------------------------------------------------------------------------

def test_key_dropped_after_single_column_selector():
    DT = dt.Frame([range(100), list(range(50))*2, list(range(25))*4],
                  names = ["A", "B", "C"])
    DT.key = ["A", "B"]

    DT_A = DT["A"]
    DT_B = DT["B"]
    DT_C = DT["C"]
    frame_integrity_check(DT_A)
    frame_integrity_check(DT_B)
    frame_integrity_check(DT_C)
    assert not DT_A.key
    assert not DT_B.key
    assert not DT_C.key
    assert DT_A.names == ("A",)
    assert DT_B.names == ("B",)
    assert DT_C.names == ("C",)
    assert DT_A.to_list() == [list(range(100))]
    assert DT_B.to_list() == [list(range(50))*2]
    assert DT_C.to_list() == [list(range(25))*4]


def test_key_kept_after_single_column_selector():
    DT = dt.Frame([range(100), list(range(50))*2, list(range(25))*4],
                  names = ["A", "B", "C"])
    DT.key = ["A"]

    DT_A = DT["A"]
    DT_B = DT["B"]
    DT_C = DT["C"]

    frame_integrity_check(DT_A)
    frame_integrity_check(DT_B)
    frame_integrity_check(DT_C)
    assert DT_A.key == ("A",)
    assert not DT_B.key
    assert not DT_C.key
    assert DT_A.names == ("A",)
    assert DT_B.names == ("B",)
    assert DT_C.names == ("C",)
    assert DT_A.to_list() == [list(range(100))]
    assert DT_B.to_list() == [list(range(50))*2]
    assert DT_C.to_list() == [list(range(25))*4]
