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
from datatable import dt, f, FExpr, by
from tests import assert_equals


#-------------------------------------------------------------------------------
# Errors
#-------------------------------------------------------------------------------

def test_alias_names_wrong_type():
    DT = dt.Frame(list('abcde'))
    msg = r"datatable.FExpr.alias\(\) expects all names to be strings, " \
          r"or lists/tuples of strings, instead name 0 is <class 'int'>"
    with pytest.raises(TypeError, match = msg):
        DT[:, f[0].alias(1)]


def test_alias_names_wrong_element_type():
    DT = dt.Frame(list('abcde'))
    msg = r"datatable.FExpr.alias\(\) expects all elements of lists/tuples " \
          r"of names to be strings, instead for name 0 element 1 is " \
          r"<class 'int'>"
    with pytest.raises(TypeError, match = msg):
        DT[:, f[0, 0].alias(['rar', 1])]


def test_alias_empty_cols():
    DT = dt.Frame(range(5))
    msg = "The number of columns does not match the number of names: 0 vs 1"
    with pytest.raises(ValueError, match = msg):
        DT[:, f[None].alias("new_name")]


def test_alias_empty_names():
    DT = dt.Frame(list('abcde'))
    msg = "The number of columns does not match the number of names: 2 vs 0"
    with pytest.raises(ValueError, match = msg):
        DT[:, f[0, 0].alias([])]


def test_alias_no_args():
    DT = dt.Frame(list('abcde'))
    msg = "The number of columns does not match the number of names: 1 vs 0"
    with pytest.raises(ValueError, match = msg):
        DT[:, f[0].alias()]


def test_alias_size_mismatch():
    DT = dt.Frame(list('abcde'))
    msg = "The number of columns does not match the number of names: 1 vs 2"
    with pytest.raises(ValueError, match = msg):
        DT[:, f.C0.alias('r', 'i')]


def test_alias_empty_frame():
    DT = dt.Frame()
    msg = "The number of columns does not match the number of names: 0 vs 1"
    with pytest.raises(ValueError, match = msg):
        DT[:, f[:].alias('C0')]


#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

def test_alias_str():
  assert str(f.A.alias('rar')) == "FExpr<alias(f.A, [rar,])>"
  assert str((f.A + 1).alias('rar')) == "FExpr<alias(f.A + 1, [rar,])>"
  assert str((f.A + f.B).alias('double')) == "FExpr<alias(f.A + f.B, [double,])>"
  assert str(f['B', 'C'].alias(['c','b'])) == "FExpr<alias(f[['B', 'C']], [c,b,])>"
  assert str(f['B', 'C'].alias('c','b')) == "FExpr<alias(f[['B', 'C']], [c,b,])>"
  assert str(f[:2].alias(['1','2'])) == "FExpr<alias(f[:2], [1,2,])>"


def test_alias_single_column():
    DT = dt.Frame([None, None, None])
    DT_mm = DT[:, f[:].alias('void')]
    DT.names = ['void']
    assert_equals(DT_mm, DT)


def test_alias_multiple_columns():
    DT = dt.Frame([range(5), [None, -1, None, 5.5, 3]])
    DT_multiple = DT[:, f[:].alias('column1', 'column2')]
    DT_twice_single = DT[:, [f[0].alias('column1'), f[1].alias('column2')]]
    DT.names = ['column1', 'column2']
    assert_equals(DT_multiple, DT)
    assert_equals(DT_twice_single, DT)


def test_alias_groupby():
    DT = dt.Frame([[2, 1, 1, 1, 2], [1.5, -1.5, math.inf, None, 3]])
    DT_cummax = DT[:, [dt.cummin(f[:]), dt.cummax(f[:])], by(f[0].alias('group'))]
    DT_ref = dt.Frame([
                 [1, 1, 1, 2, 2],
                 [1, 1, 1, 2, 2],
                 [-1.5, -1.5, -1.5, 1.5, 1.5],
                 [1, 1, 1, 2, 2],
                 [-1.5, math.inf, math.inf, 1.5, 3]
             ], names = ('group','C0', 'C1', 'C2','C3'))
    assert_equals(DT_cummax, DT_ref)

