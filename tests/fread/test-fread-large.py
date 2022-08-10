#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2020 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------
# Optional tests (enabled only if DT_LARGE_TESTS_ROOT environment variable is
# defined): read a large number of external files of various sizes, including
# some very big files.
#-------------------------------------------------------------------------------
import os
import pytest
import datatable as dt
import warnings
import zipfile
from datatable.internal import frame_integrity_check
from tests import assert_equals, find_file, is_ppc64

env_coverage = "DTCOVERAGE"
root_env_name = "DT_LARGE_TESTS_ROOT"
MEMORY_LIMIT = 2 * (1 << 30)   # 2Gb


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
def get_file_list(*path, skip=None):
    if os.environ.get(env_coverage, None):
        return [skipped("Large tests disabled in COVERAGE mode")]
    d = os.environ.get(root_env_name, "")
    if d == "":
        return [skipped("%s is not defined" % root_env_name)]
    if not os.path.isdir(d):
        return [failed("Directory '%s' (%s) does not exist"
                       % (d, root_env_name), id=d)]
    rootdir = os.path.join(d, *path)
    if not os.path.isdir(rootdir):
        return [failed("Directory '%s' does not exist" % rootdir, rootdir)]
    good_extensions = [".csv", ".txt", ".tsv", ".data", ".gz", ".zip", ".asv",
                       ".psv", ".scsv", ".hive"]
    bad_extensions = {".gif", ".jpg", ".jpeg", ".pdf", ".svg"}
    if skip:
        rem = set(os.path.join(rootdir, f) for f in skip)
    else:
        rem = set()
    out = set()
    for dirname, _, files in os.walk(rootdir):
        for filename in files:
            f = os.path.join(dirname, filename)
            try:
                ext = filename[filename.rindex("."):]
            except ValueError:
                ext = ""
            if ext in bad_extensions:
                continue
            if f in rem:
                continue
            if f.endswith(".dat.gz") or f.endswith(".tar.gz"):
                continue
            if ("readme" not in filename.lower() and
                    ".svm" not in filename and
                    ".json" not in filename and
                    ext in good_extensions):
                if filename.endswith(".zip") and f[:-4] in out:
                    continue
                if f + ".zip" in out:
                    out.remove(f + ".zip")
                if f + ".gz" in out:
                    out.remove(f + ".gz")
                try:
                    with open(f, "rb"):
                        pass
                    out.add(param(f))
                except Exception as e:
                    # out.add(skipped("%s: '%s'" % (e.__class__.__name__, f), id=f))
                    pass
            else:
                # out.add(skipped("Invalid file: '%s'" % f, id=f))
                pass
    return out



# Fixture hack. Pair with the return values of get_file_list()
@pytest.fixture()
def f(request):
    return request.param()



#-------------------------------------------------------------------------------
# Run the tests
#-------------------------------------------------------------------------------


@pytest.mark.usefixtures("win_only", "release_only")
def test_fread_4gb_jay(tempfile_jay):
    size = 4 * 10**9
    DT = dt.Frame([True])
    DT.nrows = size
    DT.to_jay(tempfile_jay)
    assert os.path.getsize(tempfile_jay) > size
    DT = dt.fread(tempfile_jay)
    frame_integrity_check(DT)
    assert DT.nrows == size
    assert DT.stype == dt.bool8


@pytest.mark.usefixtures("win_only", "release_only")
def test_fread_4gb_csv(tempfile_csv):
    size = 4 * 10**9
    DT = dt.Frame([True])
    DT.nrows = size
    DT.to_csv(tempfile_csv)
    assert os.path.getsize(tempfile_csv) > size
    # This part of the test fails with the "bad allocation" exception
    # on AppVeyor, because its nodes only have 6 Gb's of RAM.
    # del DT
    # DT = dt.fread(tempfile_csv)
    # frame_integrity_check(DT)
    # assert DT.nrows == size
    # assert DT.stype == dt.bool8


@pytest.mark.parametrize("f", get_file_list("h2oai-benchmarks", "Data"),
                         indirect=True)
def test_h2oai_benchmarks(f):
    try:
        d = dt.fread(f, memory_limit=MEMORY_LIMIT)
        frame_integrity_check(d)
    except zipfile.BadZipFile:
        pytest.skip("Bad zip file error")



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
        os.path.join("smalldata", "images", "cat_dog_tiny_thumbnails.zip"),
        # Others
        os.path.join("arff", "folder1", "iris0.csv"),
        os.path.join("jira", "pubdev_2897.csv"),
        os.path.join("jira", "runit_pubdev_3590_unexpected_column.csv"),
        os.path.join("junit", "iris.xls.zip"),
        os.path.join("junit", "test_parse_mix.csv"),
        os.path.join("junit", "arff", "jm1_arff.txt"),
        os.path.join("junit", "arff", "jm1.arff.txt"),
        os.path.join("merge", "livestock.nuts.csv"),
        os.path.join("merge", "tourism.csv"),
        os.path.join("parser", "column.csv"),
    }
    if any(ff in f for ff in ignored_files):
        pytest.skip("On the ignored files list")
    else:
        params = {}
        if is_ppc64():
            params["nthreads"] = 8
            pytest.skip("Fread tests disabled on PPC64")
            return
        if "test_pubdev3589" in f:
            params["sep"] = "\n"
        if ("single_quotes_mixed.csv" in f or
            "single_quotes_with_escaped_quotes.csv" in f or
            "single_quotes_with_escaped_quotes_custom_escapechar.csv" in f):
            params["quotechar"] = "'"
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            DT = dt.fread(f, **params)
            frame_integrity_check(DT)


