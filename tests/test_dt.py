#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import pytest
import subprocess
import sys
import time
import datatable as dt
from datatable import stype, ltype, f, isna
from tests import same_iterables, list_equals


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


@pytest.fixture()
def patched_terminal(monkeypatch):
    def send1(refresh_rate=1):
        yield None
    monkeypatch.setattr("datatable.utils.terminal.wait_for_keypresses", send1)
    dt.utils.terminal.term.override_width = 100
    dt.utils.terminal.term.disable_styling()


def assert_valueerror(datatable, rows, error_message):
    with pytest.raises(ValueError) as e:
        datatable(rows=rows)
    assert str(e.type) == "<class 'dt.ValueError'>"
    assert error_message in str(e.value)



#-------------------------------------------------------------------------------
# Run the tests
#-------------------------------------------------------------------------------

def test_platform():
    # This test performs only minimal checks, and also outputs diagnostic
    # information about the platform being run.
    print()
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



@pytest.mark.skipif(sys.platform != "darwin",
                    reason="This test behaves unpredictably on Linux")
def test_dt_loadtime(nocov):
    # Check that datatable's loading time is not too big. At the time of writing
    # this test, on a MacBook Pro laptop, the timings were the following:
    #     t_python:    0.0505s
    #     t_smtplib:   0.0850s
    #     t_datatable: 0.1335s
    #     ratio:  2.07 - 2.42
    python = sys.executable
    attempts = 0
    while attempts < 3:
        attempts += 1
        t0 = time.time()
        out = subprocess.check_output([python, "-c", ""])
        tpy = time.time() - t0
        t0 = time.time()
        out = subprocess.check_output([python, "-c", "import smtplib"])
        t_smtplib = time.time() - t0
        t0 = time.time()
        out = subprocess.check_output([python, "-c", "import datatable"])
        t_datatable = time.time() - t0
        assert out == b""
        print()
        print("t_python:    %.4fs" % tpy)
        print("t_smtplib:   %.4fs  (%.4fs)" % (t_smtplib, t_smtplib - tpy))
        print("t_datatable: %.4fs  (%.4fs)" % (t_datatable, t_datatable - tpy))
        if t_datatable < tpy or t_smtplib < tpy:
            continue
        ratio = (t_datatable - tpy) / (t_smtplib - tpy)
        print("Ratio: %.6f" % ratio)
        if ratio < 3:
            return
    assert ratio < 4


def test_dt_dependencies():
    # This test checks how many dependencies `datatable` needs to load at
    # startup. At the time of writing this test, the count was 168.
    # The boundary of 200 may need to be revised upwards at some point (if
    # new dependencies or source files are added), or downwards, if we eliminate
    # some of the heavy dependent packages as blessed, llvmlite, typesentry.
    n = subprocess.check_output([sys.executable, "-c",
                                 "import sys; "
                                 "set1 = set(sys.modules); "
                                 "import datatable; "
                                 "set2 = set(sys.modules); "
                                 "assert 'numpy' not in set2; "
                                 "print(len(set2-set1), end='')"])
    assert int(n) < 200


def test_dt_version():
    assert dt.__version__
    assert isinstance(dt.__version__, str)
    assert dt.__git_revision__
    assert isinstance(dt.__git_revision__, str)
    assert len(dt.__git_revision__) == 40


def test_dt_properties(dt0):
    assert isinstance(dt0, dt.Frame)
    dt0.internal.check()
    assert dt0.nrows == 4
    assert dt0.ncols == 7
    assert dt0.shape == (4, 7)
    assert dt0.names == ("A", "B", "C", "D", "E", "F", "G")
    assert dt0.ltypes == (ltype.int, ltype.bool, ltype.bool, ltype.real,
                          ltype.bool, ltype.bool, ltype.str)
    assert dt0.stypes == (stype.int8, stype.bool8, stype.bool8, stype.float64,
                          stype.bool8, stype.bool8, stype.str32)
    assert dt0.internal.alloc_size > 500
    assert sys.getsizeof(dt0) >= dt0.internal.alloc_size



def test_internal():
    # Run C++ tests
    from datatable.lib import core
    if hasattr(core, "test_internal"):
        core.test_internal()



