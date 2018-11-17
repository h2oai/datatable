#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import os
import shutil
import pytest
import random
import tempfile
import datatable as dt
from datatable import DatatableWarning
from tests import assert_equals



#-------------------------------------------------------------------------------
# NFF format
#-------------------------------------------------------------------------------

def test_save_and_load():
    dir1 = tempfile.mkdtemp()
    dt0 = dt.Frame({"A": [1, 7, 100, 12],
                    "B": [True, None, False, None],
                    "C": ["alpha", "beta", None, "delta"]})
    dt0.save(dir1, format="nff")
    dt1 = dt.open(dir1)
    assert_equals(dt0, dt1)
    shutil.rmtree(dir1)


def test_empty_string_col():
    """
    Test that Frame with an empty string column can be saved/opened.
    See #604
    """
    dir1 = tempfile.mkdtemp()
    dt0 = dt.Frame([[1, 2, 3], ["", "", ""]])
    dt0.save(dir1, format="nff")
    dt1 = dt.open(dir1)
    assert_equals(dt0, dt1)
    shutil.rmtree(dir1)


def test_issue627():
    """Test saving Frame with unicode file names"""
    dir1 = tempfile.mkdtemp()
    dt0 = dt.Frame({"py": [1], "ру": [2], "рy": [3], "pу": [4]})
    assert dt0.shape == (1, 4)
    dt0.save(dir1, format="nff")
    dt1 = dt.open(dir1)
    assert_equals(dt0, dt1)
    shutil.rmtree(dir1)


def test_obj_columns(tempdir):
    src1 = [1, 2, 3, 4]
    src2 = [(2, 3), (5, 6, 7), 9, {"A": 3}]
    d0 = dt.Frame([src1, src2], names=["A", "B"])
    d0.internal.check()
    assert d0.ltypes == (dt.ltype.int, dt.ltype.obj)
    assert d0.shape == (4, 2)
    with pytest.warns(DatatableWarning) as ws:
        d0.save(tempdir, format="nff")
    assert len(ws) == 1
    assert "Column 'B' of type obj64 was not saved" in ws[0].message.args[0]
    del d0
    d1 = dt.open(tempdir)
    d1.internal.check()
    assert d1.shape == (4, 1)
    assert d1.names == ("A", )
    assert d1.topython() == [src1]


def test_save_view(tempdir):
    dt0 = dt.Frame([4, 0, -2, 3, 17, 2, 0, 1, 5], names=["fancy"])
    dt1 = dt0.sort(0)
    assert dt1.internal.isview
    dt1.internal.check()
    dt1.save(tempdir, format="nff")
    dt2 = dt.open(tempdir)
    assert not dt2.internal.isview
    dt2.internal.check()
    assert dt2.names == dt1.names
    assert dt2.topython() == dt1.topython()



#-------------------------------------------------------------------------------
# Jay format
#-------------------------------------------------------------------------------

def test_jay_simple(tempfile):
    dt0 = dt.Frame({"A": [-1, 7, 10000, 12],
                    "B": [True, None, False, None],
                    "C": ["alpha", "beta", None, "delta"]})
    dt0.save(tempfile, format="jay")
    assert os.path.isfile(tempfile)
    with open(tempfile, "rb") as inp:
        assert inp.read(8) == b"JAY1\x00\x00\x00\x00"
    dt1 = dt.open(tempfile)
    assert_equals(dt0, dt1)


def test_jay_empty_string_col(tempfile):
    dt0 = dt.Frame([[1, 2, 3], ["", "", ""]], names=["hogs", "warts"])
    dt0.save(tempfile)
    assert os.path.isfile(tempfile)
    dt1 = dt.open(tempfile)
    assert_equals(dt0, dt1)


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_jay_view(tempfile, seed):
    random.seed(seed)
    src = [random.normalvariate(0, 1) for n in range(1000)]
    dt0 = dt.Frame({"values": src})
    dt1 = dt0.sort(0)
    assert dt1.internal.isview
    dt1.save(tempfile)
    assert os.path.isfile(tempfile)
    dt2 = dt.open(tempfile)
    assert not dt2.internal.isview
    dt1.internal.check()
    dt2.internal.check()
    assert dt1.names == dt2.names
    assert dt1.stypes == dt2.stypes
    assert dt1.topython() == dt2.topython()


def test_jay_unicode_names(tempfile):
    dt0 = dt.Frame({"py": [1], "ру": [2], "рy": [3], "pу": [4]})
    assert len(set(dt0.names)) == 4
    dt0.save(tempfile)
    assert os.path.isfile(tempfile)
    dt1 = dt.open(tempfile)
    assert dt0.names == dt1.names
    assert_equals(dt0, dt1)


def test_jay_object_columns(tempfile):
    src1 = [1, 2, 3, 4]
    src2 = [(2, 3), (5, 6, 7), 9, {"A": 3}]
    d0 = dt.Frame([src1, src2], names=["A", "B"])
    assert d0.stypes == (dt.int8, dt.obj64)
    with pytest.warns(DatatableWarning) as ws:
        d0.save(tempfile)
    assert len(ws) == 1
    assert "Column `B` of type obj64 was not saved" in ws[0].message.args[0]
    d1 = dt.open(tempfile)
    d1.internal.check()
    assert d1.names == ("A",)
    assert d1.topython() == [src1]


def test_jay_empty_frame(tempfile):
    d0 = dt.Frame()
    d0.save(tempfile)
    assert os.path.isfile(tempfile)
    d1 = dt.open(tempfile)
    assert d1.shape == (0, 0)
    assert d1.names == tuple()


def test_jay_all_types(tempfile):
    d0 = dt.Frame([[True, False, None, True, True],
                   [None, 1, -9, 12, 3],
                   [4, 1346, 999, None, None],
                   [591, 0, None, -395734, 19384709],
                   [None, 777, 1093487019384, -384, None],
                   [2.987, 3.45e-24, -0.189134e+12, 45982.1, None],
                   [39408.301, 9.459027045e-125, 4.4508e+222, None, 3.14159],
                   ["Life", "Liberty", "and", "Pursuit of Happiness", None],
                   ["кохайтеся", "чорнобриві", ",", "та", "не з москалями"]
                   ],
                  stypes=[dt.bool8, dt.int8, dt.int16, dt.int32, dt.int64,
                          dt.float32, dt.float64, dt.str32, dt.str64],
                  names=["b8", "i8", "i16", "i32", "i64", "f32", "f64",
                         "s32", "s64"])
    # Force calculation of mins and maxs, so that they get saved into Jay
    d0.min(), d0.max()
    assert len(set(d0.stypes)) == d0.ncols
    d0.save(tempfile)
    assert os.path.isfile(tempfile)
    d1 = dt.open(tempfile)
    assert_equals(d0, d1)


def test_jay_keys(tempfile):
    d0 = dt.Frame([["ab", "cd", "eee", "coo", "aop"],
                   [1, 2, 3, 4, 5]], names=("x", "y"))
    d0.key = "x"
    assert len(d0.key) == 1
    assert d0.topython() == [["ab", "aop", "cd", "coo", "eee"],
                             [1, 5, 2, 4, 3]]
    d0.save(tempfile)
    d1 = dt.open(tempfile)
    assert d1.key == ("x",)
    assert_equals(d0, d1)
