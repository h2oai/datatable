#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import os
import pytest
import datatable
from tests import iflarge

#-------------------------------------------------------------------------------
# Helper functions
#-------------------------------------------------------------------------------

def get_file_list(rootdir):
    exts = [".csv", ".txt", ".data", ".gz", ".csv.zip", ".txt.zip", ".data.zip"]
    out = set()
    for dirname, subdirs, files in os.walk(rootdir):
        for filename in files:
            if ("readme" not in filename and
                    any(filename.endswith(ext) for ext in exts)):
                f = os.path.join(dirname, filename)
                if filename.endswith(".zip") and f[:-4] in out:
                    continue
                if f + ".zip" in out:
                    out.remove(f + ".zip")
                out.add(f)
            else:
                print("Skipping file %s" % filename)
    return out


#-------------------------------------------------------------------------------
# Run the tests
#-------------------------------------------------------------------------------

@iflarge
@pytest.mark.skipif('not os.path.exists("../h2oai-benchmarks")')
def test_h2oai_benchmarks():
    files = get_file_list("../h2oai-benchmarks/Data")
    for f in files:
        print("Reading file %s" % f)
        d = datatable.fread(f)
        assert repr(d)
