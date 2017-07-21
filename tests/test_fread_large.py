#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import os
import pytest
import datatable


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

def test_h2oaibenchmarks(dataroot):
    h2oaibenchmarks = os.path.join(dataroot, "h2oai-benchmarks", "Data")
    if not os.path.isdir(h2oaibenchmarks):
        pytest.skip("Directory %s does not exist" % h2oaibenchmarks)
    files = get_file_list(h2oaibenchmarks)
    for f in files:
        print("Reading file %s" % f)
        d = datatable.fread(f)
        # Include `f` in the assert so that the filename is printed in case of
        # a failure
        assert f and d.internal.verify_integrity() is None
