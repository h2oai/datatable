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
import random
from datatable import dt, f
from tests import assert_equals


def test_pow_stringify():
    assert str(f.A ** 10) == "FExpr<f.A ** 10>"
    assert str(f.A ** f.B) == "FExpr<f.A ** f.B>"
    assert str(f.A ** f.B ** f.C) == "FExpr<f.A ** (f.B ** f.C)>"
    assert str((f.A ** f.B) ** f.C) == "FExpr<(f.A ** f.B) ** f.C>"
    assert str(f.A ** (f.B ** f.C)) == "FExpr<f.A ** (f.B ** f.C)>"
    assert str((f.A + f.B) ** f.C) == "FExpr<(f.A + f.B) ** f.C>"
    assert str(f.A ** (f.B / f.C)) == "FExpr<f.A ** (f.B / f.C)>"
    assert str(f.A ** (f[1]*3) ** f.Z) == "FExpr<f.A ** ((f[1] * 3) ** f.Z)>"


def test_pow_integers():
    DT = dt.Frame(range(10))
    R1 = DT[:, f[0]**2]
    assert_equals(R1, dt.Frame([i**2 for i in range(10)]/dt.int32))


def test_pow_integers2():
    # Note: large integers may overflow producing an undefined result
    DT = dt.Frame(range(10))
    R2 = DT[:, f[0]**f[0]]
    assert_equals(R2, dt.Frame([i**i for i in range(10)]/dt.int32))


def test_pow_integer_negative():
    # Raising to negative integer power produces a float64 column
    DT = dt.Frame(range(1, 10))
    RES = DT[:, f[0] ** -1]
    assert_equals(RES, dt.Frame([1/i for i in range(1, 10)]))