def test_dt_call(dt0, capsys):
    print("before computing dt1")
    dt1 = dt0(timeit=True)
    print("dt1 computed")
    dt1.internal.check()
    assert dt1.shape == dt0.shape
    assert not dt1.internal.isview
    out, err = capsys.readouterr()
    assert err == ""
    assert "Time taken:" in out


def test_dt_view(dt0, patched_terminal, capsys):
    dt0.view()
    out, err = capsys.readouterr()
    assert ("      A   B   C     D   E   F  G    \n"
            "---  --  --  --  ----  --  --  -----\n"
            " 0    2   1   1   0.1       0  1    \n"
            " 1    7   0   1   2         0  2    \n"
            " 2    0   0   1  -4         0  hello\n"
            " 3    0   1   1   4.4       0  world\n"
            "\n"
            "[4 rows x 7 columns]\n"
            "Press q to quit  ↑←↓→ to move  wasd to page  t to toggle types"
            in out)


def test_dt_getitem(dt0):
    dt1 = dt0[:, 0]
    assert dt1.shape == (4, 1)
    assert dt1.names == ("A", )
    dt1 = dt0[(..., 4)]
    assert dt1.shape == (4, 1)
    assert dt1.names == ("E", )
    elem2 = dt0[0, 1]
    assert elem2 is True
    with pytest.raises(ValueError) as e:
        dt0[0, 1, 2, 3]
    assert "Selector (0, 1, 2, 3) is not supported" in str(e.value)
    with pytest.warns(FutureWarning) as ws:
        dt0["A"]
    assert len(ws) == 1
    assert ("Single-item selectors `DT[col]` are deprecated"
            in ws[0].message.args[0])


def test_issue1406(dt0):
    with pytest.raises(ValueError) as e:
        dt0[tuple()]
    assert "Invalid selector ()" == str(e.value)
    with pytest.raises(ValueError) as e:
        dt0[(None,)]
    assert "Invalid selector (None,)" == str(e.value)



#-------------------------------------------------------------------------------
# Colindex
#-------------------------------------------------------------------------------

def test_dt_colindex(dt0):
    assert dt0.colindex(0) == 0
    assert dt0.colindex(1) == 1
    assert dt0.colindex(-1) == 6
    for i, ch in enumerate("ABCDEFG"):
        assert dt0.colindex(ch) == i


def test_dt_colindex_bad(dt0):
    with pytest.raises(ValueError) as e:
        dt0.colindex("a")
    assert "Column `a` does not exist in the Frame" in str(e.value)
    with pytest.raises(ValueError) as e:
        dt0.colindex(7)
    assert ("Column index `7` is invalid for a Frame with 7 columns" ==
            str(e.value))
    with pytest.raises(ValueError) as e:
        dt0.colindex(-8)
    assert ("Column index `-8` is invalid for a Frame with 7 columns" ==
            str(e.value))
    for x in [False, None, 1.99, [1, 2, 3]]:
        with pytest.raises(TypeError) as e:
            dt0.colindex(x)
        assert ("The argument to Frame.colindex() should be a string or an "
                "integer, not %s" % type(x) == str(e.value))


def test_dt_colindex_fuzzy_suggestions():
    def check(DT, name, suggestions):
        with pytest.raises(ValueError) as e:
            DT.colindex(name)
        assert str(e.value).endswith(suggestions)

    d0 = dt.Frame([[0]] * 3, names=["foo", "bar", "baz"])
    d0.internal.check()
    check(d0, "fo", "; did you mean `foo`?")
    check(d0, "foe", "; did you mean `foo`?")
    check(d0, "fooo", "; did you mean `foo`?")
    check(d0, "ba", "; did you mean `bar` or `baz`?")
    check(d0, "barb", "; did you mean `bar` or `baz`?")
    check(d0, "bazb", "; did you mean `baz` or `bar`?")
    check(d0, "ababa", "Frame")
    d1 = dt.Frame([[0]] * 50)
    d1.internal.check()
    check(d1, "A", "Frame")
    check(d1, "C", "; did you mean `C0`, `C1` or `C2`?")
    check(d1, "c1", "; did you mean `C1`, `C0` or `C2`?")
    check(d1, "C 1", "; did you mean `C1`, `C11` or `C21`?")
    check(d1, "V0", "; did you mean `C0`?")
    check(d1, "Va", "Frame")
    d2 = dt.Frame(varname=[1])
    d2.internal.check()
    check(d2, "vraname", "; did you mean `varname`?")
    check(d2, "VRANAME", "; did you mean `varname`?")
    check(d2, "var_name", "; did you mean `varname`?")
    check(d2, "variable", "; did you mean `varname`?")



