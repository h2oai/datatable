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
import math
import pytest
import random
import datatable as dt
from datatable import f, g, stype, ltype, join
from datatable import rowall
from datatable.internal import frame_integrity_check
from tests import list_equals, assert_equals, noop

stypes_int = ltype.int.stypes
stypes_float = ltype.real.stypes
stypes_str = ltype.str.stypes


#-------------------------------------------------------------------------------
# rowall()
#-------------------------------------------------------------------------------

def test_rowall_simple():
    DT = dt.Frame([[True, True, False, True, None, True],
                   [True, False, True, True, True, True],
                   [True, True,  True, True, True, True]])
    RES = DT[:, rowall(f[:])]
    assert_equals(RES, dt.Frame([True, False, False, True, False, True]))


def test_rowall_single_column():
    DT = dt.Frame([[True, False, None, True]])
    RES = rowall(DT)
    assert_equals(RES, dt.Frame([True, False, False, True]))


def test_rowall_sequence_of_columns():
    DT = dt.Frame(A=[True, False, True],
                  B=[3, 6, 1],
                  C=[True, None, False],
                  D=[True, True, True],
                  E=['ha', 'nope', None])
    RES = DT[:, rowall(f.A, f["C":"D"])]
    assert_equals(RES, dt.Frame([True, False, False]))


def test_rowall_no_columns():
    DT = dt.Frame(A=[True, False, True, True, None])
    R1 = DT[:, rowall()]
    R2 = DT[:, rowall(f[str])]
    assert_equals(R1, dt.Frame([True]))
    assert_equals(R2, dt.Frame([True]))


@pytest.mark.parametrize('st', stypes_int + stypes_float + stypes_str)
def test_rowall_wrong_type(st):
    DT = dt.Frame(A=[], stype=st)
    with pytest.raises(TypeError, match="Function `rowall` requires a sequence "
                                        "of boolean columns"):
        assert DT[:, rowall(f.A)]
