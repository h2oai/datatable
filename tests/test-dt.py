#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2021 H2O.ai
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
import pytest
import random
import re
import subprocess
import sys
import time
from collections import namedtuple
from datatable import stype, ltype
from datatable.internal import frame_columns_virtual, frame_integrity_check
from datatable.lib import core
from tests import (list_equals, noop, isview, assert_equals,
                   skip_on_jenkins, cpp_test, get_core_tests)


#-------------------------------------------------------------------------------
# Prepare fixtures & helper functions
#-------------------------------------------------------------------------------

@pytest.fixture()
def dt0():
    return dt.Frame([
        [2, 7, 0, 0],
        [True, False, False, True],
        [1, 1, 1, 1],
        [0.1, 2, -4, 4.4],
        [None, None, None, None],
        [0, 0, 0, 0],
        ["1", "2", "hello", "world"],
    ], names=list("ABCDEFG"))


def assert_valueerror(frame, rows, error_message):
    with pytest.raises(ValueError) as e:
        noop(frame[rows, :])
    assert isinstance(e.value, dt.exceptions.DtException)
    assert error_message in str(e.value)



#-------------------------------------------------------------------------------
# Generic tests
#-------------------------------------------------------------------------------

def test_platform():
    # This test performs only minimal checks, and also outputs diagnostic
    # information about the platform being run.
    print()
    print("argv           = %r" % sys.argv)
    print("executable     = %s" % sys.executable)
    print("platform       = %s" % sys.platform)
    print("version        = %s" % sys.version.replace("\n", " "))
    print("implementation = %s" % sys.implementation)
    print("int_info   = %s" % (sys.int_info, ))
    print("maxsize    = %s" % (sys.maxsize, ))
    print("maxunicode = %s" % (sys.maxunicode, ))
    print("float_info = %s" % (sys.float_info, ))
    print("hash_info  = %s" % (sys.hash_info, ))
    assert sys.byteorder == "little"
    assert sys.maxsize == 2**63 - 1



@skip_on_jenkins
def test_dt_loadtime(nocov):
    # Check that datatable's loading time is not too big. At the time of writing
    # this test, on a MacBook Pro laptop, the timings were the following:
    #     t_python:    0.0505s
    #     t_smtplib:   0.0850s
    #     t_datatable: 0.1335s
    #     ratio:  2.07 - 2.42
    def time_code_execution(code):
        t0 = time.time()
        out = subprocess.check_output([sys.executable, "-c", code])  # nosec
        assert out == b""
        return time.time() - t0

    attempts = 0
    while attempts < 3:
        attempts += 1
        tpy = time_code_execution("")
        t_smtplib = time_code_execution("import smtplib")
        t_datatable = time_code_execution("import datatable")
        print()
        print("t_python:    %.4fs" % tpy)
        print("t_smtplib:   %.4fs  (%.4fs)" % (t_smtplib, t_smtplib - tpy))
        print("t_datatable: %.4fs  (%.4fs)" % (t_datatable, t_datatable - tpy))
        if t_datatable < tpy or t_smtplib < tpy:
            continue
        ratio = (t_datatable - tpy) / (t_smtplib - tpy)
        print("Ratio: %.6f" % ratio)
        if ratio < 4:
            return
    assert ratio < 5


def test_dt_dependencies():
    # This test checks how many dependencies `datatable` needs to load at
    # startup. At the time of writing this test, the count was 168 (as of
    # Feb 2020: 86).
    n = subprocess.check_output([sys.executable, "-c",
                                 "import sys; "
                                 "set1 = set(sys.modules); "
                                 "import datatable; "
                                 "set2 = set(sys.modules); "
                                 "assert 'numpy' not in set2; "
                                 "print(len(set2-set1), end='')"])
    assert int(n) < 120


def test_dt_version():
    assert dt.__version__
    assert isinstance(dt.__version__, str)
    assert dt.build_info
    assert isinstance(dt.build_info.build_date, str)
    assert isinstance(dt.build_info.build_mode, str)
    assert isinstance(dt.build_info.compiler, str)
    assert isinstance(dt.build_info.git_revision, str)
    assert isinstance(dt.build_info.git_branch, str)
    assert isinstance(dt.build_info.git_date, str)
    assert isinstance(dt.build_info.git_diff, str)
    assert isinstance(dt.build_info.version, str)
    if "DTCOVERAGE" not in os.environ:
        assert dt.build_info.build_date
        assert dt.build_info.git_revision
        assert dt.build_info.git_branch
        assert dt.build_info.git_date
        assert dt.build_info.version
        assert len(dt.build_info.git_revision) == 40
        assert dt.build_info.version == dt.__version__
    assert dt.build_info.compiler.lower() != "unknown"


