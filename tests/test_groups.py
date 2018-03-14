#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable as dt
from datatable import f, mean


def test_groups_internal0():
    d0 = dt.Frame([1, 2, 3])
    ri = d0.internal.sort(0)
    assert ri.ngroups == 0
    assert ri.group_sizes is None
    assert ri.group_offsets is None


def test_groups_internal1():
    d0 = dt.Frame([2, 7, 2, 3, 7, 2, 2, 0, None, 0])
    ri = d0.internal.sort(0, True)
    assert d0.nrows == 10
    assert ri.ngroups == 5
    assert ri.group_sizes == [1, 2, 4, 1, 2]
    assert ri.group_offsets == [0, 1, 3, 7, 8, 10]


def test_groups_internal2():
    d0 = dt.DataTable([[1,   5,   3,   2,   1,    3,   1,   1,   None],
                       ["a", "b", "c", "a", None, "f", "b", "h", "d"]],
                      names=["A", "B"])
    d1 = d0(groupby="A")
    assert d1.internal.check()
    ri = d1.internal.rowindex
    assert ri.ngroups == 5
    assert ri.group_sizes == [1, 4, 1, 2, 1]
    assert d1.topython() == [[None, 1, 1, 1, 1, 2, 3, 3, 5],
                             ["d", "a", None, "b", "h", "a", "c", "f", "b"]]
    d2 = d0(groupby="B")
    assert d2.internal.check()
    ri = d2.internal.rowindex
    assert ri.ngroups == 7
    assert ri.group_sizes == [1, 2, 2, 1, 1, 1, 1]
    assert d2.topython() == [[1, 1, 2, 5, 1, 3, None, 3, 1],
                             [None, "a", "a", "b", "b", "c", "d", "f", "h"]]


def test_groups_internal3():
    f0 = dt.Frame({"A": [1, 2, 1, 3, 2, 2, 2, 1, 3, 1], "B": range(10)})
    f1 = f0(select=[f.A, "B", f.A + f.B], groupby="A")
    assert f1.internal.check()
    assert f1.internal.isview
    assert f1.topython() == [[1, 1, 1, 1, 2, 2, 2, 2, 3, 3],
                             [0, 2, 7, 9, 1, 4, 5, 6, 3, 8],
                             [1, 3, 8, 10, 3, 6, 7, 8, 6, 11]]
    ri = f1.internal.rowindex
    assert ri.ngroups == 3
    assert ri.group_sizes == [4, 4, 2]
    assert ri.group_offsets == [0, 4, 8, 10]



#-------------------------------------------------------------------------------
# Groupby on small datasets
#-------------------------------------------------------------------------------

def test_groups1():
    f0 = dt.Frame({"A": [1, 2, 1, 2, 1, 3, 1, 1],
                   "B": [0, 1, 2, 3, 4, 5, 6, 7]})
    f1 = f0(select=mean(f.B), groupby=f.A)
    assert f1.stypes == (dt.float64,)
    assert f1.topython() == [[3.8, 2.0, 5.0]]



#-------------------------------------------------------------------------------
# Groupby on large datasets
#-------------------------------------------------------------------------------

def test_groups_large1():
    n = 251 * 4000
    xs = [(i * 19) % 251 for i in range(n)]
    f0 = dt.Frame({"A": xs})
    f1 = f0(groupby="A")
    assert f1.internal.rowindex.ngroups == 251
    assert f1.internal.rowindex.group_sizes == [4000] * 251
