#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2019 H2O.ai
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
from datatable import f, mean, min, max, sum, count, by, sort
from datatable.internal import frame_integrity_check
from tests import same_iterables, assert_equals, isview



def test_groups1a():
    DT0 = dt.Frame(A=[1, 2, 1])
    DT1 = DT0[:, "A", by("A")]
    assert_equals(DT1, dt.Frame([[1, 2], [1, 2]], names=["A", "A.0"]))


def test_groups1b():
    DT = dt.Frame([[1,   5,   3,   2,   1,    3,   1,   1,   None],
                   ["a", "b", "c", "a", None, "f", "b", "h", "d"]],
                  names=["A", "B"])
    d1 = DT[:, :, by("A")]
    assert_equals(
        d1, dt.Frame(A=[None, 1, 1, 1, 1, 2, 3, 3, 5],
                     B=["d", "a", None, "b", "h", "a", "c", "f", "b"]))

    d2 = DT[:, :, by("B")]
    assert_equals(
        d2, dt.Frame(B=[None, "a", "a", "b", "b", "c", "d", "f", "h"],
                     A=[1, 1, 2, 5, 1, 3, None, 3, 1]))


def test_groups2a():
    DT0 = dt.Frame(A=[1, 2, 1], B=[3, 4, 5])
    DT1 = DT0[:, [f.A, f.B, f.A + f.B], by("A")]
    assert_equals(DT0, dt.Frame(A=[1, 2, 1], B=[3, 4, 5]))
    assert_equals(DT1, dt.Frame([[1, 1, 2], [1, 1, 2], [3, 5, 4], [4, 6, 6]],
                                names=["A", "A.0", "B", "C0"]))


def test_groups2b():
    DT0 = dt.Frame(A=[1, 2, 1, 3, 2, 2, 2, 1, 3, 1], B=range(10))
    DT1 = DT0[:, [f.B, f.A + f.B], by(f.A)]
    assert_equals(DT1, dt.Frame(A=[1, 1, 1, 1, 2, 2, 2, 2, 3, 3],
                                B=[0, 2, 7, 9, 1, 4, 5, 6, 3, 8],
                                C0=[1, 3, 8, 10, 3, 6, 7, 8, 6, 11]))


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_groups3(seed):
    random.seed(seed)
    n = 100000
    src = [random.getrandbits(10) for _ in range(n)]
    f0 = dt.Frame(A=src, B=range(n))
    f1 = f0[:, :, by("A")]
    frame_integrity_check(f1)
    assert f1.shape == (n, 2)
    assert f1.names == ("A", "B")
    f1A, f1B = f1.to_list()
    f1A.append(None)
    curr_value = f1A[0]
    group_start = 0
    for i in range(n + 1):
        if f1A[i] != curr_value:
            group = f1B[group_start:i]
            assert group == sorted(group)
            curr_value = f1A[i]
            group_start = i
    assert group_start == n
    assert curr_value is None


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_groups4(seed):
    random.seed(seed)
    n = 1000
    src = ["%x" % random.getrandbits(8) for _ in range(n)]
    f0 = dt.Frame(A=src, B=range(n))
    f1 = f0[:, :, by("A")]
    frame_integrity_check(f1)
    assert f1.shape == (n, 2)
    assert f1.names == ("A", "B")
    f1A, f1B = f1.to_list()
    f1A.append(None)
    curr_value = f1A[0]
    group_start = 0
    for i in range(n + 1):
        if f1A[i] != curr_value:
            group = f1B[group_start:i]
            assert group == sorted(group)
            curr_value = f1A[i]
            group_start = i
    assert group_start == n
    assert curr_value is None



#-------------------------------------------------------------------------------
# Groupby on small datasets
#-------------------------------------------------------------------------------

def test_group_empty_frame1():
    DT = dt.Frame(A=[])
    assert DT.shape == (0, 1)
    D1 = DT[:, :, by(f.A)]
    frame_integrity_check(D1)


def test_group_empty_frame2():
    DT = dt.Frame(A=[])
    D1 = DT[:, count(), by(f.A)]
    frame_integrity_check(D1)
    assert D1.shape == (0, 2)
    assert D1.stypes == (DT.stype, dt.int64)


def test_group_empty_frame3():
    DT = dt.Frame(A=[], stype=dt.float32)
    D2 = DT[:, count(f.A), by(f.A)]
    frame_integrity_check(D2)
    assert D2.shape == (0, 2)
    assert D2.stypes == (DT.stype, dt.int64)


def test_group_empty_frame4():
    DT = dt.Frame(A=[], stype=dt.float32)
    D2 = DT[:, sum(f.A), by(f.A)]
    frame_integrity_check(D2)
    assert D2.shape == (0, 2)
    assert D2.stypes == (dt.float32, dt.float32)


