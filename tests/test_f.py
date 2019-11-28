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
import datatable as dt
from datatable import f
from tests import assert_equals, noop

@pytest.fixture()
def DT():
    return dt.Frame([
        [2, 7, 0, 0],
        [True, False, False, True],
        [1, 1, 1, 1],
        [0.1, 2, -4, 4.4],
        [None, None, None, None],
        [0, 0, 0, 0],
        ["1", "2", "hello", "world"],
    ],
    names=list("ABCDEFG"),
    stypes=[dt.int32, dt.bool8, dt.int64, dt.float32, dt.int16,
            dt.float64, dt.str32])



#-------------------------------------------------------------------------------
# Stringify
#-------------------------------------------------------------------------------

def test_f_col_selector_unbound():
    # Check that unbounded col-selectors can be stringified. The precise
    # representation may be modified in the future; however f-expressions
    # should not raise exceptions when printed.
    # See issues #1024 and #1241
    assert str(f.a) == "Expr:col('a'; 0)"
    assert str(f.abcdefghijkl) == "Expr:col('abcdefghijkl'; 0)"
    assert str(f.abcdefghijklm) == "Expr:col('abcdefghijklm'; 0)"
    assert str(f[0]) == "Expr:col(0; 0)"
    assert str(f[1000]) == "Expr:col(1000; 0)"
    assert str(f[-1]) == "Expr:col(-1; 0)"
    assert str(f[-999]) == "Expr:col(-999; 0)"
    assert str(f[""]) == "Expr:col(''; 0)"
    assert str(f["0"]) == "Expr:col('0'; 0)"
    assert str(f["A+B"]) == "Expr:col('A+B'; 0)"
    assert str(f["_A"]) == "Expr:col('_A'; 0)"
    assert str(f["_54"]) == "Expr:col('_54'; 0)"
    assert str(f._3_) == "Expr:col('_3_'; 0)"
    assert str(f.a_b_c) == "Expr:col('a_b_c'; 0)"
    assert str(f[" y "]) == "Expr:col(' y '; 0)"
    assert str(f["a b c"]) == "Expr:col('a b c'; 0)"


def test_f_col_selector_invalid():
    with pytest.raises(TypeError) as e:
        noop(f[2.5])
    assert str(e.value) == ("Column selector should be an integer, string, or "
                            "slice, not <class 'float'>")
    # Note: at some point we may start supporting all the expressions below:
    with pytest.raises(TypeError):
        noop(f[[7, 4]])
    with pytest.raises(TypeError):
        noop(f[("A", "B", "C")])
    with pytest.raises(TypeError):
        noop(f[lambda: 1])


def test_f_expressions():
    assert str(f.C1 < f.C2) == "Expr:lt(Expr:col('C1'; 0), Expr:col('C2'; 0); )"


def test_f_columnset():
    assert str(f[:]) == "Expr:col(slice(None, None, None); 0)"
    assert str(f[:7]) == "Expr:col(slice(None, 7, None); 0)"
    assert str(f[::-1]) == "Expr:col(slice(None, None, -1); 0)"
    assert str(f[bool]) == "Expr:col(<class 'bool'>; 0)"
    assert str(f[int]) == "Expr:col(<class 'int'>; 0)"
    assert str(f[float]) == "Expr:col(<class 'float'>; 0)"
    assert str(f[str]) == "Expr:col(<class 'str'>; 0)"
    assert str(f[object]) == "Expr:col(<class 'object'>; 0)"
    assert str(f[dt.int32]) == "Expr:col(stype.int32; 0)"
    assert str(f[dt.float64]) == "Expr:col(stype.float64; 0)"
    assert str(f[dt.ltype.int]) == "Expr:col(ltype.int; 0)"


def test_f_columnset_extend():
    assert str(f[:].extend(f.A)) == \
        "Expr:setplus(Expr:col(slice(None, None, None); 0), Expr:col('A'; 0); )"
    assert str(f[int].extend(f[str])) == \
        "Expr:setplus(Expr:col(<class 'int'>; 0), Expr:col(<class 'str'>; 0); )"