def test_dt_help():
    # Issue #1931: verify that rendering help does not throw an error
    import pydoc
    from io import StringIO
    try:
        original_pager = pydoc.pager
        pydoc.pager = pydoc.plainpager
        buf = sys.stdout = StringIO()
        help(dt)
        out = buf.getvalue()
        assert len(out) > 30000
        assert "class Frame(" in out
    finally:
        # Restore the original stdout / pydoc pager
        sys.stdout = sys.__stdout__
        pydoc.pager = original_pager



def test_dt_properties(dt0):
    assert isinstance(dt0, dt.Frame)
    frame_integrity_check(dt0)
    assert dt0.source is None
    assert dt0.meta is None
    assert dt0.nrows == 4
    assert dt0.ncols == 7
    assert dt0.shape == (4, 7)
    assert dt0.ndims == 2
    assert dt0.names == ("A", "B", "C", "D", "E", "F", "G")
    assert dt0.ltypes == (ltype.int, ltype.bool, ltype.int, ltype.real,
                          ltype.void, ltype.int, ltype.str)
    assert dt0.stypes == (stype.int32, stype.bool8, stype.int8, stype.float64,
                          stype.void, stype.int8, stype.str32)
    assert sys.getsizeof(dt0) > 500


def test_sizeof():
    DT1 = dt.Frame(A=["foo"])
    DT2 = dt.Frame(A=["foo" * 1001])
    assert sys.getsizeof(DT2) - sys.getsizeof(DT1) == 3000


@cpp_test
def test_check_suites():
    # If this test fails (found an extra suite), then a new test must be
    # created in order to cover that suite. See test_core_coverage()
    # above for an example.
    found = core.get_test_suites()
    expected = ["parallel", "progress", "coverage", "fread"]
    assert set(found) == set(expected)


def test_dt_getitem(dt0):
    dt1 = dt0[:, 0]
    assert dt1.shape == (4, 1)
    assert dt1.names == ("A", )
    dt1 = dt0[(..., 4)]
    assert dt1.shape == (4, 1)
    assert dt1.names == ("E", )
    elem2 = dt0[0, 1]
    assert elem2 is True
    with pytest.raises(TypeError) as e:
        noop(dt0[0, 1, 2, 3])
    assert "Invalid item at position 2 in DT[i, j, ...] call" == str(e.value)


def test_dt_getitem2(dt0):
    assert_equals(dt0["A"], dt0[:, "A"])
    assert_equals(dt0[1], dt0[:, 1])


def test_dt_getitem3():
    DT = dt.Frame([3])
    a = DT[0]
    assert a.to_list() == [[3]]
    del a  # should not emit an error


def test_frame_as_iterable(dt0):
    assert iter(dt0)
    assert iter(dt0).__length_hint__() == dt0.ncols
    for i, col in enumerate(dt0):
        assert_equals(col, dt0[:, i])
    for i, col in enumerate(reversed(dt0)):
        assert_equals(col, dt0[:, -i-1])


def test_frame_star_expansion(dt0):
    def foo(*args):
        for arg in args:
            assert isinstance(arg, dt.Frame)
            assert arg.shape == (dt0.nrows, 1)
            frame_integrity_check(arg)

    foo(*dt0)


def test_frame_as_mapping(dt0):
    assert dt0.keys() == dt0.names
    i = 0
    for name, col in dict(dt0).items():
        assert name == dt0.names[i]
        assert_equals(col, dt0[i])
        i += 1


def test_frame_singlestar_expansion(dt0):
    def foo1(*args):
        for item in args:
            assert isinstance(item, dt.Frame)
            assert item.shape == (dt0.nrows, 1)
            frame_integrity_check(item)

    foo1(*dt0)


def test_frame_singlestar_expansion2(dt0):
    dt0list = [*dt0]
    assert len(dt0list) == dt0.ncols
    for i, col in enumerate(dt0list):
        assert_equals(col, dt0[:, i])


def test_frame_doublestar_expansion(dt0):
    def foo(**kwds):
        for name, col in kwds.items():
            assert isinstance(name, str)
            assert isinstance(col, dt.Frame)
            assert col.shape == (dt0.nrows, 1)
            assert name == col.names[0]
            frame_integrity_check(col)

    foo(**dt0)


