#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import shutil
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
