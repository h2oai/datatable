#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import shutil
import pytest
import tempfile
import datatable as dt
from tests import assert_equals



#-------------------------------------------------------------------------------
# Run the tests
#-------------------------------------------------------------------------------

def test_save_and_load():
    dir1 = tempfile.mkdtemp()
    dt0 = dt.DataTable({"A": [1, 7, 100, 12],
                        "B": [True, None, False, None],
                        "C": ["alpha", "beta", None, "delta"]})
    dt.save(dt0, dir1)
    dt1 = dt.open(dir1)
    assert_equals(dt0, dt1)
    shutil.rmtree(dir1)


def test_empty_string_col():
    """
    Test that DataTable with an empty string column can be saved/opened.
    See #604
    """
    dir1 = tempfile.mkdtemp()
    dt0 = dt.DataTable([[1, 2, 3], ["", "", ""]])
    dt.save(dt0, dir1)
    dt1 = dt.open(dir1)
    assert_equals(dt0, dt1)
    shutil.rmtree(dir1)


def test_issue627():
    """Test saving DataTable with unicode file names"""
    dir1 = tempfile.mkdtemp()
    dt0 = dt.DataTable({"py": [1], "ру": [2], "рy": [3], "pу": [4]})
    assert dt0.shape == (1, 4)
    dt.save(dt0, dir1)
    dt1 = dt.open(dir1)
    assert_equals(dt0, dt1)
    shutil.rmtree(dir1)


def test_obj_columns(tempdir):
    src1 = [1, 2, 3, 4]
    src2 = [(2, 3), (5, 6, 7), 9, {"A": 3}]
    d0 = dt.DataTable([src1, src2], names=["A", "B"])
    print("Saved to %s" % tempdir)
    assert d0.internal.check()
    assert d0.ltypes == (dt.ltype.int, dt.ltype.obj)
    assert d0.shape == (4, 2)
    with pytest.warns(UserWarning) as ws:
        dt.save(d0, tempdir)
    assert len(ws) == 1
    assert "Column 'B' of type obj64 was not saved" in ws[0].message.args[0]
    del d0
    d1 = dt.open(tempdir)
    assert d1.internal.check()
    assert d1.shape == (4, 1)
    assert d1.names == ("A", )
    assert d1.topython() == [src1]


def test_save_view(tempdir):
    dt0 = dt.DataTable([4, 0, -2, 3, 17, 2, 0, 1, 5], names=["fancy"])
    dt1 = dt0.sort(0)
    assert dt1.internal.isview
    dt.save(dt1, tempdir)
    dt2 = dt.open(tempdir)
    assert not dt2.internal.isview
    assert dt2.names == dt1.names
    assert dt2.topython() == dt1.topython()
