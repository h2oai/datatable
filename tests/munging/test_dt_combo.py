#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable as dt
from datatable import stype, f



def test_columns_rows():
    df0 = dt.Frame([range(10), range(0, 20, 2)], names=["A", "B"],
                   stype=stype.int32)
    assert df0.shape == (10, 2)

    df1 = df0[::2, f.A + f.B]
    df2 = df0[::2, :][:, f.A + f.B]
    df1.internal.check()
    df2.internal.check()
    assert df1.to_list() == [[0, 6, 12, 18, 24]]
    assert df2.to_list() == [[0, 6, 12, 18, 24]]

    df3 = df0[::-2, {"res": f.A * f.B}]
    df4 = df0[::2, :][::-1, :][:, f.A * f.B]
    df3.internal.check()
    df4.internal.check()
    assert df3.to_list() == [[162, 98, 50, 18, 2]]


def test_issue1225():
    f0 = dt.Frame(A=[1, 2, 3], B=[5, 6, 8])
    f1 = f0[::-1, :][:, [dt.float64(f.A), f.B]]
    # TODO: restore this check after #1188
    # assert f1.internal.isview
    f1.materialize()
    assert f1.stypes == (stype.float64, stype.int8)
    assert f1.to_list() == [[3.0, 2.0, 1.0], [8, 6, 5]]
