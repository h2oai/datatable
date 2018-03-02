#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable as dt


def test_groups_internal0():
    d0 = dt.DataTable([1, 2, 3])
    ri = d0.internal.sort(0)
    assert ri.ngroups == 0
    assert ri.group_sizes is None
    assert ri.group_offsets is None


def test_groups_internal1():
    d0 = dt.DataTable([2, 7, 2, 3, 7, 2, 2, 0, None, 0])
    ri = d0.internal.sort(0, True)
    assert d0.nrows == 10
    assert ri.ngroups == 5
    assert ri.group_sizes == [1, 2, 4, 1, 2]
    assert ri.group_offsets == [0, 1, 3, 7, 8, 10]
