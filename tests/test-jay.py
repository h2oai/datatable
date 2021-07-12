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
import datatable as dt
import math
import os
import pickle
import pytest
import random
import shutil
from datatable.exceptions import DatatableWarning
from datatable.internal import frame_integrity_check
from tests import assert_equals, noop, isview



#-------------------------------------------------------------------------------
# Jay format
#-------------------------------------------------------------------------------

def test_jay_simple(tempfile_jay):
    dt0 = dt.Frame({"A": [-1, 7, 10000, 12],
                    "B": [True, None, False, None],
                    "C": ["alpha", "beta", None, "delta"]})
    dt0.to_jay(tempfile_jay)
    assert os.path.isfile(tempfile_jay)
    with open(tempfile_jay, "rb") as inp:
        assert inp.read(8) == b"JAY1\x00\x00\x00\x00"
    dt1 = dt.Frame(tempfile_jay)
    assert dt1.source == tempfile_jay
    assert_equals(dt0, dt1)


def test_fread(tempfile_jay):
    # Check that Jay format can be read even if the file's extension isn't "jay"
    tempfile_joy = tempfile_jay[:-4] + ".joy"
    try:
        DT = dt.Frame(AA=range(1000000))
        DT.to_jay(tempfile_jay)
        DT.to_jay(tempfile_joy)
        f1 = dt.fread(tempfile_jay)
        f2 = dt.fread(tempfile_joy)
        assert_equals(f1, f2)
        assert f1.source == tempfile_jay
        assert f2.source == tempfile_joy
    finally:
        # The file `tempfile_joy` is memory-mapped as the frame `f2`.
        # So in order to delete `tempfile_joy` we first need to
        # delete `f2`. Otherwise (for instance, on Windows) we won't
        # have permissions to delete this file.
        f2 = None
        os.remove(tempfile_joy)


def test_jay_bytes_object():
    # Make sure that opening a Jay bytes object produces a valid object
    # even after the original bytes object is garbage-collected.
    # See issue #2547
    DT = dt.Frame(["abcd"] * 100000)
    RES = dt.fread(DT.to_jay())
    assert_equals(RES, DT)


def test_jay_empty_string_col(tempfile_jay):
    dt0 = dt.Frame([[1, 2, 3], ["", "", ""]], names=["hogs", "warts"])
    dt0.to_jay(tempfile_jay)
    assert os.path.isfile(tempfile_jay)
    dt1 = dt.Frame(tempfile_jay)
    assert_equals(dt0, dt1)
    assert dt1.source == tempfile_jay


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_jay_view(tempfile_jay, seed):
    random.seed(seed)
    src = [random.normalvariate(0, 1) for n in range(1000)]
    dt0 = dt.Frame({"values": src})
    dt1 = dt0.sort(0)
    assert isview(dt1)
    dt1.to_jay(tempfile_jay)
    assert os.path.isfile(tempfile_jay)
    dt2 = dt.fread(tempfile_jay)
    assert not isview(dt2)
    assert dt2.source == tempfile_jay
    frame_integrity_check(dt1)
    frame_integrity_check(dt2)
    assert dt1.names == dt2.names
    assert dt1.stypes == dt2.stypes
    assert dt1.to_list() == dt2.to_list()


def test_jay_unicode_names(tempfile_jay):
    dt0 = dt.Frame({"py": [1], "ру": [2], "рy": [3], "pу": [4]})
    assert len(set(dt0.names)) == 4
    dt0.to_jay(tempfile_jay)
    assert os.path.isfile(tempfile_jay)
    dt1 = dt.fread(tempfile_jay)
    assert dt0.names == dt1.names
    assert_equals(dt0, dt1)
    assert dt1.source == tempfile_jay


def test_jay_object_columns(tempfile_jay):
    src1 = [1, 2, 3, 4]
    src2 = [(2, 3), (5, 6, 7), 9, {"A": 3}] / dt.obj64
    d0 = dt.Frame(A=src1, B=src2)
    assert d0.stypes == (dt.int32, dt.obj64)
    with pytest.warns(DatatableWarning) as ws:
        d0.to_jay(tempfile_jay)
    assert len(ws) == 1
    assert "Column `B` of type obj64 was not saved" in ws[0].message.args[0]
    d1 = dt.fread(tempfile_jay)
    frame_integrity_check(d1)
    assert d1.names == ("A",)
    assert d1.to_list() == [src1]
    assert d1.source == tempfile_jay


def test_jay_empty_frame(tempfile_jay):
    d0 = dt.Frame()
    d0.to_jay(tempfile_jay)
    assert os.path.isfile(tempfile_jay)
    d1 = dt.fread(tempfile_jay)
    assert d1.shape == (0, 0)
    assert d1.names == tuple()
    assert d1.source == tempfile_jay


def test_jay_all_types(tempfile_jay):
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
    noop(d0.min())
    noop(d0.max())
    assert len(set(d0.stypes)) == d0.ncols
    d0.to_jay(tempfile_jay)
    assert os.path.isfile(tempfile_jay)
    d1 = dt.fread(tempfile_jay)
    assert_equals(d0, d1)
    assert d1.source == tempfile_jay


def test_jay_keys(tempfile_jay):
    d0 = dt.Frame([["ab", "cd", "eee", "coo", "aop"],
                   [1, 2, 3, 4, 5]], names=("x", "y"))
    d0.key = "x"
    assert len(d0.key) == 1
    assert d0.to_list() == [["ab", "aop", "cd", "coo", "eee"],
                            [1, 5, 2, 4, 3]]
    d0.to_jay(tempfile_jay)
    d1 = dt.fread(tempfile_jay)
    assert d1.key == ("x",)
    assert d1.source == tempfile_jay
    assert_equals(d0, d1)


