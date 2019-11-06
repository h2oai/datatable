#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2019 H2O.ai
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
from datatable import f, g, dt, by, join, update
from datatable.internal import frame_integrity_check
from tests import assert_equals



def test_update_simple():
    DT = dt.Frame(A=range(5))
    DT[:, update(B=10)]
    assert_equals(DT, dt.Frame(A=range(5), B=[10]*5))


def test_update_existing_column():
    DT = dt.Frame(A=range(5))
    DT[:, update(A=f.A * 2)]
    assert_equals(DT, dt.Frame(A=range(0, 10, 2)))


def test_update_multiple_columns():
    DT = dt.Frame(A=range(5))
    DT[:, update(I8=dt.int8(f.A),
                 I16=dt.int16(f.A),
                 I64=dt.int64(f.A))]
    assert_equals(DT, dt.Frame([[0, 1, 2, 3, 4]] * 4,
                               names=["A", "I8", "I16", "I64"],
                               stypes=[dt.int32, dt.int8, dt.int16, dt.int64]))


def test_update_multiple_dependents():
    DT = dt.Frame(A=range(5))
    DT[:, update(B=f.A + 1, A=f.A + 2, D=f.A + 3)]
    assert_equals(DT, dt.Frame(A=range(2, 7), B=range(1, 6), D=range(3, 8)))


def test_update_mixed_dimensions():
    DT = dt.Frame(A=range(5))
    DT[:, update(B=f.A * 2, C=10)]
    assert_equals(DT, dt.Frame(A=range(5), B=range(0, 10, 2), C=[10]*5))


def test_update_mixed_2():
    DT = dt.Frame(A=range(5))
    DT[:, update(B=3, C=f.A)]
    assert_equals(DT, dt.Frame(A=range(5), B=[3]*5, C=range(5)))


def test_update_with_groupby():
    DT = dt.Frame(A=range(5), B=[1, 1, 2, 2, 2])
    DT[:, update(C=7, D=dt.mean(f.A), E=f.A+1), by(f.B)]
    assert_equals(DT, dt.Frame(A=range(5), B=[1, 1, 2, 2, 2], C=[7]*5,
                               D=[0.5, 0.5, 3.0, 3.0, 3.0], E=range(1, 6)))


def test_update_with_delete():
    DT = dt.Frame(A=range(5))
    with pytest.raises(ValueError, match=r"update\(\) clause cannot be used "
                                         r"with a delete expression"):
        del DT[:, update(B=0)]


def test_update_with_assign():
    DT = dt.Frame(A=range(5))
    with pytest.raises(ValueError, match=r"update\(\) clause cannot be used "
                                         r"with an assignment expression"):
        DT[:, update(B=0)] = None


def test_update_misplaced():
    DT = dt.Frame(A=range(5))
    with pytest.raises(TypeError, match="Column selector must be an integer "
                                        "or a string"):
        DT[update(B=0)]
    with pytest.raises(TypeError, match="Invalid item at position 2 in "
                                        r"DT\[i, j, \.\.\.\] call"):
        DT[:, :, update(B=0)]