def test_issue1406(dt0):
    with pytest.raises(ValueError) as e:
        noop(dt0[tuple()])
    assert "Invalid tuple of size 0 used as a frame selector" in str(e.value)
    with pytest.raises(ValueError) as e:
        noop(dt0[3,])
    assert "Invalid tuple of size 1 used as a frame selector" in str(e.value)


def test_warnings_as_errors():
    # See issue #2005
    import warnings
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        try:
            dt.fread("A,A,A\n1,2,3")
        except dt.exceptions.DatatableWarning as e:
            assert "Duplicate column names found" in str(e)
            assert "SystemError" not in str(e)
            assert e.__cause__ is None


def test_names_deduplication():
    with pytest.warns(dt.exceptions.DatatableWarning):
        DT = dt.Frame(names=["A", "A01", "A01"])
        assert DT.names == ("A", "A01", "A2")



def test__len__():
    DT = dt.Frame(A=[1, 2], B=[3, 7], D=["a", 'n'], V=[1.1, None])
    assert len(DT) == 4
    assert DT.__len__() == 4
    with pytest.raises(TypeError):
        assert DT.__len__(3)


def test_collections():
    DT = dt.Frame()
    if sys.version_info >= (3, 7):
        import collections.abc
        assert isinstance(DT, collections.abc.Sized)
        assert isinstance(DT, collections.abc.Iterable)
        assert isinstance(DT, collections.abc.Reversible)
    else:
        import collections
        assert isinstance(DT, collections.Sized)
        assert isinstance(DT, collections.Iterable)
        assert isinstance(DT, collections.Reversible)



@pytest.mark.parametrize("testname", get_core_tests("coverage"), indirect=True)
def test_core_coverage(testname):
    # Run all core tests in suite "coverage"
    core.run_test("coverage", testname)



#-------------------------------------------------------------------------------
# Run several random attacks on a datatable as a whole
#-------------------------------------------------------------------------------

