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



#-------------------------------------------------------------------------------
# Booleans
#-------------------------------------------------------------------------------

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



#-------------------------------------------------------------------------------
# Integers
#-------------------------------------------------------------------------------

@pyarrow_test
@pytest.mark.parametrize("stype", [dt.int8, dt.int16, dt.int32, dt.int64])
def test_convert_int_0(pa, stype):
    DT = dt.Frame([[]], stype=stype)
    assert DT.shape == (0, 1)
    tbl = DT.to_arrow()
    assert isinstance(tbl, pa.Table)
    assert tbl.shape == (0, 1)
    assert tbl.to_pydict() == {"C0": []}


@pyarrow_test
@pytest.mark.parametrize("stype", [dt.int8, dt.int16, dt.int32, dt.int64])
def test_convert_int_1(pa, stype):
    src = [3, 17, None, -1]
    DT = dt.Frame(src, stype=stype)
    tbl = DT.to_arrow()
    assert isinstance(tbl, pa.Table)
    assert tbl.to_pydict() == {"C0": src}


@pyarrow_test
@pytest.mark.parametrize("stype", [dt.int8, dt.int16, dt.int32, dt.int64])
def test_convert_int_2(pa, stype):
    src = [None] * 12000
    src[33] = -1
    src[178] = 29
    src[5555:6666] = [0]*1111
    DT = dt.Frame(DATA=src, stype=stype)
    tbl = DT.to_arrow()
    assert isinstance(tbl, pa.Table)
    assert tbl.shape == (len(src), 1)
    assert tbl.to_pydict() == {"DATA": src}


@pyarrow_test
@pytest.mark.parametrize('seed', [random.getrandbits(64)])
def test_convert_int_3(pa, seed):
    random.seed(seed)
    src = [random.randint(-1000, 10000) for i in range(10)] \
          * int(random.expovariate(0.001) + 1)
    DT = dt.Frame(iii=src)
    tbl = DT.to_arrow()
    assert tbl.to_pydict() == {"iii": src}



#-------------------------------------------------------------------------------
# Floats
#-------------------------------------------------------------------------------

@pyarrow_test
@pytest.mark.parametrize("stype", [dt.float32, dt.float64])
def test_convert_float_0(pa, stype):
    DT = dt.Frame(D=[], stype=stype)
    assert DT.shape == (0, 1)
    tbl = DT.to_arrow()
    assert isinstance(tbl, pa.Table)
    assert tbl.shape == (0, 1)
    assert tbl.to_pydict() == {"D": []}


@pyarrow_test
@pytest.mark.parametrize("stype", [dt.float32, dt.float64])
def test_convert_float_1(pa, stype):
    src = [1.4, 1.56, -2.2, 3.14, 9.99999]
    DT = dt.Frame({"my perfect data": src}, stype=stype)
    tbl = DT.to_arrow()
    assert tbl.to_pydict() == DT.to_dict()


@pyarrow_test
@pytest.mark.parametrize('seed', [random.getrandbits(64)])
def test_convert_float_2(pa, seed):
    random.seed(seed)
    src = [random.random() * 10000 for i in range(10)] \
          * int(random.expovariate(0.001) + 1)
    DT = dt.Frame(dddd=src)
    tbl = DT.to_arrow()
    assert tbl.to_pydict() == {"dddd": src}
