#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import os
import pytest
import datatable

root_env_name = "DT_LARGE_TESTS_ROOT"

#-------------------------------------------------------------------------------
# Helper functions
#-------------------------------------------------------------------------------

def param(id):
    return pytest.param(lambda: id, id=id)

def skipped(reason, id=None):
    return pytest.param(lambda: pytest.skip(reason), id=id)

def failed(reason, id=None):
    return pytest.param(lambda: pytest.fail(reason, False), id=id)


# Function returns a collection of FUNCTIONS. Each function returns a
# unique file path str (or calls pytest.fail).
# This prevents pytest from completely aborting if a failure occurs here.
def get_file_list(*path):
    d = os.environ.get(root_env_name, "")
    if d == "":
        return [skipped("Environment variable '%s' is empty or not defined"
                        % root_env_name)]
    if not os.path.isdir(d):
        return [failed("Directory '%s' (%s) does not exist"
                       % (d, root_env_name), id=d)]
    rootdir = os.path.join(d, *path)
    if not os.path.isdir(rootdir):
        return [failed("Directory '%s' does not exist" % rootdir, rootdir)]
    exts = [".csv", ".txt", ".tsv", ".data", ".gz", ".zip", ".asv", ".psv",
            ".scsv", ".hive"]
    out = set()
    for dirname, subdirs, files in os.walk(rootdir):
        for filename in files:
            f = os.path.join(dirname, filename)
            if ("readme" not in filename and
                    ".svm" not in filename and
                    ".json" not in filename and
                    any(filename.endswith(ext) for ext in exts)):
                if filename.endswith(".zip") and f[:-4] in out:
                    continue
                if f + ".zip" in out:
                    out.remove(f + ".zip")
                out.add(param(f))
            else:
                out.add(skipped("Invalid file: '%s'" % f, id=f))
    return out

# Fixture hack. Pair with the return values of get_file_list()
@pytest.fixture()
def f(request):
    return request.param()


#-------------------------------------------------------------------------------
# Run the tests
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("f", get_file_list("h2oai-benchmarks", "Data"),
                         indirect=True)
def test_h2oai_benchmarks(f):
    d = datatable.fread(f)
    assert d.internal.check()



@pytest.mark.parametrize("f", get_file_list("h2o-3", "smalldata"),
                         indirect=True)
def test_h2o3_smalldata(f):
    ignored_files = {
        # Zip files containing >1 files
        os.path.join("gbm_test", "bank-full.csv.zip"),
        os.path.join("jira", "pub-999.zip"),
        os.path.join("parser", "hexdev_497", "airlines_no_first_header.zip"),
        os.path.join("parser", "hexdev_497", "airlines_first_header.zip"),
        os.path.join("parser", "hexdev_497", "airlines_small_csv.zip"),
        os.path.join("prostate", "prostate.bin.csv.zip"),
        # Others
        os.path.join("arff", "folder1", "iris0.csv"),
        os.path.join("jira", "pubdev_2897.csv"),
        os.path.join("junit", "test_parse_mix.csv"),
        os.path.join("merge", "livestock.nuts.csv"),
        os.path.join("merge", "tourism.csv"),
        os.path.join("parser", "column.csv"),
    }
    if any(ff in f for ff in ignored_files):
        pytest.skip("On the ignored files list")
    else:
        params = {}
        if "test_pubdev3589" in f:
            params["fill"] = True
        d0 = datatable.fread(f, **params)
        assert d0.internal.check()


@pytest.mark.parametrize("f", get_file_list("h2o-3", "bigdata", "laptop"),
                         indirect=True)
def test_h2o3_bigdata(f):
    ignored_files = {
        # empty files
        os.path.join("mnist", "t10k-images-idx3-ubyte.gz"),
        os.path.join("mnist", "t10k-labels-idx1-ubyte.gz"),
        os.path.join("mnist", "train-images-idx3-ubyte.gz"),
        os.path.join("mnist", "train-labels-idx1-ubyte.gz"),
        # zip files having more than 1 file inside
        os.path.join("flights-nyc", "delays14.csv.zip"),
        os.path.join("flights-nyc", "flights14.csv.zip"),
        os.path.join("flights-nyc", "weather_delays14.csv.zip"),
        os.path.join("jira", "la1s.wc.arff.txt.zip"),
        os.path.join("jira", "re0.wc.arff.txt.zip"),
        os.path.join("jira", "rotterdam.csv.zip"),
        os.path.join("parser", "hexdev_497", "milsongs_csv.zip"),
    }
    filledna_files = {
        os.path.join("lending-club", "LoanStats3a.csv"),
        os.path.join("lending-club", "LoanStats3b.csv"),
        os.path.join("lending-club", "LoanStats3c.csv"),
        os.path.join("lending-club", "LoanStats3d.csv"),
    }
    if any(ff in f for ff in ignored_files):
        pytest.skip("On the ignored files list")
    else:
        params = {}
        if any(ff in f for ff in filledna_files):
            params["fill"] = True
        d0 = datatable.fread(f, **params)
        assert d0.internal.check()



@pytest.mark.parametrize("f", get_file_list("h2o-3", "fread"),
                         indirect=True)
def test_h2o3_fread(f):
    d0 = datatable.fread(f)
    assert d0.internal.check()
