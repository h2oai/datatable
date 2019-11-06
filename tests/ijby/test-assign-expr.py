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
#
# Test assigning expressions
#
#-------------------------------------------------------------------------------
import math
import pytest
from datatable import f, g, dt, by, join
from datatable.internal import frame_integrity_check
from tests import assert_equals



#-------------------------------------------------------------------------------
# Assign to complete columns
#-------------------------------------------------------------------------------

def test_assign_change_stype():
    DT = dt.Frame(A=range(5))
    DT["A"] = dt.int64(f.A)
    DT["B"] = dt.str64(f.A)
    assert_equals(DT, dt.Frame(A=range(5), B=list("01234"),
                               stypes={"A": dt.int64, "B": dt.str64}))


def test_assign_expr_unary():
    DT = dt.Frame(A=range(5))
    DT["B"] = -f.A
    assert_equals(DT, dt.Frame(A=range(5), B=range(0, -5, -1)))


def test_assign_expr_binary():
    DT = dt.Frame(A=range(5))
    DT["B"] = f.A / 2
    assert_equals(DT, dt.Frame(A=range(5), B=[0, 0.5, 1.0, 1.5, 2.0]))


def test_assign_expr_overwrite():
    DT = dt.Frame(A=range(5))
    DT["A"] = f.A + 1
    assert_equals(DT, dt.Frame(A=range(1, 6)))


def test_assign_multiple_exprs():
    DT = dt.Frame(A=range(5))
    DT[:, ["B", "C"]] = [f.A * 2, f.A - 1]
    assert_equals(DT, dt.Frame(A=range(5), B=range(0, 10, 2), C=range(-1, 4)))


def test_assign_multiple_exprs_with_overwrite():
    DT = dt.Frame(A=range(5))
    DT[:, ["B", "A", "C"]] = [f.A * 2, f.A - 1, f.A + 1]
    assert_equals(DT, dt.Frame(A=range(-1, 4), B=range(0, 10, 2),
                               C=range(1, 6)))


def test_assign_nonexisting_column():
    # See #1983: if column `B` is created at a wrong moment in the evaluation
    # sequence, this may seg.fault
    DT = dt.Frame(A=range(5))
    with pytest.raises(ValueError, match="Column `B` does not exist in the "
                                         "Frame"):
        DT[:, "B"] = f.B + 1
    frame_integrity_check(DT)




#-------------------------------------------------------------------------------
# Assign to sub-frames
#-------------------------------------------------------------------------------

def test_assign_expr_partial():
    DT = dt.Frame(A=range(5))
    DT[f.A < 3, f.A] = f.A + 5
    assert_equals(DT, dt.Frame(A=[5, 6, 7, 3, 4]))


def test_assign_expr_partial_with_type_change():
    DT = dt.Frame(A=range(5))
    with pytest.raises(TypeError, match="Cannot assign float value to "
                                        "column `A` of type int32"):
        DT[f.A > 3, f.A] = 1 / f.A




#-------------------------------------------------------------------------------
# Assign complex combos
#-------------------------------------------------------------------------------

def test_assign_with_groupby():
    DT = dt.Frame(A=range(5), B=[1, 1, 2, 2, 2])
    DT[:, "C", by(f.B)] = dt.mean(f.A)
    assert_equals(DT, dt.Frame(A=range(5), B=[1, 1, 2, 2, 2],
                               C=[0.5, 0.5, 3.0, 3.0, 3.0]))


def test_assign_with_groupby2():
    DT = dt.Frame(A=range(5), B=[1, 1, 2, 2, 2])
    DT[:, "C", by(f.B)] = f.A - dt.mean(f.A)
    assert_equals(DT, dt.Frame(A=range(5), B=[1, 1, 2, 2, 2],
                               C=[-0.5, 0.5, -1.0, 0, 1.0]))


def test_assign_from_joined_frame():
    DT = dt.Frame(A=range(5))
    JDT = dt.Frame(A=[1, 2, 3], B=['a', 'b', 'c'])
    JDT.key = 'A'
    DT[:, "Z", join(JDT)] = g.B
    assert_equals(DT, dt.Frame(A=range(5), Z=[None, 'a', 'b', 'c', None]))


def test_assign_compute_with_joined():
    DT = dt.Frame(profit=[23.4, 75.1, 92.3, 0.1],
                  month=['Jan', 'Apr', 'Aug', 'Feb'])
    JDT = dt.Frame(month=['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul',
                          'Aug', 'Sep', 'Oct', 'Nov', 'Dec'],
                   ndays=[31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31])
    JDT.key = 'month'
    DT[:, "daily_profit", join(JDT)] = f.profit / g.ndays
    assert_equals(DT, dt.Frame(profit=[23.4, 75.1, 92.3, 0.1],
                               month=['Jan', 'Apr', 'Aug', 'Feb'],
                               daily_profit=[23.4/31, 75.1/30, 92.3/31, .1/28]))
