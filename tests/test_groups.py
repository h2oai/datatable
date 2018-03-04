#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable as dt


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
