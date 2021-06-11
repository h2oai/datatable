#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2021 H2O.ai
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
from datatable import dt, f, as_type
from tests import assert_equals



def test_as_type_arguments():
    msg = r"Function datatable.as_type\(\) requires exactly 2 positional " \
          r"arguments, but none were given"
    with pytest.raises(TypeError, match=msg):
        as_type()

    msg = r"Function datatable.as_type\(\) requires exactly 2 positional " \
          r"arguments, but only 1 was given"
    with pytest.raises(TypeError, match=msg):
        as_type(f.A)

    msg = r"Function datatable.as_type\(\) takes at most 2 positional " \
          r"arguments, but 3 were given"
    with pytest.raises(TypeError, match=msg):
        as_type(f.A, f.B, f.C)


def test_as_type_repr():
    assert repr(as_type(f.A, dt.int64)) == 'FExpr<as_type(f.A, int64)>'
    assert repr(as_type(f[1], dt.str32)) == 'FExpr<as_type(f[1], str32)>'


@pytest.mark.parametrize("target", [dt.int64, int, dt.str32, dt.float32])
def test_astype_stype(target):
    DT = dt.Frame(A=range(5))
    assert_equals(DT[:, as_type(f.A, target)],
                  dt.Frame(A=range(5), stype=target))


def test_astype_type():
    DT = dt.Frame(A=range(10))
    assert_equals(DT[:, as_type(f.A, dt.Type.float64)],
                  dt.Frame(A=range(10), stype=dt.float64))