# To pick up attacks based on the corresponding weights, random attacker
# uses random.choices(), introduced in Python 3.6.
@pytest.mark.usefixtures("numpy")
def test_random_attack():
    import subprocess
    script = os.path.join(os.path.dirname(__file__),
                          "..", "tests_random", "continuous.py")
    proc = subprocess.Popen([sys.executable, script, "-v", "-n 5"],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = proc.communicate(timeout=100)
    assert "FAIL" not in out.decode()
    assert not err



#-------------------------------------------------------------------------------
# Types
#-------------------------------------------------------------------------------

def test_dt0_types(dt0):
    T = dt.Type
    assert dt0.types == \
        [T.int32, T.bool8, T.int8, T.float64, T.void, T.int8, T.str32]


def test_empty_frame_types():
    DT = dt.Frame([])
    assert DT.types == []
    DT.nrows = 11
    assert DT.types == []


def test_modify_types_list():
    DT = dt.Frame(A=[1, 4], B=['g', 'r'])
    assert DT.types == [dt.Type.int32, dt.Type.str32]
    types = DT.types
    types[0] = 42
    assert DT.types == [dt.Type.int32, dt.Type.str32]


def test_dt0_type(dt0):
    assert dt0[0].type == dt.Type.int32
    assert dt0[1].type == dt.Type.bool8
    assert dt0[2].type == dt.Type.int8
    assert dt0[3].type == dt.Type.float64
    assert dt0[4].type == dt.Type.void
    assert dt0[5].type == dt.Type.int8
    assert dt0[6].type == dt.Type.str32
    assert dt0[:, [2, 5]].type == dt.Type.int8


def test_type_empty_frame():
    DT = dt.Frame()
    assert DT.type is None
    DT.nrows = 3
    assert DT.type is None


def test_type_invalid(dt0):
    msg = "The type of column 'B' is bool8, which is different from " \
          "the type of the previous column"
    with pytest.raises(dt.exceptions.InvalidOperationError, match=msg):
        assert dt0.type




#-------------------------------------------------------------------------------
# Stype
#-------------------------------------------------------------------------------

def test_dt_stype(dt0):
    assert dt0[0].stype == stype.int32
    assert dt0[1].stype == stype.bool8
    assert dt0[:, [2, 5]].stype == stype.int8
    assert dt0[-1].stype == stype.str32


def test_dt_stype_heterogenous(dt0):
    with pytest.raises(dt.exceptions.InvalidOperationError) as e:
        noop(dt0.stype)
    assert ("The stype of column 'B' is bool8, which is different "
            "from the stype of the previous column" in str(e.value))


#-------------------------------------------------------------------------------
# Meta information
#-------------------------------------------------------------------------------

def test_meta():
    DT = dt.Frame([[3, 1, 4, 1, 5], ["three", "one", "four", "one", "five"]])
    assert DT.meta is None
    DT.meta = {"C0" : "numbers"}
    assert DT.meta == {"C0" : "numbers"}
    DT.meta["C1"] = "strings"
    DT1 = DT.copy()
    assert DT.meta == {"C0" : "numbers", "C1" : "strings"}
    assert DT.meta == DT1.meta
    frame_integrity_check(DT)
    frame_integrity_check(DT1)


#-------------------------------------------------------------------------------
# Resize rows
#-------------------------------------------------------------------------------

def test_resize_rows_api():
    f0 = dt.Frame([20])
    f0.nrows = 3
    f0.nrows = 5
    frame_integrity_check(f0)
    assert f0.to_list() == [[20, None, None, None, None]]


def test_resize_rows0():
    f0 = dt.Frame(range(10), stype=dt.int32)
    f0.nrows = 6
    frame_integrity_check(f0)
    assert f0.shape == (6, 1)
    assert f0.stypes == (dt.int32,)
    assert f0.to_list() == [[0, 1, 2, 3, 4, 5]]
    f0.nrows = 12
    frame_integrity_check(f0)
    assert f0.shape == (12, 1)
    assert f0.stypes == (dt.int32,)
    assert f0.to_list() == [[0, 1, 2, 3, 4, 5] + [None] * 6]
    f0.nrows = 1
    frame_integrity_check(f0)
    assert f0.shape == (1, 1)
    assert f0.stypes == (dt.int32,)
    assert f0[0, 0] == 0
    f0.nrows = 20
    frame_integrity_check(f0)
    assert f0.shape == (20, 1)
    assert f0.stypes == (dt.int32,)
    assert f0.to_list() == [[0] + [None] * 19]


def test_resize_rows1():
    srcs = [[True], [5], [14.3], ["fooga"], ["zoom"]]
    stypes = (dt.bool8, dt.int64, dt.float64, dt.str32, dt.str64)
    f0 = dt.Frame(srcs, stypes=stypes)
    frame_integrity_check(f0)
    assert f0.shape == (1, 5)
    assert f0.stypes == stypes
    f0.nrows = 7
    frame_integrity_check(f0)
    assert f0.shape == (7, 5)
    assert f0.stypes == stypes
    assert f0.to_list() == [src + [None] * 6 for src in srcs]
    f0.nrows = 20
    frame_integrity_check(f0)
    assert f0.shape == (20, 5)
    assert f0.stypes == stypes
    assert f0.to_list() == [src + [None] * 19 for src in srcs]
    f0.nrows = 0
    frame_integrity_check(f0)
    assert f0.shape == (0, 5)
    assert f0.stypes == stypes
    assert f0.to_list() == [[]] * 5


def test_resize_rows_nastrs():
    f0 = dt.Frame(["foo", None, None, None, "gar"])
    f0.nrows = 3
    assert f0.to_list() == [["foo", None, None]]
    f0.nrows = 10
    frame_integrity_check(f0)
    assert f0.to_list() == [["foo"] + [None] * 9]


def test_resize_view_slice():
    f0 = dt.Frame(range(100))
    f1 = f0[8::2, :]
    frame_integrity_check(f1)
    assert f1.shape == (46, 1)
    assert isview(f1)
    f1.nrows = 10
    frame_integrity_check(f1)
    assert f1.shape == (10, 1)
    assert isview(f1)
    assert f1.to_list()[0] == list(range(8, 28, 2))
    f1.nrows = 15
    frame_integrity_check(f1)
    assert f1.shape == (15, 1)
    assert isview(f1)
    assert f1.to_list()[0] == list(range(8, 28, 2)) + [None] * 5


def test_resize_view_array():
    f0 = dt.Frame(range(100))
    f1 = f0[[1, 1, 2, 3, 5, 8, 13, 0], :]
    frame_integrity_check(f1)
    assert f1.shape == (8, 1)
    assert isview(f1)
    assert f1.to_list() == [[1, 1, 2, 3, 5, 8, 13, 0]]
    f1.nrows = 4
    frame_integrity_check(f1)
    assert f1.shape == (4, 1)
    assert isview(f1)
    assert f1.to_list() == [[1, 1, 2, 3]]
    f1.nrows = 5
    frame_integrity_check(f1)
    assert f1.shape == (5, 1)
    assert isview(f1)
    assert f1.to_list() == [[1, 1, 2, 3, None]]


def test_resize_bad():
    f0 = dt.Frame(range(10))

    with pytest.raises(ValueError) as e:
        f0.nrows = -3
    assert "Number of rows cannot be negative" in str(e.value)

    with pytest.raises(ValueError) as e:
        f0.nrows = 10**100
    assert "Value is too large" in str(e.value)

    with pytest.raises(TypeError) as e:
        f0.nrows = (10, 2)
    assert ("Number of rows must be an integer, not <class 'tuple'>"
            in str(e.value))


def test_resize_issue1527(capsys):
    f0 = dt.Frame(A=[])
    assert f0.nrows == 0
    f0.nrows = 5
    assert f0.nrows == 5
    assert f0.to_list() == [[None] * 5]


def test_resize_invalidates_stats():
    f0 = dt.Frame([3, 1, 4, 1, 5, 9, 2, 6])
    assert_equals(f0.max(), dt.Frame([9]))
    f0.nrows = 3
    frame_integrity_check(f0)
    assert_equals(f0.max(), dt.Frame([4]))


def test_resize_reduce_nrows_in_keyed_frame():
    DT = dt.Frame(A=range(100))
    DT.key = "A"
    DT.nrows = 50
    assert DT.key == ("A",)
    frame_integrity_check(DT)
    assert DT.to_list() == [list(range(50))]


def test_resize_increase_nrows_in_keyed_frame():
    DT = dt.Frame(A=range(100))
    DT.key = "A"
    with pytest.raises(ValueError, match = "Cannot increase the number of rows "
                       "in a keyed frame"):
        DT.nrows = 150
    assert DT.key == ("A",)
    frame_integrity_check(DT)
    assert DT.to_list() == [list(range(100))]


#-------------------------------------------------------------------------------
# Frame.repeat()
#-------------------------------------------------------------------------------

def test_dt_repeat():
    f0 = dt.Frame(range(10))
    f1 = dt.repeat(f0, 3)
    frame_integrity_check(f1)
    assert f1.to_list() == [list(range(10)) * 3]


def test_dt_repeat2():
    f0 = dt.Frame(["A", "B", "CDE"])
    f1 = dt.repeat(f0, 7)
    frame_integrity_check(f1)
    assert f1.to_list() == [f0.to_list()[0] * 7]


def test_dt_repeat_multicol():
    f0 = dt.Frame(A=[None, 1.4, -2.6, 3.9998],
                  B=["row", "row", "row", "your boat"],
                  C=[25, -9, 18, 2],
                  D=[True, None, True, False])
    f1 = dt.repeat(f0, 4)
    frame_integrity_check(f1)
    assert isview(f1)
    assert f1.names == f0.names
    assert f1.stypes == f0.stypes
    assert f1.to_list() == [col * 4 for col in f0.to_list()]


def test_dt_repeat_view():
    f0 = dt.Frame(A=[1, 3, 4, 5], B=[2, 6, 3, 1])
    f1 = f0[::2, :]
    f2 = dt.repeat(f1, 5)
    frame_integrity_check(f2)
    assert f2.to_dict() == {"A": [1, 4] * 5, "B": [2, 3] * 5}


def test_dt_repeat_empty_frame():
    f0 = dt.Frame()
    f1 = dt.repeat(f0, 5)
    frame_integrity_check(f1)
    assert f1.to_list() == []


def test_repeat_empty_frame2():
    f0 = dt.Frame(A=[], B=[], C=[], stypes=[dt.int32, dt.str32, dt.float32])
    f1 = dt.repeat(f0, 1000)
    frame_integrity_check(f1)
    assert f1.names == f0.names
    assert f1.stypes == f0.stypes
    assert f1.to_list() == f0.to_list()



#-------------------------------------------------------------------------------
# Renaming columns
#-------------------------------------------------------------------------------

def test_rename_list():
    d0 = dt.Frame([[7], [2], [5]])
    assert d0.names == ("C0", "C1", "C2")
    d0.names = ("A", "B", "E")
    d0.names = ["a", "c", "e"]
    frame_integrity_check(d0)
    assert d0.names == ("a", "c", "e")
    assert d0.colindex("a") == 0
    assert d0.colindex("c") == 1
    assert d0.colindex("e") == 2


def test_rename_dict():
    d0 = dt.Frame([[1], [2], ["hello"]])
    # Do not access `.names` before renaming (even with `assert`), see #2829.
    d0.names = {"C0": "x", "C2": "z"}
    d0.names = {"C1": "y"}
    frame_integrity_check(d0)
    assert d0.names == ("x", "y", "z")
    assert d0.colindex("x") == 0
    assert d0.colindex("y") == 1
    assert d0.colindex("z") == 2


def test_rename_dict_after_del():
    d0 = dt.Frame([[1], [2], ["hello"]])
    del d0["C1"]
    d0.names = {"C0": "x", "C2": "C1"}
    frame_integrity_check(d0)
    assert d0.names == ("x", "C1")
    assert d0.colindex("x") == 0
    assert d0.colindex("C1") == 1


def test_rename_bad1():
    d0 = dt.Frame([[1], [2], ["hello"]], names=("a", "b", "c"))
    with pytest.raises(TypeError):
        d0.names = {"a", "b"}


def test_rename_bad2():
    d0 = dt.Frame([[1], [2], ["hello"]], names=("a", "b", "c"))
    with pytest.raises(KeyError) as e:
        d0.names = {"xxx": "yyy"}
    assert "Cannot find column xxx in the Frame" == str(e.value)


def test_rename_bad3():
    d0 = dt.Frame([[1], [2], ["hello"]], names=("a", "b", "c"))
    with pytest.raises(ValueError) as e:
        d0.names = ['1', '2', '3', '5']
    assert ("The names list has length 4, while the Frame has only 3 columns"
            in str(e.value))


def test_rename_bad4():
    d0 = dt.Frame([[1], [2], ["hello"]], names=("a", "b", "c"))
    with pytest.raises(ValueError) as e:
        d0.names = ('k', )
    assert ("The names list has length 1, while the Frame has 3 columns"
            in str(e.value))


def test_rename_bad5():
    d0 = dt.Frame([[1], [2], ["hello"]], names=("a", "b", "c"))
    with pytest.raises(TypeError) as e:
        d0.names = {"a": 17}
    assert ("The replacement name for column a should be a string, "
            "but got <class 'int'>" == str(e.value))


def test_rename_default():
    d0 = dt.Frame([[1], [2], ["hello"]], names=("a", "b", "c"))
    del d0.names
    assert d0.names == ("C0", "C1", "C2")
    d0.names = None
    assert d0.names == ("C0", "C1", "C2")



#-------------------------------------------------------------------------------
# Test conversions into python
#-------------------------------------------------------------------------------

def test_to_list():
    src = [[-1, 0, 1, 3],
           ["cat", "dog", "mouse", "elephant"],
           [False, True, None, True]]
    d0 = dt.Frame(src, names=["A", "B", "C"])
    assert d0.ltypes == (ltype.int, ltype.str, ltype.bool)
    a0 = d0.to_list()
    assert len(a0) == 3
    assert len(a0[0]) == 4
    assert a0 == src
    # Special case for booleans (because Booleans compare equal to 1/0)
    assert all(b is None or isinstance(b, bool) for b in a0[2])


def test_to_list2():
    src = [[1.0, None, float("nan"), 3.3]]
    d0 = dt.Frame(src)
    assert d0.ltypes == (ltype.real, )
    a0 = d0.to_list()[0]
    assert a0 == [1.0, None, None, 3.3]


def test_to_tuples():
    d0 = dt.Frame([[2, 17, -5, 148],
                   [3.6, 9.99, -14.15, 2.5e100],
                   ["mars", "venus", "mercury", "polonium"],
                   [None, True, False, None]])
    assert d0.to_tuples() == [(2, 3.6, "mars", None),
                              (17, 9.99, "venus", True),
                              (-5, -14.15, "mercury", False),
                              (148, 2.5e100, "polonium", None)]


def test_to_dict():
    d0 = dt.Frame(A=["purple", "yellow", "indigo", "crimson"],
                  B=[0, None, 123779, -299],
                  C=[1.23, 4.56, 7.89, 10.11])
    assert d0.to_dict() == {"A": ["purple", "yellow", "indigo", "crimson"],
                            "B": [0, None, 123779, -299],
                            "C": [1.23, 4.56, 7.89, 10.11]}





#-------------------------------------------------------------------------------
# [0, 0] (conversion to scalar python variable)
#-------------------------------------------------------------------------------

def test_scalar1():
    d0 = dt.Frame([False])
    assert d0[0, 0] is False
    d1 = dt.Frame([-32767])
    assert d1[0, 0] == -32767
    d2 = dt.Frame([3.14159])
    assert d2[0, 0] == 3.14159
    d3 = dt.Frame(["koo!"])
    assert d3[0, 0] == "koo!"
    d4 = dt.Frame([None])
    assert d4[0, 0] is None


def test_scalar_on_view(dt0):
    assert dt0[1, 0] == 7
    assert dt0[3, 3] == 4.4
    assert dt0[2, 6] == "hello"
    assert dt0[2::5, 3::7][0, 0] == -4
    assert dt0[[3], "G"][0, 0] == "world"



#-------------------------------------------------------------------------------
# Explicit element selection
#-------------------------------------------------------------------------------

def test_single_element_extraction(dt0):
    assert dt0[0, 1] is True
    assert dt0[1, 1] is False
    assert dt0[2, "G"] == "hello"


def test_single_element_extraction_from_view(dt0):
    dt1 = dt0[[2, 3, 0], :]
    assert dt1[0, 0] == 0
    assert dt1[2, 2] == 1
    assert dt1[0, "D"] == -4.0
    assert dt1[1, "G"] == "world"


@pytest.mark.parametrize('st', list(dt.stype))
def test_single_element_all_stypes(st):
    from datetime import date as dd
    if st in (dt.stype.time64, dt.stype.arr32, dt.stype.arr64,
              dt.stype.cat8, dt.stype.cat16, dt.stype.cat32):
        return
    pt = (bool if st == dt.stype.bool8 else
          int if st.ltype == dt.ltype.int else
          float if st.ltype == dt.ltype.real else
          str if st.ltype == dt.ltype.str else
          dd if st == dt.stype.date32 else
          object)
    src = [True, False, True, None] if pt is bool else \
          [1, 7, -99, 214, None, 3333] if pt is int else \
          [2.5, 3.4e15, -7.909, None] if pt is float else \
          ['Oh', 'gobbly', None, 'sproo'] if pt is str else \
          [dd(2000, 5, 5), dd(2012, 12, 12), None] if pt is dd else \
          [dt, st, list, None, {3, 2, 1}]
    df = dt.Frame(A=src, stype=st)
    frame_integrity_check(df)
    assert df.names == ("A", )
    assert df.stypes == (st, )
    for i, item in enumerate(src):
        x = df[i, 0]
        y = df[i, "A"]
        assert x == y
        if item is None or st == dt.stype.void:
            assert x is None
        else:
            assert isinstance(x, pt)
            if st == dt.stype.int8:
                assert (x - item) % 256 == 0
            elif st == dt.stype.float32:
                assert abs(1 - item / x) < 1e-7
            else:
                assert x == item


def test_single_element_select_invalid_column(dt0):
    with pytest.raises(KeyError) as e:
        noop(dt0[0, "Dd"])
    assert ("Column Dd does not exist in the Frame; did you mean D?" ==
            str(e.value))



#-------------------------------------------------------------------------------
# head / tail
#-------------------------------------------------------------------------------

def test_head():
    d0 = dt.Frame(A=range(20), B=[3, 5] * 10)
    d1 = d0.head(3)
    assert d1.to_dict() == {"A": [0, 1, 2], "B": [3, 5, 3]}
    d2 = d0.head(1)
    assert d2.to_dict() == {"A": [0], "B": [3]}
    d3 = d0.head(0)
    assert d3.to_dict() == {"A": [], "B": []}
    d4 = d0.head(100)
    assert d4.to_dict() == d0.to_dict()
    d5 = d0.head()
    assert d5.to_dict() == {"A": list(range(10)), "B": [3, 5] * 5}


def test_tail():
    d0 = dt.Frame(A=range(20), B=[3, 5] * 10)
    d1 = d0.tail(3)
    assert d1.to_dict() == {"A": [17, 18, 19], "B": [5, 3, 5]}
    d2 = d0.tail(1)
    assert d2.to_dict() == {"A": [19], "B": [5]}
    d3 = d0.tail(0)
    assert d3.to_dict() == {"A": [], "B": []}
    d4 = d0.tail(100)
    assert d4.to_dict() == d0.to_dict()
    d5 = d0.tail()
    assert d5.to_dict() == {"A": list(range(10, 20)), "B": [3, 5] * 5}


def test_head_bad():
    d0 = dt.Frame(range(10))
    msg = r"The argument in method datatable.Frame.head\(\) cannot be negative"
    with pytest.raises(ValueError, match=msg):
        d0.head(-5)
    msg = r"The argument in method datatable.Frame.head\(\) should be an integer"
    with pytest.raises(TypeError, match=msg) as e:
        d0.head(5.0)


def test_tail_bad():
    d0 = dt.Frame(range(10))
    msg = r"The argument in method datatable.Frame.tail\(\) cannot be negative"
    with pytest.raises(ValueError, match=msg) as e:
        d0.tail(-5)
    msg = r"The argument in method datatable.Frame.tail\(\) should be an integer"
    with pytest.raises(TypeError) as e:
        d0.tail(5.0)



#-------------------------------------------------------------------------------
# Materialize
#-------------------------------------------------------------------------------

def test_materialize():
    DT1 = dt.Frame(A=range(12))[::2, :]
    DT2 = dt.repeat(dt.Frame(B=["red", "green", "blue"]), 2)
    DT3 = dt.Frame(C=[4, 2, 9.1, 12, 0])
    DT = dt.cbind(DT1, DT2, DT3, force=True)
    assert frame_columns_virtual(DT) == [True, True, True]
    DT.materialize()
    assert frame_columns_virtual(DT) == [False, False, False]


def test_materialize_object_col():
    class A:
        pass

    DT = dt.Frame([A() for i in range(5)], stype=dt.obj64)
    DT = DT[::-1, :]
    frame_integrity_check(DT)
    DT.materialize()
    frame_integrity_check(DT)
    assert DT.shape == (5, 1)
    assert DT.stype == dt.obj64
    assert all(isinstance(x, A) and sys.getrefcount(x) > 0
               for x in DT.to_list()[0])


def test_materialize_to_memory(tempfile_jay):
    DT = dt.Frame(A=["red", "orange", "yellow"], B=[5, 2, 8])
    DT.to_jay(tempfile_jay)
    del DT
    DT = dt.fread(tempfile_jay)
    DT.materialize(to_memory=True)
    os.unlink(tempfile_jay)
    dt.Frame([5]).to_jay(tempfile_jay)
    assert_equals(DT, dt.Frame(A=["red", "orange", "yellow"], B=[5, 2, 8]))


def test_materialize_to_memory_bad_type():
    DT = dt.Frame(range(5))
    msg = r"Argument to_memory in method datatable.Frame.materialize\(\) should be a boolean"
    with pytest.raises(TypeError, match=msg):
        DT.materialize(to_memory=0)



#-------------------------------------------------------------------------------
# export_names()
#-------------------------------------------------------------------------------

def test_export_names(dt0):
    A, B, C, D, E, F, G = dt0.export_names()
    all_names = [A, B, C, D, E, F, G]
    assert_equals(dt0[:, all_names], dt0)
    assert_equals(dt0[A > 0, :], dt0[:2, :])
    assert str(A + B <= C + D) == "FExpr<f['A'] + f['B'] <= f['C'] + f['D']>"
    assert str(dt.f.A + dt.f.B <= dt.f.C + dt.f.D) == "FExpr<f.A + f.B <= f.C + f.D>"
    assert "A" not in globals()



#-------------------------------------------------------------------------------
# Misc
#-------------------------------------------------------------------------------

def test_internal_rowindex():
    d0 = dt.Frame(list(range(100)))
    d1 = d0[:20, :]
    assert frame_columns_virtual(d0) == [False]
    assert frame_columns_virtual(d1) == [True]


def test_issue898():
    """
    Test that reification of obj column views does not lead to any disastrous
    results (previously this was crashing).
    """
    class A: pass
    f0 = dt.Frame([A() for i in range(1111)] / dt.obj64)
    assert f0.stypes == (dt.stype.obj64, )
    f1 = f0[:-1, :]
    del f0
    f1.materialize()
    res = f1.to_list()
    del res


def test_issue1728(tempfile_jay):
    data = dt.Frame({'department1': [None, 't'], 'C0': [3580, 1047]})
    data.to_jay(tempfile_jay)
    del data
    counts = dt.fread(tempfile_jay)
    counts = counts[1:, :]
    counts = counts[:, :, dt.sort(-1)]
    counts.materialize()
    frame_integrity_check(counts)
    assert counts.to_dict() == {'department1': ['t'], 'C0': [1047]}


def test_issue2036():
    DT = dt.Frame(Z=range(17))
    DT.rbind([DT] * 3)
    assert DT.nrows == 68
    del DT[[1, 4, 7], :]
    assert DT.nrows == 65
    DT.nrows = 14
    assert DT.nrows == 14
    DT = DT.copy()
    assert DT.nrows == 14


def test_issue2269():
    import ctypes
    DT = dt.Frame(range(10000))
    ptr = dt.internal.frame_column_data_r(DT, 0)
    assert isinstance(ptr, ctypes.c_void_p)


def test_issue2873():
    from time import time
    DT = dt.Frame([[1]] * 10000)
    names10000 = DT.names
    names1000 = names10000[:1000]
    t0 = time()
    DT[:, names10000]
    t10000 = time() - t0
    t0 = time()
    DT[:, names1000]
    t1000 = time() - t0
    assert t10000 < 1.0
    # The timer can have low resolution and produce `t1000 == 0`
    if t1000 > 0:
        assert t10000 / t1000 < 50
