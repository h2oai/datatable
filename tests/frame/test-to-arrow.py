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
import datatable as dt
import pytest
import random

pyarrow_test = pytest.mark.usefixtures("pyarrow")



@pyarrow_test
def test_convert_bool_0(pa):
    DT = dt.Frame(x = [], stype=bool)
    tbl = DT.to_arrow()
    assert isinstance(tbl, pa.Table)
    assert tbl.to_pydict() == {"x": []}


@pyarrow_test
@pytest.mark.parametrize('v', [True, False, None])
def test_convert_bool_1(pa, v):
    DT = dt.Frame(y = [v], stype=bool)
    tbl = DT.to_arrow()
    assert isinstance(tbl, pa.Table)
    assert tbl.to_pydict() == {"y": [v]}


@pyarrow_test
def test_convert_bool_2(pa):
    DT = dt.Frame(Nm = [False, True, True, None, False])
    tbl = DT.to_arrow()
    assert isinstance(tbl, pa.Table)
    assert tbl.column_names == ['Nm']
    assert tbl.to_pydict() == {"Nm": [False, True, True, None, False]}



@pyarrow_test
def test_convert_bool_3(pa):
    src = [False, False, True, True, None] * 105
    DT = dt.Frame(R = src)
    tbl = DT.to_arrow()
    assert isinstance(tbl, pa.Table)
    assert tbl.column_names == ['R']
    assert tbl.shape == (len(src), 1)
    assert tbl.to_pydict()['R'] == src


@pyarrow_test
@pytest.mark.parametrize('seed', [random.getrandbits(64)])
def test_convert_bool_4(pa, seed):
    random.seed(seed)
    src = [random.choice([True, False, None])
           for i in range(random.randint(3, 12))] \
           * int(random.expovariate(0.01) + 1)
    DT = dt.Frame(col=src, stype=bool)
    tbl = DT.to_arrow()
    assert isinstance(tbl, pa.Table)
    assert tbl.column_names == ['col']
    assert tbl.shape == (len(src), 1)
    assert tbl.to_pydict()['col'] == src