#-------------------------------------------------------------------------------
# Delete columns
#-------------------------------------------------------------------------------

# Not a fixture: create a new datatable each time this function is called
def smalldt():
    res = dt.Frame([[i] for i in range(16)], names=list("ABCDEFGHIJKLMNOP"))
    assert res.ltypes == (ltype.bool, ltype.bool) + (ltype.int,) * 14
    return res

def test_del_0cols():
    d0 = smalldt()
    del d0[:, []]
    d0.internal.check()
    assert d0.shape == (1, 16)
    assert d0.topython() == smalldt().topython()

def test_del_1col_str_1():
    d0 = smalldt()
    del d0[:, "A"]
    d0.internal.check()
    assert d0.shape == (1, 15)
    assert d0.topython() == [[i] for i in range(1, 16)]
    assert d0.names == tuple("BCDEFGHIJKLMNOP")
    assert len(d0.ltypes) == d0.ncols

def test_del_1col_str_2():
    d0 = smalldt()
    del d0[:, "B"]
    d0.internal.check()
    assert d0.shape == (1, 15)
    assert d0.topython() == [[i] for i in range(16) if i != 1]
    assert d0.names == tuple("ACDEFGHIJKLMNOP")

def test_del_1col_str_3():
    d0 = smalldt()
    del d0[:, "P"]
    d0.internal.check()
    assert d0.shape == (1, 15)
    assert d0.topython() == [[i] for i in range(16) if i != 15]
    assert d0.names == tuple("ABCDEFGHIJKLMNO")

def test_del_1col_int():
    d0 = smalldt()
    del d0[:, -1]
    d0.internal.check()
    assert d0.shape == (1, 15)
    assert d0.names == tuple("ABCDEFGHIJKLMNO")

def test_del_cols_strslice():
    d0 = smalldt()
    del d0[:, "E":"K"]
    d0.internal.check()
    assert d0.shape == (1, 9)
    assert d0.names == tuple("ABCDLMNOP")
    assert d0.topython() == [[0], [1], [2], [3], [11], [12], [13], [14], [15]]

def test_del_cols_intslice1():
    d0 = smalldt()
    del d0[:, ::2]
    d0.internal.check()
    assert d0.shape == (1, 8)
    assert d0.names == tuple("BDFHJLNP")
    assert d0.topython() == [[i] for i in range(1, 16, 2)]

def test_del_cols_intslice2():
    d0 = smalldt()
    del d0[:, ::-2]
    d0.internal.check()
    assert d0.shape == (1, 8)
    assert d0.names == tuple("ACEGIKMO")
    assert d0.topython() == [[i] for i in range(0, 16, 2)]

def test_del_cols_all():
    d0 = smalldt()
    del d0[:, :]
    d0.internal.check()
    assert d0.names == tuple()
    assert d0.shape == (0, 0)

def test_del_cols_list():
    d0 = smalldt()
    del d0[:, [0, 3, 0, 5, 0, 9]]
    d0.internal.check()
    assert d0.names == tuple("BCEGHIKLMNOP")
    assert d0.shape == (1, 12)

def test_del_cols_multislice():
    d0 = smalldt()
    del d0[:, [slice(10), 12, -1]]
    d0.internal.check()
    assert d0.names == tuple("KLNO")
    assert d0.shape == (1, 4)

def test_del_cols_generator():
    d0 = smalldt()
    del d0[:, (i**2 for i in range(4))]
    d0.internal.check()
    assert d0.names == tuple("CDFGHIKLMNOP")

def test_del_cols_with_allrows():
    d0 = smalldt()
    del d0[:, ["A", "B", "C"]]
    d0.internal.check()
    assert d0.shape == (1, 13)
    assert d0.names == tuple("DEFGHIJKLMNOP")

