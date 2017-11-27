#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
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
