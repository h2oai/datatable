#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import shutil
import tempfile

from tests import datatable


#-------------------------------------------------------------------------------
# Prepare fixtures & helper functions
#-------------------------------------------------------------------------------

def assert_equals(datatable1, datatable2):
    nrows, ncols = datatable1.shape
    assert datatable1.shape == datatable2.shape
    assert datatable1.names == datatable2.names
    assert datatable1.types == datatable2.types
    data1 = datatable1.internal.window(0, nrows, 0, ncols).data
    data2 = datatable2.internal.window(0, nrows, 0, ncols).data
    assert data1 == data2
    assert datatable1.internal.verify_integrity() is None


#-------------------------------------------------------------------------------
# Run the tests
#-------------------------------------------------------------------------------

def test_save_and_load():
    dir1 = tempfile.mkdtemp()
    dt0 = datatable.DataTable({"A": [1, 7, 100, 12],
                               "B": [True, None, False, None],
                               "C": ["alpha", "beta", None, "delta"]})
    datatable.save(dt0, dir1)
    dt1 = datatable.open(dir1)
    assert_equals(dt0, dt1)
    shutil.rmtree(dir1)
