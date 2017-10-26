#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import shutil
import tempfile
import datatable
from tests import assert_equals



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
