#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2020 H2O.ai
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
import copy
import datatable as dt
import pytest
from datatable import f
from datatable.internal import frame_integrity_check
from tests import assert_equals



def test_copy_frame():
    d0 = dt.Frame(A=range(5),
                  B=["dew", None, "strab", "g", None],
                  C=[1.0 - i * 0.2 for i in range(5)],
                  D=[True, False, None, False, True])
    assert sorted(d0.names) == list("ABCD")
    d1 = d0.copy()
    frame_integrity_check(d0)
    frame_integrity_check(d1)
    assert d0.names == d1.names
    assert d0.stypes == d1.stypes
    assert d0.to_list() == d1.to_list()
    d0[1, "A"] = 100
    assert d0.to_list() != d1.to_list()
    d0.names = ("w", "x", "y", "z")
    assert d1.names != d0.names


def test_copy_keyed_frame():
    d0 = dt.Frame(A=range(5), B=["alpha", "beta", "gamma", "delta", "epsilon"])
    d0.key = "A"
    d1 = d0.copy()
    d2 = dt.Frame(d0)
    frame_integrity_check(d1)
    frame_integrity_check(d2)
    assert d2.names == d1.names == d0.names
    assert d2.stypes == d1.stypes == d0.stypes
    assert d2.key == d1.key == d0.key
    assert d2.to_list() == d1.to_list() == d0.to_list()


def test_deep_copy():
    DT = dt.Frame(A=range(5), B=["alpha", "beta", "gamma", "delta", "epsilon"])
    DT = DT[:, ["A", "B"]]  # for py35
    D1 = DT.copy(deep=False)
    D2 = DT.copy(deep=True)
    assert_equals(DT, D1)
    assert_equals(DT, D2)
    dt_ptr = dt.internal.frame_column_data_r(DT, 1)
    assert dt.internal.frame_column_data_r(D1, 1).value == dt_ptr.value
    assert dt.internal.frame_column_data_r(D2, 1).value != dt_ptr.value


def test_copy_copy():
    DT = dt.Frame(A=range(5))
    D1 = copy.copy(DT)
    assert_equals(DT, D1)


def test_copy_deepcopy():
    DT = dt.Frame(A=[5.6, 4.4, 9.3, None])
    D1 = copy.deepcopy(DT)
    assert_equals(DT, D1)
    assert (dt.internal.frame_column_data_r(D1, 0).value !=
            dt.internal.frame_column_data_r(DT, 0).value)


def test_rename_after_copy():
    # Check that when copying a frame, the changes to memoized .py_names and
    # .py_inames do not get propagated to the original Frame.
    d0 = dt.Frame([[1], [2], [3]])
    assert d0.names == ("C0", "C1", "C2")
    d1 = d0.copy()
    assert d1.names == ("C0", "C1", "C2")
    d1.names = {"C1": "ha!"}
    assert d1.names == ("C0", "ha!", "C2")
    assert d0.names == ("C0", "C1", "C2")
    assert d1.colindex("ha!") == 1
    with pytest.raises(KeyError):
        d0.colindex("ha!")


def test_setkey_after_copy():
    # See issue #2095
    DT = dt.Frame([[3] * 5, range(5)], names=["id1", "id2"])
    assert DT.colindex("id1") == 0
    assert DT.colindex("id2") == 1
    X = DT.copy()
    X.key = "id2"
    assert DT.colindex("id1") == 0
    assert DT.colindex("id2") == 1
    frame_integrity_check(DT)


def test_issue2179(numpy):
    DT = dt.Frame(numpy.ma.array([False], mask=[True]), names=['A'])
    DT1 = copy.deepcopy(DT)
    DT2 = copy.deepcopy(DT)
    frame_integrity_check(DT1)
    frame_integrity_check(DT2)


def test_copy_source(tempfile):
    with open(tempfile, "w") as out:
        out.write("A\n3\n1\n4\n1\n5\n")
    DT = dt.fread(tempfile)
    assert DT.source == tempfile
    copy1 = DT.copy()
    copy2 = DT.copy(deep=True)
    assert copy1.source == tempfile
    assert copy2.source == tempfile