def test_delitem_invalid_selectors():
    """
    Test deleting columns from a datatable.
    Note: don't use dt0 here, because this test will modify it, potentially
    invalidating other tests.
    """
    d0 = smalldt()
    with pytest.raises(ValueError) as e:
        del d0[:, 0.5]
    assert "Unknown `select` argument: 0.5" in str(e.value)
    with pytest.raises(ValueError):
        del d0[:, d0]
    with pytest.raises(TypeError):
        del d0[:, [1, 2, 1, 0.7]]



#-------------------------------------------------------------------------------
# Delete rows
#-------------------------------------------------------------------------------

def test_del_rows_single():
    d0 = dt.Frame(range(10))
    del d0[3, :]
    d0.internal.check()
    assert d0.topython() == [[0, 1, 2, 4, 5, 6, 7, 8, 9]]


def test_del_rows_slice0():
    d0 = dt.Frame(range(10))
    del d0[:3, :]
    d0.internal.check()
    assert d0.shape == (7, 1)
    assert d0.topython() == [list(range(3, 10))]


def test_del_rows_slice1():
    d0 = dt.Frame(range(10))
    del d0[-4:, :]
    d0.internal.check()
    assert d0.shape == (6, 1)
    assert d0.topython() == [list(range(10 - 4))]


def test_del_rows_slice2():
    d0 = dt.Frame(range(10))
    del d0[2:6, :]
    d0.internal.check()
    assert d0.shape == (6, 1)
    assert d0.topython() == [[0, 1, 6, 7, 8, 9]]


def test_del_rows_slice_empty():
    d0 = dt.Frame(range(10))
    del d0[4:4, :]
    d0.internal.check()
    assert d0.shape == (10, 1)
    assert d0.topython() == [list(range(10))]


def test_del_rows_slice_reverse():
    d0 = dt.Frame(range(10))
    s0 = list(range(10))
    del d0[:4:-1, :]
    del s0[:4:-1]
    d0.internal.check()
    assert d0.topython() == [s0]


def test_del_rows_slice_all():
    d0 = dt.Frame(range(10))
    del d0[::-1, :]
    d0.internal.check()
    assert d0.shape == (0, 1)


def test_del_all():
    d0 = dt.Frame(range(10))
    del d0[:, :]
    d0.internal.check()
    assert d0.shape == (0, 0)


def test_del_rows_slice_step():
    d0 = dt.Frame(range(10))
    del d0[::3, :]
    d0.internal.check()
    assert d0.topython() == [[1, 2, 4, 5, 7, 8]]


def test_del_rows_array():
    d0 = dt.Frame(range(10))
    del d0[[0, 7, 8], :]
    d0.internal.check()
    assert d0.topython() == [[1, 2, 3, 4, 5, 6, 9]]


@pytest.mark.skip("Issue #805")
def test_del_rows_array_unordered():
    d0 = dt.Frame(range(10))
    del d0[[3, 1, 5, 2, 2, 0, -1], :]
    d0.internal.check()
    assert d0.topython() == [[4, 6, 7, 8]]


def test_del_rows_filter():
    d0 = dt.Frame(range(10), names=["A"], stype="int32")
    del d0[f.A < 4, :]
    d0.internal.check()
    assert d0.topython() == [[4, 5, 6, 7, 8, 9]]


def test_del_rows_nas():
    d0 = dt.Frame({"A": [1, 5, None, 12, 7, None, -3]})
    del d0[isna(f.A), :]
    d0.internal.check()
    assert d0.topython() == [[1, 5, 12, 7, -3]]


def test_del_rows_from_view1():
    d0 = dt.Frame(range(10))
    d1 = d0[::2, :]  # 0 2 4 6 8
    del d1[3, :]
    assert d1.topython() == [[0, 2, 4, 8]]
    del d1[[0, 3], :]
    assert d1.topython() == [[2, 4]]


def test_del_rows_from_view2():
    f0 = dt.Frame([1, 3, None, 4, 5, None, None, 2, None, None, None])
    f1 = f0[5:, :]
    del f1[isna(f[0]), :]
    assert f1.topython() == [[2]]



#-------------------------------------------------------------------------------
# Resize rows
#-------------------------------------------------------------------------------

def test_resize_rows_api():
    f0 = dt.Frame([20])
    f0.nrows = 3
    f0.nrows = 5
    f0.internal.check()
    assert f0.topython() == [[20, 20, 20, None, None]]


