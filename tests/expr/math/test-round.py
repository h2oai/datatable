#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2020 H2O.ai
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
from datatable import dt, f
from datatable.math import round as dtround
from tests import assert_equals


def test_round_noargs():
    msg = r"Function datatable.round\(\) requires exactly 1 positional " \
          r"argument, but none were given"
    with pytest.raises(TypeError, match=msg):
        assert dtround()


def test_round_ndigits_expr():
    msg = r"Argument ndigits in function datatable.round\(\) should be an " \
          r"integer, instead got <class 'datatable.FExpr'>"
    with pytest.raises(TypeError, match=msg):
        assert dtround(f.A, ndigits=f.B)


def test_round_expr_instance():
    assert isinstance(dtround(f.A), dt.FExpr)
    assert isinstance(dtround(2.5), dt.FExpr)
    assert isinstance(dtround(2.5, ndigits=1), dt.FExpr)


def test_round_expr_str():
    assert str(dtround(1)) == "FExpr<round(1)>"
    assert str(dtround(f.A)) == "FExpr<round(f.A)>"
    assert str(dtround(f.A, ndigits=None)) == "FExpr<round(f.A)>"
    assert str(dtround(f.A, ndigits=0)) == "FExpr<round(f.A, ndigits=0)>"
    assert str(dtround(f[:], ndigits=-5)) == "FExpr<round(f[:], ndigits=-5)>"



#-------------------------------------------------------------------------------
# SType::BOOL
#-------------------------------------------------------------------------------

def test_round_bool_positive_ndigits():
    DT = dt.Frame(A=[True, False, None])
    assert_equals(DT[:, dtround(f.A)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=0)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=1)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=2)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=3)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=5)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=9)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=19)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=999999)], DT)


def test_round_bool_negative_ndigits():
    DT = dt.Frame(A=[True, False, None])
    D0 = dt.Frame(A=[False, False, False])
    assert_equals(DT[:, dtround(f.A, ndigits=-1)], D0)
    assert_equals(DT[:, dtround(f.A, ndigits=-2)], D0)
    assert_equals(DT[:, dtround(f.A, ndigits=-3)], D0)
    assert_equals(DT[:, dtround(f.A, ndigits=-7)], D0)
    assert_equals(DT[:, dtround(f.A, ndigits=-19)], D0)
    assert_equals(DT[:, dtround(f.A, ndigits=-1234567)], D0)



#-------------------------------------------------------------------------------
# SType::INT8
#-------------------------------------------------------------------------------

def test_round_int8_positive_ndigits():
    DT = dt.Frame(A=[None] + list(range(-127, 128)), stype=dt.int8)
    assert_equals(DT[:, dtround(f.A)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=0)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=1)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=2)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=3)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=5)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=9)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=17)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=987654321)], DT)


@pytest.mark.parametrize('nd', [-1, -2])
def test_round_int8_negative1(nd):
    DT = dt.Frame(A=[None] + list(range(-127, 128)), stype=dt.int8)
    RES = DT[:, dtround(f.A, ndigits=nd)]
    EXP = dt.Frame(A=[None] + [round(x, nd) for x in range(-127, 128)],
                   stype=dt.int8)
    assert_equals(RES, EXP)
