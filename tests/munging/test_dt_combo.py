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
    assert df1.internal.check()
    assert df2.internal.check()
    assert df1.topython() == [[0, 6, 12, 18, 24]]
    assert df2.topython() == [[0, 6, 12, 18, 24]]

    df3 = df0[::-2, {"res": f.A * f.B}]
    df4 = df0[::2, :][::-1, :][:, f.A * f.B]
    assert df3.internal.check()
    assert df4.internal.check()
    assert df3.topython() == [[162, 98, 50, 18, 2]]
