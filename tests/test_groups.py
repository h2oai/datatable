#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable as dt
import pytest
import random
from datatable import f, mean, min, max, sum, by
from tests import same_iterables


def test_groups_internal0():
    d0 = dt.Frame([1, 2, 3])
    ri, gby = d0.internal.sort(0, False)
    assert gby is None


def test_groups_internal1():
    d0 = dt.Frame([2, 7, 2, 3, 7, 2, 2, 0, None, 0])
    ri, gby = d0.internal.sort(0, True)
    assert d0.nrows == 10
    assert gby.ngroups == 5
    assert gby.group_sizes == [1, 2, 4, 1, 2]
    assert gby.group_offsets == [0, 1, 3, 7, 8, 10]


def test_groups_internal2():
    d0 = dt.DataTable([[1,   5,   3,   2,   1,    3,   1,   1,   None],
                       ["a", "b", "c", "a", None, "f", "b", "h", "d"]],
                      names=["A", "B"])
    d1 = d0(groupby="A")
    d1.internal.check()
    gb = d1.internal.groupby
    assert gb.ngroups == 5
    assert gb.group_sizes == [1, 4, 1, 2, 1]
    assert d1.to_list() == [[None, 1, 1, 1, 1, 2, 3, 3, 5],
                            ["d", "a", None, "b", "h", "a", "c", "f", "b"]]
    d2 = d0(groupby="B")
    d2.internal.check()
    gb = d2.internal.groupby
    assert gb.ngroups == 7
    assert gb.group_sizes == [1, 2, 2, 1, 1, 1, 1]
    assert d2.to_list() == [[1, 1, 2, 5, 1, 3, None, 3, 1],
                            [None, "a", "a", "b", "b", "c", "d", "f", "h"]]


def test_groups_internal3():
    f0 = dt.Frame(A=[1, 2, 1, 3, 2, 2, 2, 1, 3, 1], B=range(10))
    f1 = f0(select=["B", f.A + f.B], groupby="A")
    f1.internal.check()
    assert f1.to_list() == [[1, 1, 1, 1, 2, 2, 2, 2, 3, 3],
                            [0, 2, 7, 9, 1, 4, 5, 6, 3, 8],
                            [1, 3, 8, 10, 3, 6, 7, 8, 6, 11]]
    gb = f1.internal.groupby
    assert gb
    assert gb.ngroups == 3
    assert gb.group_sizes == [4, 4, 2]
    assert gb.group_offsets == [0, 4, 8, 10]


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_groups_internal4(seed):
    random.seed(seed)
    n = 100000
    src = [random.getrandbits(10) for _ in range(n)]
    f0 = dt.Frame({"A": src})
    f1 = f0(groupby="A")
    gb = f1.internal.groupby
    f1.internal.check()
    assert gb
    grp_offsets = gb.group_offsets
    ssrc = sorted(src)
    x_prev = -1
    for i in range(gb.ngroups):
        s = set(ssrc[grp_offsets[i]:grp_offsets[i + 1]])
        assert len(s) == 1
        x = list(s)[0]
        assert x != x_prev
        x_prev = x


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_groups_internal5_strs(seed):
    random.seed(seed)
    n = 1000
    src = ["%x" % random.getrandbits(8) for _ in range(n)]
    f0 = dt.Frame({"A": src})
    f1 = f0(groupby="A")
    gb = f1.internal.groupby
    f1.internal.check()
    assert gb
    grp_offsets = gb.group_offsets
    ssrc = sorted(src)
    assert f1.to_list() == [ssrc]
    x_prev = None
    for i in range(gb.ngroups):
        s = set(ssrc[grp_offsets[i]:grp_offsets[i + 1]])
        assert len(s) == 1
        x = list(s)[0]
        assert x != x_prev
        x_prev = x



#-------------------------------------------------------------------------------
# Groupby on small datasets
#-------------------------------------------------------------------------------

def test_groups1():
    f0 = dt.Frame({"A": [1, 2, 1, 2, 1, 3, 1, 1],
                   "B": [0, 1, 2, 3, 4, 5, 6, 7]})
    f1 = f0(select=mean(f.B), groupby=f.A)
    assert f1.stypes == (dt.int8, dt.float64,)
    assert f1.to_list() == [[1, 2, 3], [3.8, 2.0, 5.0]]
    f2 = f0[:, mean(f.B), "A"]
    assert f2.stypes == f1.stypes
    assert f2.to_list() == f1.to_list()


def test_groups_multiple():
    f0 = dt.Frame({"color": ["red", "blue", "green", "red", "green"],
                   "size": [5, 2, 7, 13, 0]})
    f1 = f0[:, [min(f.size), max(f.size)], "color"]
    f1.internal.check()
    assert f1.to_list() == [["blue", "green", "red"], [2, 0, 5], [2, 7, 13]]


def test_groups_autoexpand():
    f0 = dt.Frame({"color": ["red", "blue", "green", "red", "green"],
                   "size": [5, 2, 7, 13, 0]})
    f1 = f0[:, [mean(f.size), f.size], f.color]
    f1.internal.check()
    assert f1.to_list() == [["blue", "green", "green", "red", "red"],
                            [2.0, 3.5, 3.5, 9.0, 9.0],
                            [2, 7, 0, 5, 13]]


@pytest.mark.skip(reason="Issue #1348")
def test_groupby_with_filter1():
    f0 = dt.Frame({"KEY": [1, 2, 1, 2, 1, 2], "X": [-10, 2, 3, 0, 1, -7]})
    f1 = f0[f.X > 0, sum(f.X), f.KEY]
    assert f1.to_list() == [[1, 2], [4, 2]]


@pytest.mark.skip(reason="Issue #1348")
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




#-------------------------------------------------------------------------------
# Test different reducers
#-------------------------------------------------------------------------------

def test_reduce_sum():
    f0 = dt.Frame({"color": ["red", "blue", "green", "red", "green"],
                   "size": [5, 2, 7, 13, -1]})
    f1 = f0[:, sum(f.size), f.color]
    f1.internal.check()
    assert f1.to_list() == [["blue", "green", "red"],
                            [2, 6, 18]]




#-------------------------------------------------------------------------------
# Groupby on large datasets
#-------------------------------------------------------------------------------

def test_groups_large1():
    n = 251 * 4000
    xs = [(i * 19) % 251 for i in range(n)]
    f0 = dt.Frame({"A": xs})
    f1 = f0(groupby="A")
    assert f1.internal.groupby.ngroups == 251
    assert f1.internal.groupby.group_sizes == [4000] * 251


@pytest.mark.parametrize("n,seed", [(x, random.getrandbits(32))
                                    for x in (100, 200, 300, 500, 1000, 0)])
def test_groups_large2_str(n, seed):
    random.seed(seed)
    while n == 0:
        n = int(random.expovariate(0.0005))
    src = ["%x" % random.getrandbits(6) for _ in range(n)]
    f0 = dt.Frame({"A": src})
    f1 = f0(groupby="A")
    f1.internal.check()
    assert f1.internal.groupby.ngroups == len(set(src))


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
    assert V.internal.isview
    assert V.shape == (4, 3)
    assert V.to_dict() == {'A': [2, 3, 2, 3],
                           'B': [6, 2, 3, 1],
                           'C': ['d', 'b', 'd', 'b']}
    RES = V[:, max(f.B), by(f.C)]
    assert RES.shape == (2, 2)
    assert RES.to_dict() == {'C': ['b', 'd'],
                             'C0': [2, 6]}


