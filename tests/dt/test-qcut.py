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
import math
import pytest
import random
from datatable import dt, stype, f, qcut, FExpr
from datatable.internal import frame_integrity_check
from tests import assert_equals


#-------------------------------------------------------------------------------
# Errors
#-------------------------------------------------------------------------------

def test_qcut_error_noargs():
    msg = r"Function datatable\.qcut\(\) requires exactly 1 positional " \
           "argument, but none were given"
    with pytest.raises(TypeError, match=msg):
        qcut()


def test_qcut_error_wrong_column_types():
    DT = dt.Frame([[0], [dt]/dt.obj64])
    msg = r"qcut\(\) cannot be applied to string or object columns, instead " \
           "column 1 has an stype: obj64"
    with pytest.raises(TypeError, match=msg):
        DT[:, qcut(f[:])]


def test_qcut_error_wrong_column_type_zero_rows():
    DT = dt.Frame(obj = [] / dt.obj64)
    msg = r"qcut\(\) cannot be applied to string or object columns, instead " \
           "column 0 has an stype: obj64"
    with pytest.raises(TypeError, match=msg):
        DT[:, qcut(f[:])]


def test_qcut_error_float_nquantiles():
    msg = "Expected an integer, instead got <class 'float'>"
    DT = dt.Frame(range(10))
    with pytest.raises(TypeError, match=msg):
        DT[:, qcut(f[:], nquantiles = 1.5)]


def test_qcut_error_zero_nquantiles():
    msg = "Number of quantiles must be positive, instead got: 0"
    DT = dt.Frame(range(10))
    with pytest.raises(ValueError, match=msg):
        DT[:, qcut(f[:], nquantiles = 0)]


def test_qcut_error_negative_nquantiles():
    msg = "Number of quantiles must be positive, instead got: -10"
    DT = dt.Frame(range(10))
    with pytest.raises(ValueError, match=msg):
        DT[:, qcut(f[:], nquantiles = -10)]


def test_qcut_error_negative_nquantiles_list():
    msg = r"All elements in nquantiles must be positive, got nquantiles\[1\]: -1"
    DT = dt.Frame([[3, 1, 4], [1, 5, 9]])
    with pytest.raises(ValueError, match=msg):
        DT[:, qcut(f[:], nquantiles = [10, -1])]


def test_qcut_error_inconsistent_nquantiles():
    msg = "When nquantiles is a list or a tuple, its length must be " \
          "the same as the number of input columns, " \
          "i.e. 2, instead got: 1"
    DT = dt.Frame([[3, 1, 4], [1, 5, 9]])
    with pytest.raises(ValueError, match=msg):
        DT[:, qcut(f[:], nquantiles = [10])]


def test_qcut_error_groupby():
    msg = r"qcut\(\) cannot be used in a groupby context"
    DT = dt.Frame(range(10))
    with pytest.raises(NotImplementedError, match=msg):
        DT[:, qcut(f[0]), f[0]]




#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

def test_qcut_str():
  assert str(qcut(f.A)) == "FExpr<qcut(f.A)>"
  assert str(qcut(f.A) + 1) == "FExpr<qcut(f.A) + 1>"
  assert str(qcut(f.A + f.B)) == "FExpr<qcut(f.A + f.B)>"
  assert str(qcut(f.B, nquantiles=3)) == "FExpr<qcut(f.B, nquantiles=3)>"
  assert str(qcut(f[:2], nquantiles=[3, 4])) == \
          "FExpr<qcut(f[:2], nquantiles=[3, 4])>"


def test_qcut_empty_frame():
    DT = dt.Frame()
    expr_qcut = qcut(DT)
    assert isinstance(expr_qcut, FExpr)
    assert_equals(DT[:, f[:]], DT)


def test_qcut_zerorow_frame():
    DT = dt.Frame([[], []])
    DT_qcut = DT[:, qcut(f[:])]
    expr_qcut = qcut(DT)
    assert isinstance(expr_qcut, FExpr)
    assert_equals(DT_qcut, dt.Frame([[] / dt.int32, [] / dt.int32]))


def test_qcut_trivial():
    DT = dt.Frame({"trivial": range(10)})
    DT_qcut = DT[:, qcut(f[:])]
    expr_qcut = qcut(DT)
    assert isinstance(expr_qcut, FExpr)
    assert_equals(DT, DT_qcut)


def test_qcut_expr_simple():
    DT = dt.Frame([range(0, 30, 3), range(0, 20, 2)])
    DT_qcut = DT[:, qcut(f[0] - f[1])]
    assert_equals(dt.Frame(range(10)), DT_qcut)


def test_qcut_one_row():
    nquantiles = [1, 2, 3, 4]
    DT = dt.Frame([[True], [404], [3.1415926], [None]])
    DT_qcut = DT[:, qcut(f[:], nquantiles = nquantiles)]
    assert DT_qcut.to_list() == [[0], [0], [1], [None]]