def test_jay_rbound_column(tempfile_jay):
    # See issue #2543
    data = ["loooooooooooooooooooooooong"] * 10000 + ["A"] * 30000
    src = "Z\n%s\n" % "\n".join(data)
    DT = dt.fread(src)
    assert dt.internal.frame_columns_virtual(DT)[0] is True
    DT.to_jay(tempfile_jay)
    RES = dt.fread(tempfile_jay)
    frame_integrity_check(RES)
    assert_equals(RES, dt.Frame(Z=data))


def test_jay_very_wide_frame():
    # See issue 2876
    n = 1_000_000
    DT = dt.Frame([[1]] * n)
    jay = DT.to_jay()
    RES = dt.fread(jay)
    assert RES.shape == (1, n)
    assert RES[0, 0] == 1
    assert RES[0, -1] == 1
    assert RES[0, n//2] == 1


def test_jay_backward_compatibility():
    src = (b"JAY1\x00\x00\x00\x00\x05\x00\x00\x00\x07\x00\x00\x00\n\x00\x00\x00"
           b"\x00\x00\x00\x80\x00\x00\x00\x00\x00\x00\x08@\x00\x00\x00\x00\x00"
           b"\x00\x04@\x9a\x99\x99\x99\x99\x99\x11@\x00\x00\x00\x00\x00\x00\xf8"
           b"\x7f\x00\x00\x00\x00\x03\x00\x00\x00\x08\x00\x00\x00\x0e\x00\x00"
           b"\x00\x0e\x00\x00\x80\x00\x00\x00\x00onethreeeleven\x00\x00\x10\x00"
           b"\x00\x00\x0c\x00\x1c\x00\x08\x00\x10\x00\x00\x00\x04\x00\x0c\x00"
           b"\x00\x00\x18\x00\x00\x00\x04\x00\x00\x00\x00\x00\x00\x00\x03\x00"
           b"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x98\x00"
           b"\x00\x00T\x00\x00\x00\x14\x00\x00\x00\x00\x00\x0e\x004\x003\x00"
           b"\x14\x00\x04\x00,\x00$\x00\x0e\x00\x00\x00H\x00\x00\x00\x00\x00"
           b"\x00\x00\x0e\x00\x00\x00\x00\x00\x00\x000\x00\x00\x00\x00\x00\x00"
           b"\x00\x14\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00"
           b"\x00\x08\x00\x00\x00\x00\x00\x00\x07\x01\x00\x00\x00D\x00\x00\x00"
           b"\xd0\xff\xff\xff\x00\x00\x00\x07\x10\x00\x00\x00\x00\x00\x00"
           b"\x00 \x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00"
           b"\x08\x00\x00\x00\x00\x00\x00\x06\x01\x00\x00\x00B\x00\x00\x00\x10"
           b"\x00(\x00'\x00\x08\x00\x00\x00 \x00\x18\x00\x07\x00\x10\x00\x00\x00"
           b"\x00\x00\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00\x00"
           b"\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x08\x00\x00\x00"
           b"\x00\x00\x00\x03\x01\x00\x00\x00A\x00\x00\x00\xf8\x00\x00\x00\x00"
           b"\x00\x00\x00\x00\x00\x00\x001JAY")
    DT = dt.fread(src)
    assert DT.shape == (4, 3)
    assert DT.types == [dt.Type.int32, dt.Type.float64, dt.Type.str32]
    assert DT.to_list() == [[5, 7, 10,  None],
                            [3.0, 2.5, 4.4, None],
                            ['one', 'three', 'eleven', None]]



#-------------------------------------------------------------------------------
# pickling
#-------------------------------------------------------------------------------

def test_pickle(tempfile):
    DT = dt.Frame(A=range(10), B=list("ABCDEFGHIJ"), C=[5.5]*10)
    with open(tempfile, 'wb') as out:
        pickle.dump(DT, out)
    with open(tempfile, 'rb') as inp:
        DT2 = pickle.load(inp)
    frame_integrity_check(DT2)
    assert DT.to_list() == DT2.to_list()
    assert DT.names == DT2.names
    assert DT.stypes == DT2.stypes
    assert DT2.source == "<pickle>"


def test_pickle2(tempfile):
    DT = dt.Frame([[1, 2, 3, 4],
                   [5, 6, 7, None],
                   [10, 0, None, -5],
                   [1000, 10000, 10000000, 10**18],
                   [True, None, None, False],
                   [None, 3.14, 2.99, 1.6923e-18],
                   [134.23891, 901239.00001, 2.5e+300, math.inf],
                   ["Bespectable", None, "z e w", "1"],
                   ["We", "the", "people", "!"]],
                  stypes=[dt.int8, dt.int16, dt.int32, dt.int64, dt.bool8,
                          dt.float32, dt.float64, dt.str32, dt.str64])
    with open(tempfile, 'wb') as out:
        pickle.dump(DT, out)
    with open(tempfile, 'rb') as inp:
        DT2 = pickle.load(inp)
    frame_integrity_check(DT2)
    assert DT.names == DT2.names
    assert DT.stypes == DT2.stypes
    assert DT.to_list() == DT2.to_list()


def test_pickle_keyed_frame(tempfile):
    DT = dt.Frame(A=list("ABCD"), B=[12.1, 34.7, 90.1238, -23.239045])
    DT.key = "A"
    with open(tempfile, 'wb') as out:
        pickle.dump(DT, out)
    with open(tempfile, 'rb') as inp:
        DT2 = pickle.load(inp)
    frame_integrity_check(DT2)
    assert DT.names == DT2.names
    assert DT.stypes == DT2.stypes
    assert DT.to_list() == DT2.to_list()
    assert DT.key == DT2.key
    assert DT2.source == "<pickle>"
