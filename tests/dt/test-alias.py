#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2022 H2O.ai
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
import math
import re
import pytest
from datatable import dt, f, alias, FExpr, by
from tests import assert_equals


#-------------------------------------------------------------------------------
# Errors
#-------------------------------------------------------------------------------

def test_alias_not_a_string_or_sequence():
    DT = dt.Frame(list('abcde'))
    msg = 'The names parameter in alias\\(\\) '
    msg = msg + 'should be a string/list/tuple.+'
    with pytest.raises(TypeError, match = msg):
        DT[:, alias(f[0], 1)]


def test_alias_non_string_in_sequence():
    DT = dt.Frame(list('abcde'))
    msg = "Argument 1 in the names parameter in alias() "
    msg = msg + "should be a string; instead, got <class 'int'>"
    with pytest.raises(TypeError, match = re.escape(msg)):
        DT[:, alias([f[0], f[0]], ['rar', 1]), by(f[0])]

def test_alias_empty_seq():
    DT = dt.Frame(list('abcde'))
    msg = "Kindly provide at least one string value in the names argument."
    with pytest.raises(TypeError, match = msg):
        DT[:, alias([f[0], f[0]], []), by(f[0])]

def test_alias_no_args_passed():
    msg = (f"Function datatable.{alias.__name__}"
           "\\(\\) requires exactly 2 positional arguments, but none were given")
    with pytest.raises(TypeError, match = msg):
        alias()

def test_alias_size_mismatch():
    DT = dt.Frame(list('abcde'))
    msg = "The number of arguments\\(2\\) passed to names does not match "
    msg = msg + "the number of specified columns\\(1\\)."
    with pytest.raises(ValueError, match = msg):
         DT[:, dt.alias(f.C0, ['r', 'i'])]

def test_alias_empty_frame():
    DT = dt.Frame()
    msg = "The number of arguments\\(1\\) passed to names does not match "
    msg = msg + "the number of specified columns\\(0\\)."
    with pytest.raises(ValueError, match = msg):
         DT[:, alias(DT, 'C0')]


# #-------------------------------------------------------------------------------
# # Normal
# #-------------------------------------------------------------------------------

def test_alias_str():
  assert str(alias(f.A, 'rar')) == "FExpr<" + alias.__name__ + "(f.A, names=[rar,])>"
  assert str(alias(f.A, 'rar') + 1) == "FExpr<" + alias.__name__ + "(f.A, names=[rar,]) + 1>"
  assert str(alias(f.A + f.B, 'double')) == "FExpr<" + alias.__name__ + "(f.A + f.B, names=[double,])>"
  assert str(alias([f.B, f.C], ['c','b'])) == "FExpr<" + alias.__name__ + "([f.B, f.C], names=[c,b,])>"
  assert str(alias(f[:2], ['1','2'])) == "FExpr<"+ alias.__name__ + "(f[:2], names=[1,2,])>"



def test_alias_single_column():
    DT = dt.Frame([None, None, None])
    DT_mm = DT[:, alias(f[:], 'void')]
    DT.names = ['void']
    assert_equals(DT_mm, DT)

def test_alias_multiple_columns():
    DT = dt.Frame([range(5), [None, -1, None, 5.5, 3]])
    DT_alias = DT[:, dt.alias(f[:], ('column1', 'column2'))]
    DT_ref = DT[:, [dt.alias(f[0], 'column1'), dt.alias(f[1], 'column2')]]
    DT.names = ['column1', 'column2']
    assert_equals(DT_alias, DT)
    assert_equals(DT_ref, DT)


def test_alias_groupby():
    DT = dt.Frame([[2, 1, 1, 1, 2], [1.5, -1.5, math.inf, None, 3]])
    DT_cummax = DT[:, [dt.cummin(f[:]), dt.cummax(f[:])], by(dt.alias(f[0], 'group'))]
    DT_ref = dt.Frame([
                 [1, 1, 1, 2, 2],
                 [1, 1, 1, 2, 2],
                 [-1.5, -1.5, -1.5, 1.5, 1.5],
                 [1, 1, 1, 2, 2],
                 [-1.5, math.inf, math.inf, 1.5, 3]
             ], names = ('group','C0', 'C1', 'C2','C3'))
    assert_equals(DT_cummax, DT_ref)