def test_resize_rows0():
    f0 = dt.Frame(range(10), stype=dt.int32)
    f0.nrows = 6
    f0.internal.check()
    assert f0.shape == (6, 1)
    assert f0.stypes == (dt.int32,)
    assert f0.topython() == [[0, 1, 2, 3, 4, 5]]
    f0.nrows = 12
    f0.internal.check()
    assert f0.shape == (12, 1)
    assert f0.stypes == (dt.int32,)
    assert f0.topython() == [[0, 1, 2, 3, 4, 5] + [None] * 6]
    f0.nrows = 1
    f0.internal.check()
    assert f0.shape == (1, 1)
    assert f0.stypes == (dt.int32,)
    assert f0.scalar() == 0
    f0.nrows = 20
    f0.internal.check()
    assert f0.shape == (20, 1)
    assert f0.stypes == (dt.int32,)
    assert f0.topython() == [[0] * 20]


def test_resize_rows1():
    srcs = [[True], [5], [14.3], ["fooga"], ["zoom"]]
    stypes = (dt.bool8, dt.int64, dt.float64, dt.str32, dt.str64)
    f0 = dt.Frame(srcs, stypes=stypes)
    f0.internal.check()
    assert f0.shape == (1, 5)
    assert f0.stypes == stypes
    f0.nrows = 7
    f0.internal.check()
    assert f0.shape == (7, 5)
    assert f0.stypes == stypes
    assert f0.topython() == [src * 7 for src in srcs]
    f0.nrows = 20
    f0.internal.check()
    assert f0.shape == (20, 5)
    assert f0.stypes == stypes
    assert f0.topython() == [src * 7 + [None] * 13 for src in srcs]
    f0.nrows = 0
    f0.internal.check()
    assert f0.shape == (0, 5)
    assert f0.stypes == stypes
    assert f0.topython() == [[]] * 5


def test_resize_rows_nastrs():
    f0 = dt.Frame(["foo", None, None, None, "gar"])
    f0.nrows = 3
    assert f0.topython() == [["foo", None, None]]
    f0.nrows = 10
    f0.internal.check()
    assert f0.topython() == [["foo"] + [None] * 9]


def test_resize_view_slice():
    f0 = dt.Frame(range(100))
    f1 = f0[8::2, :]
    f1.internal.check()
    assert f1.shape == (46, 1)
    assert f1.internal.isview
    f1.nrows = 10
    f1.internal.check()
    assert f1.shape == (10, 1)
    assert f1.internal.isview
    assert f1.topython()[0] == list(range(8, 28, 2))
    f1.nrows = 15
    f1.internal.check()
    assert f1.shape == (15, 1)
    assert not f1.internal.isview
    assert f1.topython()[0] == list(range(8, 28, 2)) + [None] * 5


def test_resize_view_array():
    f0 = dt.Frame(range(100))
    f1 = f0[[1, 1, 2, 3, 5, 8, 13, 0], :]
    f1.internal.check()
    assert f1.shape == (8, 1)
    assert f1.internal.isview
    assert f1.topython() == [[1, 1, 2, 3, 5, 8, 13, 0]]
    f1.nrows = 4
    f1.internal.check()
    assert f1.shape == (4, 1)
    assert f1.internal.isview
    assert f1.topython() == [[1, 1, 2, 3]]
    f1.nrows = 5
    f1.internal.check()
    assert f1.shape == (5, 1)
    assert not f1.internal.isview
    assert f1.topython() == [[1, 1, 2, 3, None]]


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



#-------------------------------------------------------------------------------
# Renaming columns
#-------------------------------------------------------------------------------

def test_rename_list():
    d0 = dt.Frame([[7], [2], [5]])
    d0.names = ("A", "B", "E")
    d0.names = ["a", "c", "e"]
    d0.internal.check()
    assert d0.names == ("a", "c", "e")
    assert d0.colindex("a") == 0
    assert d0.colindex("c") == 1
    assert d0.colindex("e") == 2


def test_rename_dict():
    d0 = dt.Frame([[1], [2], ["hello"]])
    assert d0.names == ("C0", "C1", "C2")
    d0.names = {"C0": "x", "C2": "z"}
    d0.names = {"C1": "y"}
    d0.internal.check()
    assert d0.names == ("x", "y", "z")
    assert d0.colindex("x") == 0
    assert d0.colindex("y") == 1
    assert d0.colindex("z") == 2