def test_groups_small1():
    DT0 = dt.Frame({"A": [1, 2, 1, 2, 1, 3, 1, 1],
                    "B": [0, 1, 2, 3, 4, 5, 6, 7]})
    DT1 = DT0[:, mean(f.B), by(f.A)]
    assert_equals(DT1, dt.Frame(A=[1, 2, 3], B=[3.8, 2.0, 5.0]))
    DT2 = DT0[:, mean(f.B), "A"]
    assert_equals(DT2, DT1)


def test_groups_multiple():
    f0 = dt.Frame({"color": ["red", "blue", "green", "red", "green"],
                   "size": [5, 2, 7, 13, 0]})
    f1 = f0[:, [min(f.size), max(f.size)], "color"]
    frame_integrity_check(f1)
    assert f1.to_list() == [["blue", "green", "red"], [2, 0, 5], [2, 7, 13]]


def test_groups_autoexpand():
    f0 = dt.Frame({"color": ["red", "blue", "green", "red", "green"],
                   "size": [5, 2, 7, 13, 0]})
    f1 = f0[:, [mean(f.size), f.size], f.color]
    frame_integrity_check(f1)
    assert f1.to_list() == [["blue", "green", "green", "red", "red"],
                            [2.0, 3.5, 3.5, 9.0, 9.0],
                            [2, 7, 0, 5, 13]]




#-------------------------------------------------------------------------------
# Test different reducers
#-------------------------------------------------------------------------------

def test_reduce_sum():
    f0 = dt.Frame({"color": ["red", "blue", "green", "red", "green"],
                   "size": [5, 2, 7, 13, -1]})
    f1 = f0[:, sum(f.size), f.color]
    frame_integrity_check(f1)
    assert f1.to_list() == [["blue", "green", "red"],
                            [2, 6, 18]]




#-------------------------------------------------------------------------------
# Groupby on large datasets
#-------------------------------------------------------------------------------

def test_groups_large1():
    n = 251 * 4000
    xs = [(i * 19) % 251 for i in range(n)]
    f0 = dt.Frame({"A": xs})
    f1 = f0[:, count(), by("A")]
    assert f1.to_list() == [list(range(251)), [4000] * 251]


@pytest.mark.parametrize("n,seed", [(x, random.getrandbits(32))
                                    for x in (100, 200, 300, 500, 1000, 0)])
def test_groups_large2_str(n, seed):
    random.seed(seed)
    while n == 0:
        n = int(random.expovariate(0.0005))
    src = ["%x" % random.getrandbits(6) for _ in range(n)]
    f0 = dt.Frame({"A": src})
    f1 = f0[:, count(), by("A")]
    frame_integrity_check(f1)
    assert f1.nrows == len(set(src))


@pytest.mark.parametrize("seed", [random.getrandbits(32) for _ in range(10)])
def test_groupby_large_random_integers(seed):
    random.seed(seed)
    ngrps1 = random.choice([1, 1, 2, 2, 2, 3, 4, 5])
    n0 = 1 << random.choice([1, 1, 2, 2, 2, 3, 3, 3, 4, 5, 6, 7])
    chunks = ([random.sample(range(n0), random.randint(1, n0))] +
              [random.sample([0] * 100 + list(range(256)),
                             random.randint(1, 20))
               for i in range(ngrps1)])
    n = int(random.expovariate(0.0001)) + 10
    sample = [sum(random.choice(chunks[i]) << (8 * i)
                  for i in range(len(chunks)))
              for _ in range(n)]
    nuniques = len(set(sample))
    f0 = dt.Frame(sample)
    assert f0.nunique1() == nuniques
    f1 = dt.rbind(*([f0] * random.randint(2, 20)))
    assert f1.nunique1() == nuniques


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_issue_2242(seed):
    n = 25000
    X = dt.Frame(AGE=[random.randint(1, 50) for i in range(n)],
                 PAY=[random.choice([True, False]) for i in range(n)])
    RES = X[:, dt.math.log((count() + 1)/(sum(f.PAY) + 0.5) - 1), by(f.AGE)]
    assert RES.shape == (50, 2)
    data = RES.to_list()
    assert data[0] == list(range(1, 51))
    assert all(isinstance(x, float) for x in data[1])




#-------------------------------------------------------------------------------
# Groupby on multiple columns
#-------------------------------------------------------------------------------

