#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import os
import pytest
import datatable


#-------------------------------------------------------------------------------
# Helper functions
#-------------------------------------------------------------------------------

def get_file_list(rootdir):
    if not os.path.isdir(rootdir):
        pytest.skip("Directory %s does not exist" % rootdir)
    exts = [".csv", ".txt", ".tsv", ".data", ".gz", ".zip", ".asv", ".psv",
            ".scsv", ".hive"]
    out = set()
    for dirname, subdirs, files in os.walk(rootdir):
        for filename in files:
            f = os.path.join(dirname, filename)
            if ("readme" not in filename and
                    any(filename.endswith(ext) for ext in exts)):
                if filename.endswith(".zip") and f[:-4] in out:
                    continue
                if f + ".zip" in out:
                    out.remove(f + ".zip")
                out.add(f)
            else:
                print("Skipping file %s" % f)
    return out


#-------------------------------------------------------------------------------
# Run the tests
#-------------------------------------------------------------------------------

def test_h2oai_benchmarks(dataroot):
    for f in get_file_list(os.path.join(dataroot, "h2oai-benchmarks", "Data")):
        if ".json" in f:
            print("Skipping file %s" % f)
            continue
        print("Reading file %s" % f)
        d = datatable.fread(f)
        # Include `f` in the assert so that the filename is printed in case of
        # a failure
        assert f and d.internal.verify_integrity() is None


def test_h2o3_smalldata(dataroot):
    ignored_files = {
        # seg.faults
        "31081_New_York_City__Hourly_2014.csv",
        "31081_New_York_City__Hourly_2013.csv",
        os.path.join("prostate", "prostate.float.csv.zip"),
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
    for f in get_file_list(os.path.join(dataroot, "h2o-3", "smalldata")):
        if ".svm" in f or any(ff in f for ff in ignored_files):
            print("Skipping file %s" % f)
            continue
        print("Reading file %s" % f)
        params = {}
        if "test_pubdev3589" in f:
            params["fill"] = True
        d0 = datatable.fread(f, **params)
        assert f and d0.internal.verify_integrity() is None



def test_h2o3_bigdata(dataroot):
    ignored_files = {
        # seg.faults
        os.path.join("automl", "data_"),  # many files here
        os.path.join("citibike-nyc", "31081_New_York_City__Hourly_2013.csv"),
        os.path.join("citibike-nyc", "31081_New_York_City__Hourly_2014.csv"),
        os.path.join("usecases", "H20_Rush_New_Dataset_100k_stripped.csv"),
        os.path.join("lending-club", "LoanStats3a.csv"),
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
    files = get_file_list(os.path.join(dataroot, "h2o-3", "bigdata", "laptop"))
    for f in files:
        if ".svm" in f or any(ff in f for ff in ignored_files):
            print("Skipping file %s" % f)
            continue
        print("Reading file %s" % f)
        params = {}
        if any(ff in f for ff in filledna_files):
            params["fill"] = True
        d0 = datatable.fread(f, **params)
        assert f and d0.internal.verify_integrity() is None