def test_f_columnset_remove():
    assert str(f[:].remove(f.A)) == \
        "Expr:setminus(Expr:col(slice(None, None, None); 0), Expr:col('A'; 0); )"
    assert str(f[int].remove(f[0])) == \
        "Expr:setminus(Expr:col(<class 'int'>; 0), Expr:col(0; 0); )"



#-------------------------------------------------------------------------------
# Select individual columns
#-------------------------------------------------------------------------------

def test_f_int(DT):
    assert_equals(DT[:, f[3]], DT[:, 3])
    assert_equals(DT[:, f[-1]], DT[:, 6])
    assert_equals(DT[f[0] > 0, f[-1]], dt.Frame(G=["1", "2"]))

    with pytest.raises(ValueError, match="Column index `10` is invalid for a "
                                         "Frame with 7 columns"):
        assert DT[:, f[10]]


def test_f_str(DT):
    assert_equals(DT[:, "B"], DT[:, 1])
    assert_equals(DT[:, f.B], DT[:, 1])
    assert_equals(DT[:, f["B"]], DT[:, 1])
    for i, name in enumerate(DT.names):
        assert_equals(DT[:, f[name]], DT[:, i])

    with pytest.raises(ValueError) as e:
        noop(DT[:, f["d"]])
    assert ("Column `d` does not exist in the Frame; "
            "did you mean `D`, `A` or `B`?" == str(e.value))




#-------------------------------------------------------------------------------
# Select columnsets
#-------------------------------------------------------------------------------

def test_f_columnset(DT):
    assert_equals(DT[:, f[:]], DT)
    assert_equals(DT[:, f[::-1]], DT[:, ::-1])
    assert_equals(DT[:, f[:4]], DT[:, :4])
    assert_equals(DT[:, f[3:4]], DT[:, 3:4])
    assert_equals(DT[:, f["B":"E"]], DT[:, 1:5])
    assert_equals(DT[:, f[bool]], DT[:, 1])
    assert_equals(DT[:, f[int]], DT[:, [0, 2, 4]])
    assert_equals(DT[:, f[float]], DT[:, [3, 5]])
    assert_equals(DT[:, f[str]], DT[:, 6])
    assert_equals(DT[:, f[dt.str32]], DT[:, 6])
    assert_equals(DT[:, f[dt.str64]], DT[:, []])
    assert_equals(DT[:, f[None]], DT[:, []])


def test_f_columnset_stypes(DT):
    for st in dt.stype:
        assert_equals(DT[:, f[st]],
                      DT[:, [i for i in range(DT.ncols)
                             if DT.stypes[i] == st]])


def test_f_columnset_ltypes(DT):
    for lt in dt.ltype:
        assert_equals(DT[:, f[lt]],
                      DT[:, [i for i in range(DT.ncols)
                             if DT.ltypes[i] == lt]])

def test_columnset_sum(DT):
    assert_equals(DT[:, f[int].extend(f[float])], DT[:, [int, float]])
    assert_equals(DT[:, f[:3].extend(f[-3:])], DT[:, [0, 1, 2, -3, -2, -1]])
    assert_equals(DT[:, f.A.extend(f.B)], DT[:, ['A', 'B']])
    assert_equals(DT[:, f[:].extend({"extra": f.A + f.C})],
                  dt.cbind(DT, DT[:, {"extra": f.A + f.C}]))


def test_columnset_diff(DT):
    assert_equals(DT[:, f[:].remove(f[3])], DT[:, [0, 1, 2, 4, 5, 6]])
    assert_equals(DT[:, f[:].remove(f[2:-2])], DT[:, [0, 1, 5, 6]])
    assert_equals(DT[:, f[:5].remove(f[int])], DT[:, [1, 3]])
    assert_equals(DT[:, f[:].remove(f.Z)], DT)
    assert_equals(DT[:, f[:].remove([f.Z, f.Y])], DT)
    assert_equals(DT[:, f[:].remove(f[100:])], DT)
    assert_equals(DT[:, f[:].remove(f["F":])], DT[:, :"E"])
