#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable as dt
from datatable import stype, f
from datatable.internal import frame_columns_virtual, frame_integrity_check



def test_columns_rows():
    df0 = dt.Frame([range(10), range(0, 20, 2)], names=["A", "B"],
                   stype=stype.int32)
    assert df0.shape == (10, 2)

    df1 = df0[::2, f.A + f.B]
    df2 = df0[::2, :][:, f.A + f.B]
    frame_integrity_check(df1)
    frame_integrity_check(df2)
    assert df1.to_list() == [[0, 6, 12, 18, 24]]
    assert df2.to_list() == [[0, 6, 12, 18, 24]]

    df3 = df0[::-2, {"res": f.A * f.B}]
    df4 = df0[::2, :][::-1, :][:, f.A * f.B]
    frame_integrity_check(df3)
    frame_integrity_check(df4)
    assert df3.to_list() == [[162, 98, 50, 18, 2]]


def test_issue1225():
    f0 = dt.Frame(A=[1, 2, 3], B=[5, 6, 8], stypes = {"B": "int8"})
    f1 = f0[::-1, :][:, [dt.float64(f.A), f.B]]
    assert frame_columns_virtual(f1) == (False, True)
    f1.materialize()
    assert f1.stypes == (stype.float64, stype.int8)
    assert f1.to_list() == [[3.0, 2.0, 1.0], [8, 6, 5]]