def test_groupby_multi():
    DT = dt.Frame(A=[1, 2, 3] * 3, B=[1, 2] * 4 + [1], C=range(9))
    res = DT[:, sum(f.C), by("A", "B")]
    assert res.to_list() == [[1, 1, 2, 2, 3, 3],
                             [1, 2, 1, 2, 1, 2],
                             [6, 3, 4, 8, 10, 5]]


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_groupby_multi_large(seed):
    random.seed(seed)
    letters = "abcdefghijklmn"
    n = 100 + int(random.expovariate(0.0001))
    col0 = [random.choice([True, False]) for _ in range(n)]
    col1 = [random.randint(-10, 10) for _ in range(n)]
    col2 = [random.choice(letters) for _ in range(n)]
    col3 = [random.random() for _ in range(n)]
    rows = [(col0[i], col1[i], col2[i], col3[i]) for i in range(n)]
    rows.sort()
    grouped = []
    lastkey = rows[0][:3]
    sumval = 0
    for i in range(n):
        ikey = rows[i][:3]
        if ikey != lastkey:
            grouped.append(lastkey + (sumval,))
            lastkey = ikey
            sumval = 0
        sumval += rows[i][3]
    grouped.append(lastkey + (sumval,))
    DT0 = dt.Frame([col0, col1, col2, col3], names=["A", "B", "C", "D"])
    DT1 = DT0[:, sum(f.D), by(f.A, f.B, f.C)]
    DT2 = dt.Frame(grouped)
    assert same_iterables(DT1.to_list(), DT2.to_list())


def test_groupby_on_view():
    # See issue #1542
    DT = dt.Frame(A=[1, 2, 3, 1, 2, 3],
                  B=[3, 6, 2, 4, 3, 1],
                  C=['b', 'd', 'b', 'b', 'd', 'b'])
    V = DT[f.A != 1, :]
    assert isview(V)
    assert_equals(V, dt.Frame(A=[2, 3, 2, 3],
                              B=[6, 2, 3, 1],
                              C=['d', 'b', 'd', 'b']))
    RES = V[:, max(f.B), by(f.C)]
    assert_equals(RES, dt.Frame(C=['b', 'd'], B=[2, 6]))


def test_groupby_empty():
    DT = dt.Frame(A=range(5))
    res = DT[:, :, by()]
    assert_equals(res, DT)


def test_groupby_select_all_columns():
    # Check that when selecting all columns, the result has the same number
    # of columns as the original.
    DT = dt.Frame(id2=[1, 2] * 3, id4=[1] * 6, v3=[1, 3, 2, 3, 3, 3])
    res = DT[:, :, by(f.id2, f.id4)]
    assert_equals(res, dt.Frame(id2=[1, 1, 1, 2, 2, 2], id4=[1] * 6,
                                v3=[1, 2, 3, 3, 3, 3]))


def test_groupby_with_sort():
    DT = dt.Frame(A=[1,2,3]*4, B=[1,2]*6, C=range(12))
    R1 = DT[:, count(), by(f.A, f.B)]
    R2 = DT[:, count(), by(f.A, f.B), sort(f.C)]
    R0 = dt.Frame(A=[1, 1, 2, 2, 3, 3],
                  B=[1, 2, 1, 2, 1, 2],
                  count=[2] * 6, stypes={"count": dt.int64})
    assert_equals(R1, R0)
    assert_equals(R2, R0)



#-------------------------------------------------------------------------------
# i + by
#-------------------------------------------------------------------------------

@pytest.mark.xfail(reason="Issue #1586")
def test_groupby_with_filter1():
    f0 = dt.Frame(KEY=[1, 2, 1, 2, 1, 2], X=[-10, 2, 3, 0, 1, -7])
    f1 = f0[f.X > 0, sum(f.X), f.KEY]
    assert f1.to_list() == [[1, 2], [4, 2]]


@pytest.mark.xfail(reason="Issue #1586")
def test_groupby_with_filter2():
    # Check that rowindex works even when applied to a view
    n = 10000
    src0 = [random.getrandbits(2) for _ in range(n)]
    src1 = [random.gauss(1, 1) for _ in range(n)]
    f0 = dt.Frame({"key": src0, "val": src1})
    f1 = f0[f.val >= 0, :]
    f2 = f1[f.val <= 2, sum(f.val), f.key]
    answer = [sum(src1[i] for i in range(n)
                  if src0[i] == key and 0 <= src1[i] <= 2)
              for key in range(4)]
    assert f2.to_list() == [[0, 1, 2, 3], answer]


def test_int_row_with_by():
    DT = dt.Frame(A=[1, 2, 3, 1, 2, 1], B=range(6))
    assert_equals(DT[0, :, by(f.A)], dt.Frame(A=[1, 2, 3], B=[0, 1, 2]))
    assert_equals(DT[1, :, by(f.A)], dt.Frame(A=[1, 2], B=[3, 4]))
    assert_equals(DT[2, :, by(f.A)], dt.Frame(A=[1], B=[5], stype=dt.int32))
    assert_equals(DT[3, :, by(f.A)], dt.Frame(A=[], B=[], stype=dt.int32))
    assert_equals(DT[-1, :, by(f.A)], dt.Frame(A=[1, 2, 3], B=[5, 4, 2]))
    assert_equals(DT[-2, :, by(f.A)], dt.Frame(A=[1, 2], B=[3, 1]))
    assert_equals(DT[-3, :, by(f.A)], dt.Frame(A=[1], B=[0], stype=dt.int32))
    assert_equals(DT[-4, :, by(f.A)], dt.Frame(A=[], B=[], stype=dt.int32))