def test_rename_frame_copy():
    # Check that when copying a frame, the changes to memoized .py_names and
    # .py_inames do not get propagated to the original Frame.
    d0 = dt.Frame([[1], [2], [3]])
    assert d0.names == ("C0", "C1", "C2")
    d1 = d0.copy()
    assert d1.names == ("C0", "C1", "C2")
    d1.names = {"C1": "ha!"}
    assert d1.names == ("C0", "ha!", "C2")
    assert d0.names == ("C0", "C1", "C2")
    assert d1.colindex("ha!") == 1
    with pytest.raises(ValueError):
        d0.colindex("ha!")


def test_rename_bad1():
    d0 = dt.Frame([[1], [2], ["hello"]], names=("a", "b", "c"))
    with pytest.raises(TypeError):
        d0.names = {"a", "b"}


def test_rename_bad2():
    d0 = dt.Frame([[1], [2], ["hello"]], names=("a", "b", "c"))
    with pytest.raises(ValueError) as e:
        d0.names = {"xxx": "yyy"}
    assert "Cannot find column `xxx` in the Frame" == str(e.value)


def test_rename_bad3():
    d0 = dt.Frame([[1], [2], ["hello"]], names=("a", "b", "c"))
    with pytest.raises(ValueError) as e:
        d0.names = ['1', '2', '3', '5']
    assert ("The `names` list has length 4, while the Frame has only 3 columns"
            in str(e.value))


def test_rename_bad4():
    d0 = dt.Frame([[1], [2], ["hello"]], names=("a", "b", "c"))
    with pytest.raises(ValueError) as e:
        d0.names = ('k', )
    assert ("The `names` list has length 1, while the Frame has 3 columns"
            in str(e.value))