@pytest.mark.parametrize("f", get_file_list("h2o-3", "bigdata", "laptop"),
                         indirect=True)
def test_h2o3_bigdata(f):
    ignored_files = {
        # Feather files
        os.path.join("ipums_feather.gz"),
        # empty files
        os.path.join("mnist", "t10k-images-idx3-ubyte.gz"),
        os.path.join("mnist", "t10k-labels-idx1-ubyte.gz"),
        os.path.join("mnist", "train-images-idx3-ubyte.gz"),
        os.path.join("mnist", "train-labels-idx1-ubyte.gz"),
        # ARFF files
        os.path.join("parser", "anARFFFile.txt"),
        # zip files having more than 1 file inside
        os.path.join("flights-nyc", "delays14.csv.zip"),
        os.path.join("flights-nyc", "flights14.csv.zip"),
        os.path.join("flights-nyc", "weather_delays14.csv.zip"),
        os.path.join("images", "demo_disney_data.zip"),  # jpegs...
        os.path.join("jira", "la1s.wc.arff.txt.zip"),
        os.path.join("jira", "re0.wc.arff.txt.zip"),
        os.path.join("jira", "rotterdam.csv.zip"),
        os.path.join("parser", "hexdev_497", "milsongs_csv.zip"),
        os.path.join("glm", "GLM_model_python_1543520565753_1.zip"),
        os.path.join("glm", "GLM_model_python_1543520565753_3.zip"),
        os.path.join("glm", "GLM_model_python_1544561074878_1.zip"),
        # requires `comment` parameter
        os.path.join("new-poker-hand.full.311M.txt"),
        # files with 36M columns
        os.path.join("testng", "newsgroup_train1.csv"),
        os.path.join("testng", "newsgroup_validation1.csv"),
        # broken CRC zip files
        os.path.join("jira", "tenThousandCat50C.csv.zip"),
        os.path.join("jira", "tenThousandCat100C.csv.zip"),
        os.path.join("parser", "year2005.csv.gz"),
    }
    filledna_files = {
        os.path.join("lending-club", "LoanStats3a.csv"),
        os.path.join("lending-club", "LoanStats3b.csv"),
        os.path.join("lending-club", "LoanStats3c.csv"),
        os.path.join("lending-club", "LoanStats3d.csv"),
        os.path.join("LoanStats3a.csv"),
        os.path.join("LoanStats3b.csv"),
        os.path.join("LoanStats3c.csv"),
        os.path.join("LoanStats3d.csv"),
        os.path.join("Kaggle_Product_BO_Test_v2.csv.zip"),
        os.path.join("Kaggle_Product_BO_Training_v2.csv.zip"),
        os.path.join("model_selection", "backwardBinomial200C50KRows.csv"),
        os.path.join("model_selection", "backwardBinomial200C50KRowsWeighted.csv"),
        os.path.join("model_selection", "gaussian_500Cols_500KRows.csv"),
        os.path.join("model_selection", "maxrglm100Cols50KRowsWeighted.csv"),
    }
    if any(ff in f for ff in ignored_files):
        pytest.skip("On the ignored files list")
        return

    params = {"memory_limit": MEMORY_LIMIT}
    if is_ppc64():
        params["nthreads"] = 8
        pytest.skip("Fread tests disabled on PPC64")
        return
    if any(ff in f for ff in filledna_files):
        params["fill"] = True
    if "imagenet/cat_dog_mouse.tgz" in f:
        f = os.path.join(f, "cat_dog_mouse.csv")

    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        DT = dt.fread(f, **params)
        frame_integrity_check(DT)



@pytest.mark.parametrize("f", get_file_list("h2o-3", "fread", skip={"1206FUT.txt"}),
                         indirect=True)
def test_fread_all(f):
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        DT = dt.fread(f)
        frame_integrity_check(DT)


def test_fread_1206FUT():
    # Based on R test 901.*
    f = find_file("h2o-3", "fread", "1206FUT.txt")
    d0 = dt.fread(f, strip_whitespace=True)
    d1 = dt.fread(f, strip_whitespace=False)
    frame_integrity_check(d0)
    frame_integrity_check(d1)
    assert d0.shape == d1.shape == (308, 21)
    assert d0.names == d1.names
    assert d0.stypes == d1.stypes
    p0 = d0.to_list()
    p1 = d1.to_list()
    assert p0[:-1] == p1[:-1]
    assert p0[-1] == [""] * 10 + ["A"] * 298
    assert p1[-1] == [" "] * 10 + ["A"] * 298


def test_fread_maxnrows_with_large_file():
    f = find_file("h2o-3", "bigdata", "laptop", "airlines_all.05p.csv")
    d0 = dt.fread(f, max_nrows=111)
    frame_integrity_check(d0)
    assert d0.nrows == 111


def test_fread_large_file_with_string_na_column():
    if not os.environ.get(root_env_name, ""):
        pytest.skip("%s is not defined" % root_env_name)
    # Create a CSV file with string column > 2GB in size, and containing an NA value
    value = "Abcdefghij" * 100
    target_size = 2500 * 10**6
    nrows = target_size // len(value)
    src = [value] * nrows
    na_pos = nrows // 2
    src[na_pos] = "NA"
    srctext = "\n".join(src)
    DT = dt.fread(text=srctext, na_strings=["NA"], header=False)
    assert DT.shape == (nrows, 1)
    assert DT.stypes == (dt.str64,)
    assert DT[na_pos, 0] is None
    frame_integrity_check(DT)