def test_qcut_small():
    nquantiles = [4, 5, 4, 2, 5, 4, 10, 3, 2, 5]
    colnames = ["bool", "one_group_odd", "one_group_even",
                "int_pos", "int_neg", "int", "float",
                "inf_max", "inf_min", "inf"]

    DT = dt.Frame(
           [[True, None, False, False, True, None],
           [None, 10, None, 10, 10, 10],
           [None, 10, None, 10, 10, 10],
           [3, None, 4, 1, 5, 4],
           [-5, -1, -1, -1, None, 0],
           [None, -5, -314, 0, 5, 314],
           [None, 1.4, 4.1, 1.5, 5.9, 1.4],
           [math.inf, 1.4, 4.1, 1.5, 5.9, 1.4],
           [-math.inf, 1.4, 4.1, 1.5, 5.9, 1.4],
           [-math.inf, 1.4, 4.1, math.inf, 5.9, 1.4]],
           names = colnames
         )

    DT_ref = dt.Frame(
               [[3, None, 0, 0, 3, None],
               [None, 2, None, 2, 2, 2],
               [None, 1, None, 1, 1, 1],
               [0, None, 1, 0, 1, 1],
               [0, 2, 2, 2, None, 4],
               [None, 0, 0, 1, 2, 3],
               [None, 0, 6, 3, 9, 0],
               [2, 0, 1, 0, 2, 0],
               [0, 0, 1, 0, 1, 0],
               [0, 1, 2, 4, 3, 1]],
               names = colnames,
               stypes = [stype.int32] * DT.ncols
             )

    DT_qcut = DT[:, qcut(f[:], nquantiles = nquantiles)]
    DT_qcut_frame = DT[:, qcut(DT, nquantiles = nquantiles)]
    assert_equals(DT_ref, DT_qcut)
    assert_equals(DT_ref, DT_qcut_frame)


@pytest.mark.parametrize("seed", [random.getrandbits(32) for _ in range(5)])
def test_qcut_random(pandas, seed):
    random.seed(seed)
    max_size = 20
    max_value = 100
    nrows = random.randint(1, max_size)
    ncols = 4
    stypes = (stype.bool8, stype.int32, stype.float64, stype.float64)
    names = ("bool", "int", "float", "nafloat")
    nquantiles = [random.randint(1, max_size) for _ in range(ncols)]
    data = [[] for _ in range(ncols)]

    for _ in range(nrows):
        data[0].append(random.randint(0, 1)
                       if random.random() > 0.1 else None)
        data[1].append(random.randint(-max_value, max_value)
                       if random.random() > 0.05 else None)
        data[2].append(random.random() * 2 * max_value - max_value
                       if random.random() > 0.2 else None)
        data[3].append(random.random() * 2 * max_value - max_value
                       if random.random() < 0.1 else None)

    DT = dt.Frame(data, stypes = stypes, names = names)
    DT_qcut = DT[:, qcut(f[:], nquantiles = nquantiles)]

    DT_nunique = DT.nunique()

    frame_integrity_check(DT_qcut)
    assert DT_qcut.names == names
    assert DT_qcut.stypes == tuple(stype.int32 for _ in range(ncols))

    for j in range(ncols):
        if DT_nunique[0, j] == 1:
            c = int((nquantiles[j] - 1) / 2)
            assert(DT_qcut[j].to_list() ==
                   [[None if DT[i, j] is None else c for i in range(nrows)]])
        else:
            if DT_qcut[j].countna1() == nrows:
                assert DT_qcut[j].min1() is None
                assert DT_qcut[j].max1() is None
            else:
                assert DT_qcut[j].min1() == 0
                assert DT_qcut[j].max1() == nquantiles[j] - 1


@pytest.mark.skip(reason="This test is used for dev only as we are not "
                  "fully consistent with pandas")
@pytest.mark.parametrize("seed", [random.getrandbits(32) for _ in range(5)])
def test_qcut_vs_pandas_random(pandas, seed):
    random.seed(seed)
    max_size = 20
    max_value = 100

    n = random.randint(1, max_size)

    ncols = 2
    nquantiles = [random.randint(1, max_size) for _ in range(ncols)]
    data = [[] for _ in range(ncols)]

    for _ in range(n):
        data[0].append(random.randint(-max_value, max_value))
        data[1].append(random.random() * 2 * max_value - max_value)

    DT = dt.Frame(data, stypes = [stype.int32, stype.float64])
    DT_qcut = DT[:, qcut(f[:], nquantiles = nquantiles)]
    PD_qcut = [pandas.qcut(data[i], nquantiles[i], labels=False) for i in range(ncols)]

    assert [list(PD_qcut[i]) for i in range(ncols)] == DT_qcut.to_list()