def test_rename_bad5():
    d0 = dt.Frame([[1], [2], ["hello"]], names=("a", "b", "c"))
    with pytest.raises(TypeError) as e:
        d0.names = {"a": 17}
    assert ("The replacement name for column `a` should be a string, "
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
    a0 = d0.topython()
    assert len(a0) == 3
    assert len(a0[0]) == 4
    assert a0 == src
    # Special case for booleans (because Booleans compare equal to 1/0)
    assert all(b is None or isinstance(b, bool) for b in a0[2])


def test_to_list2():
    src = [[1.0, None, float("nan"), 3.3]]
    d0 = dt.Frame(src)
    assert d0.ltypes == (ltype.real, )
    a0 = d0.topython()[0]
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
# Test conversions into Pandas / Numpy
#-------------------------------------------------------------------------------

@pytest.mark.usefixtures("pandas")
def test_topandas():
    d0 = dt.Frame({"A": [1, 5], "B": ["hello", "you"], "C": [True, False]})
    p0 = d0.topandas()
    assert p0.shape == (2, 3)
    assert same_iterables(p0.columns.tolist(), ["A", "B", "C"])
    assert p0["A"].values.tolist() == [1, 5]
    assert p0["B"].values.tolist() == ["hello", "you"]
    assert p0["C"].values.tolist() == [True, False]


@pytest.mark.usefixtures("pandas")
def test_topandas_view():
    d0 = dt.Frame([[1, 5, 2, 0, 199, -12],
                   ["alpha", "beta", None, "delta", "epsilon", "zeta"],
                   [.3, 1e2, -.5, 1.9, 2.2, 7.9]], names=["A", "b", "c"])
    d1 = d0[::-2, :]
    p1 = d1.topandas()
    assert p1.shape == d1.shape
    assert p1.columns.tolist() == ["A", "b", "c"]
    assert p1.values.T.tolist() == d1.topython()


@pytest.mark.usefixtures("pandas")
def test_topandas_nas():
    d0 = dt.Frame([[True, None, None, None, False],
                   [1, 5, None, -16, 100],
                   [249, None, 30000, 1, None],
                   [4587074, None, 109348, 1394, -343],
                   [None, None, None, None, 134918374091834]])
    d0.internal.check()
    assert d0.stypes == (dt.stype.bool8, dt.stype.int8, dt.stype.int16,
                         dt.stype.int32, dt.stype.int64)
    p0 = d0.topandas()
    # Check that each column in Pandas DataFrame has the correct number of NAs
    assert p0.count().tolist() == [2, 4, 3, 4, 1]


def test_tonumpy0(numpy):
    d0 = dt.Frame([1, 3, 5, 7, 9])
    assert d0.stypes == (stype.int8, )
    a0 = d0.tonumpy()
    assert a0.shape == d0.shape
    assert a0.dtype == numpy.dtype("int8")
    assert a0.tolist() == [[1], [3], [5], [7], [9]]
    a1 = numpy.array(d0)
    assert (a0 == a1).all()


def test_tonumpy1(numpy):
    d0 = dt.Frame({"A": [1, 5], "B": ["helo", "you"],
                   "C": [True, False], "D": [3.4, None]})
    a0 = d0.tonumpy()
    assert a0.shape == d0.shape
    assert a0.dtype == numpy.dtype("object")
    assert same_iterables(a0.T.tolist(), d0.topython())
    a1 = numpy.array(d0)
    assert (a0 == a1).all()


def test_numpy_constructor_simple(numpy):
    tbl = [[1, 4, 27, 9, 22], [-35, 5, 11, 2, 13], [0, -1, 6, 100, 20]]
    d0 = dt.Frame(tbl)
    assert d0.shape == (5, 3)
    assert d0.stypes == (stype.int8, stype.int8, stype.int8)
    assert d0.topython() == tbl
    n0 = numpy.array(d0)
    assert n0.shape == d0.shape
    assert n0.dtype == numpy.dtype("int8")
    assert n0.T.tolist() == tbl


def test_numpy_constructor_empty(numpy):
    d0 = dt.Frame()
    assert d0.shape == (0, 0)
    n0 = numpy.array(d0)
    assert n0.shape == (0, 0)
    assert n0.tolist() == []


def test_numpy_constructor_multi_types(numpy):
    # Test that multi-types datatable will be promoted into a common type
    tbl = [[1, 5, 10],
           [True, False, False],
           [30498, 1349810, -134308],
           [1.454, 4.9e-23, 10000000]]
    d0 = dt.Frame(tbl)
    assert d0.stypes == (stype.int8, stype.bool8, stype.int32, stype.float64)
    n0 = numpy.array(d0)
    assert n0.dtype == numpy.dtype("float64")
    assert n0.T.tolist() == [[1.0, 5.0, 10.0],
                             [1.0, 0, 0],
                             [30498, 1349810, -134308],
                             [1.454, 4.9e-23, 1e7]]
    assert (d0.tonumpy() == n0).all()


def test_numpy_constructor_view(numpy):
    d0 = dt.Frame([range(100), range(0, 1000000, 10000)])
    d1 = d0[::-2, :]
    assert d1.internal.isview
    n1 = numpy.array(d1)
    assert n1.dtype == numpy.dtype("int32")
    assert n1.T.tolist() == [list(range(99, 0, -2)),
                             list(range(990000, 0, -20000))]
    assert (d1.tonumpy() == n1).all()


def test_numpy_constructor_single_col(numpy):
    d0 = dt.Frame([1, 1, 3, 5, 8, 13, 21, 34, 55])
    assert d0.stypes == (stype.int8, )
    n0 = numpy.array(d0)
    assert n0.shape == d0.shape
    assert n0.dtype == numpy.dtype("int8")
    assert (n0 == d0.tonumpy()).all()


def test_numpy_constructor_single_string_col(numpy):
    d = dt.Frame(["adf", "dfkn", "qoperhi"])
    assert d.shape == (3, 1)
    assert d.stypes == (stype.str32, )
    a = numpy.array(d)
    assert a.shape == d.shape
    assert a.dtype == numpy.dtype("object")
    assert a.T.tolist() == d.topython()
    assert (a == d.tonumpy()).all()


def test_numpy_constructor_view_1col(numpy):
    d0 = dt.Frame({"A": [1, 2, 3, 4], "B": [True, False, True, False]})
    d2 = d0[::2, "B"]
    a = d2.tonumpy()
    assert a.T.tolist() == [[True, True]]
    assert (a == numpy.array(d2)).all()


def test_tonumpy_with_stype(numpy):
    """
    Test using dt.tonumpy() with explicit `stype` argument.
    """
    src = [[1.5, 2.6, 5.4], [3, 7, 10]]
    d0 = dt.Frame(src)
    assert d0.stypes == (stype.float64, stype.int8)
    a1 = d0.tonumpy("float32")
    a2 = d0.tonumpy()
    del d0
    # Check using list_equals(), which allows a tolerance of 1e-6, because
    # conversion to float32 resulted in loss of precision
    assert list_equals(a1.T.tolist(), src)
    assert a2.T.tolist() == src
    assert a1.dtype == numpy.dtype("float32")
    assert a2.dtype == numpy.dtype("float64")



#-------------------------------------------------------------------------------
# .scalar()
#-------------------------------------------------------------------------------

def test_scalar1():
    d0 = dt.Frame([False])
    assert d0.scalar() is False
    d1 = dt.Frame([-32767])
    assert d1.scalar() == -32767
    d2 = dt.Frame([3.14159])
    assert d2.scalar() == 3.14159
    d3 = dt.Frame(["koo!"])
    assert d3.scalar() == "koo!"
    d4 = dt.Frame([None])
    assert d4.scalar() is None


def test_scalar_bad():
    d0 = dt.Frame([100, 200])
    with pytest.raises(ValueError) as e:
        d0.scalar()
    assert (".scalar() method cannot be applied to a Frame with shape "
            "`[2 x 1]`" in str(e.value))


def test_scalar_on_view(dt0):
    assert dt0[1, 0] == 7
    assert dt0[3, 3] == 4.4
    assert dt0[2, 6] == "hello"
    assert dt0[2::5, 3::7].scalar() == -4
    assert dt0[[3], "G"].scalar() == "world"



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
    pt = (bool if st == dt.stype.bool8 else
          int if st.ltype == dt.ltype.int else
          float if st.ltype == dt.ltype.real else
          str if st.ltype == dt.ltype.str else
          object)
    src = [True, False, True, None] if pt is bool else \
          [1, 7, -99, 214, None, 3333] if pt is int else \
          [2.5, 3.4e15, -7.909, None] if pt is float else \
          ['Oh', 'gobbly', None, 'sproo'] if pt is str else \
          [dt, st, list, None, {3, 2, 1}]
    df = dt.Frame(A=src, stype=st)
    df.internal.check()
    assert df.names == ("A", )
    assert df.stypes == (st, )
    for i in range(len(src)):
        x = df[i, 0]
        y = df[i, "A"]
        assert x == y
        if src[i] is None:
            assert x is None
        else:
            assert isinstance(x, pt)
            if st == dt.stype.int8:
                assert (x - src[i]) % 256 == 0
            elif st == dt.stype.float32:
                assert abs(1 - src[i] / x) < 1e-7
            else:
                assert x == src[i]



#-------------------------------------------------------------------------------
# Frame copy
#-------------------------------------------------------------------------------

def test_copy_frame():
    d0 = dt.Frame(A=range(5),
                  B=["dew", None, "strab", "g", None],
                  C=[1.0 - i * 0.2 for i in range(5)],
                  D=[True, False, None, False, True])
    assert sorted(d0.names) == list("ABCD")
    d1 = d0.copy()
    d0.internal.check()
    d1.internal.check()
    assert d0.names == d1.names
    assert d0.stypes == d1.stypes
    assert d0.topython() == d1.topython()
    d0[1, "A"] = 100
    assert d0.topython() != d1.topython()
    d0.names = ("w", "x", "y", "z")
    assert d1.names != d0.names



#-------------------------------------------------------------------------------
# Misc
#-------------------------------------------------------------------------------

def test_internal_rowindex():
    d0 = dt.Frame(range(100))
    d1 = d0[:20, :]
    assert d0.internal.rowindex is None
    assert repr(d1.internal.rowindex) == "_RowIndex(0/20/1)"


def test_issue898():
    """
    Test that reification of obj column views does not lead to any disastrous
    results (previously this was crashing).
    """
    class A: pass
    f0 = dt.Frame([A() for i in range(1111)])
    assert f0.stypes == (dt.stype.obj64, )
    f1 = f0[:-1, :]
    del f0
    f1.materialize()
    res = f1.topython()
    del res
