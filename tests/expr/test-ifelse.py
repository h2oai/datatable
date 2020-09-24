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
from datatable import dt, f, ifelse, by
from tests import assert_equals


def test_ifelse_bad_signature():
    DT = dt.Frame(A=range(10))
    msg = r"Function datatable\.ifelse\(\) requires at least 3 arguments"
    with pytest.raises(TypeError, match=msg):
        DT[:, ifelse()]
    with pytest.raises(TypeError, match=msg):
        DT[:, ifelse(f.A > 0)]
    with pytest.raises(TypeError, match=msg):
        DT[:, ifelse(f.A > 0, f.A)]

    msg = r"Missing the required default argument in function " \
          r"datatable\.ifelse\(\)"
    with pytest.raises(TypeError, match=msg):
        DT[:, ifelse(f.A > 0, f.A, f.A, f.A)]


def test_ifelse_wrong_type():
    DT = dt.Frame(A=range(10))
    DT["B"] = dt.str32(f.A)
    msg = r"The condition1 argument in ifelse\(\) must be a boolean column"
    with pytest.raises(TypeError, match=msg):
        DT[:, ifelse(f.A, f.A, f.A)]
    with pytest.raises(TypeError, match=msg):
        DT[:, ifelse(f.B, f.A, f.A)]


def test_ifelse_columnsets():
    DT = dt.Frame(A=range(10), B=[7]*10, C=list('abcdefghij'))
    msg = r"The condition1 argument in ifelse\(\) cannot be a multi-column " \
          r"expression"
    with pytest.raises(TypeError, match=msg):
        DT[:, ifelse(f[:], 0, 1)]
    with pytest.raises(TypeError, match=msg):
        DT[:, ifelse(f[int] > 3, 3, f[int])]

    msg = r"The value1 argument in ifelse\(\) cannot be a multi-column " \
          r"expression"
    with pytest.raises(TypeError, match=msg):
        DT[:, ifelse(f.A > 3, f[:], f.A)]

    msg = r"The value2 argument in ifelse\(\) cannot be a multi-column " \
          r"expression"
    with pytest.raises(TypeError, match=msg):
        DT[:, ifelse(f.A > 3, f.A, f[:])]



def test_ifelse_simple():
    DT = dt.Frame(A=range(10))
    DT["B"] = ifelse(f.A > 5, f.A - 5, f.A + 5)
    assert_equals(DT, dt.Frame(A=range(10), B=[5, 6, 7, 8, 9, 10, 1, 2, 3, 4]))


def test_ifelse_with_scalars():
    DT = dt.Frame(A=range(10))
    RES = DT[:, ifelse(f.A % 2 == 0, "even", "odd")]
    assert_equals(RES, dt.Frame(["even", "odd"] * 5))


def test_example():
    DT = dt.Frame(domestic_income=[4500, 2500, 1500, 4000],
                  internationaL_income=[2000,5000,1000,4500])
    DT["profit_loss"] = ifelse(f.domestic_income > f.internationaL_income,
                               "profit", "loss")
    assert_equals(DT, dt.Frame(
                        domestic_income=[4500, 2500, 1500, 4000],
                        internationaL_income=[2000,5000,1000,4500],
                        profit_loss=["profit", "loss", "profit", "loss"]
                        ))


def test_different_stypes():
    DT = dt.Frame(A=[3], B=[7.1])
    RES = DT[:, ifelse(f.A > 0, f.A, f.B)]
    assert_equals(RES, dt.Frame([3.0]))


def test_condition_with_NAs():
    DT = dt.Frame(A=[True, False, None], B=[5, 7, 9])
    RES = DT[:, ifelse(f.A, f.B, -f.B)]
    assert_equals(RES, dt.Frame([5, -7, None]))


def test_ifelse_with_groupby():
    DT = dt.Frame(A=[2, 5, 2, 5, 2, 2], B=range(6))
    R1 = DT[:, ifelse(f.A == 2, dt.min(f.B), dt.max(f.B)), by(f.A)]
    R2 = DT[:, ifelse(f.A == 2, f.B, dt.max(f.B)), by(f.A)]
    R3 = DT[:, ifelse(f.A == 2, dt.min(f.B), f.B), by(f.A)]
    R4 = DT[:, ifelse(f.B > 2, dt.min(f.B), f.B), by(f.A)]
    assert_equals(R1, dt.Frame(A=[2, 5], C0=[0, 3]))
    assert_equals(R2, dt.Frame(A=[2, 2, 2, 2, 5, 5], C0=[0, 2, 4, 5, 3, 3]))
    assert_equals(R3, dt.Frame(A=[2, 2, 2, 2, 5, 5], C0=[0, 0, 0, 0, 1, 3]))
    assert_equals(R4, dt.Frame(A=[2, 2, 2, 2, 5, 5], C0=[0, 2, 0, 0, 1, 1]))



#-------------------------------------------------------------------------------
# Multi-expression ifelse
#-------------------------------------------------------------------------------

def test_ifelse_multi():
    DT = dt.Frame(A=['fox', 'cat', 'jay', 'cow'])
    RES = DT[:, ifelse(f.A == 'fox', 3,
                       f.A == 'dog', 7,
                       f.A == 'cow', 2, -1)]
    assert_equals(RES, dt.Frame([3, -1, -1, 2]))


def test_ifelse_multi_different_grouplevels():
    DT = dt.Frame(A=[1, 2, 3, 4, 5, 6])
    RES = DT[:, ifelse(f.A <= 2, dt.min(f.A),
                       f.A >= 5, dt.max(f.A),
                       f.A == 0, 1000000,
                       f.A)]
    assert_equals(RES, dt.Frame([1, 1, 3, 4, 6, 6]))


def test_countries():
    # inspired by @knapply's example
    countries = dt.Frame(
        name=["Czech Republic", "Czecho-Slovakia", "Mexico", "Czech Republic",
              "Canada", "Czechoslovakia", "USA", "Britain"],
        year=[1918] + list(range(1990, 1997))
    )
    def is_czech_name(x):
        return ((x == "Czechoslovak Republic") |
                (x == "Czechoslovakia") |
                (x == "Czech Republic") |
                (x == "Czecho-Slovakia"))

    name, year = countries.export_names()
    RES = countries[:, {"historic_name": ifelse(
              is_czech_name(name) & (year <= 1938), "Czechoslovak Republic",
              is_czech_name(name) & (year <= 1992), "Czechoslovakia",
              is_czech_name(name) & (year >= 1993), "Czech Republic",
              (name == "USA"),                      "United States of America",
              (name == "Britain"),                  "United Kingdom",
              name
            )}]
    assert_equals(RES,
        dt.Frame(historic_name=["Czechoslovak Republic", "Czechoslovakia",
            "Mexico", "Czechoslovakia", "Canada", "Czech Republic",
            "United States of America", "United Kingdom"]))

