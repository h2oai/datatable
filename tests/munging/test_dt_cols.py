#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2019 H2O.ai
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
import pytest
import datatable as dt
from tests import same_iterables, noop, isview, assert_equals
from datatable import ltype, stype, f
from datatable.internal import frame_integrity_check


#-------------------------------------------------------------------------------
# Prepare fixtures & helper functions
#-------------------------------------------------------------------------------

@pytest.fixture()
def tbl0():
    T, F = True, False
    return [
        [1,   7,   9,    0,   -4,   3],  # int
        [2,   3,   2,    1,   10,   0],  # int
        [0.5, 0.1, 0.7, -0.3,  0.1, 1],  # real
        [T,   F,   T,    T,    F,   T],  # bool
    ]

@pytest.fixture()
def dt0(tbl0):
    return dt.Frame(tbl0, names=["A", "B", "C", "D"])


@pytest.fixture()
def dt1():
    return dt.Frame(
        [[None]] * 10, names=list("ABCDEFGHIJ"),
        stypes=[stype.bool8, stype.int8, stype.int16, stype.int32,
                stype.int64, stype.float32, stype.float64,
                stype.str32, stype.str64, stype.obj64]
    )


def assert_valueerror(frame, cols, error_message):
    with pytest.raises(ValueError) as e:
        assert frame[:, cols]
    assert str(e.type) == "<class 'datatable.ValueError'>"
    assert error_message in str(e.value)

def assert_typeerror(frame, cols, error_message):
    with pytest.raises(TypeError) as e:
        noop(frame[:, cols])
    assert str(e.type) == "<class 'datatable.TypeError'>"
    assert error_message in str(e.value)


def test_dt_properties(dt0):
    assert dt0.shape == (6, 4)
    assert dt0.names == ("A", "B", "C", "D")
    assert dt0.ltypes == (ltype.int, ltype.int, ltype.real, ltype.bool)



#-------------------------------------------------------------------------------
# Basic tests
#-------------------------------------------------------------------------------

def test_j_ellipsis(dt0):
    dt1 = dt0[:, ...]
    assert dt1.shape == dt0.shape
    assert dt1.names == dt0.names
    assert dt1.stypes == dt0.stypes
    assert not isview(dt1)


def test_j_none(dt0):
    dt1 = dt0[:, None]
    assert dt1.shape == dt0.shape
    assert dt1.names == dt0.names
    assert dt1.stypes == dt0.stypes
    assert not isview(dt1)


def test_j_function(dt0):
    assert_typeerror(
        dt0, lambda r: r.A,
        "An object of type <class 'function'> cannot be used in an Expr")



#-------------------------------------------------------------------------------
# Integer-valued `j`
#
# Test column selectors which are simple integers, eg:
#     DT[:, -3]
# should select the 3rd column from the end
#-------------------------------------------------------------------------------

def test_j_integer(dt0, tbl0):
    for i in range(-4, 4):
        dt1 = dt0[:, i]
        assert dt1.shape == (6, 1)
        assert dt1.names == ("ABCD"[i], )
        assert not isview(dt1)
        assert dt1.to_list()[0] == tbl0[i]


def test_j_integer_wrong(dt0):
    assert_valueerror(
        dt0, 4,
        "Column index `4` is invalid for a Frame with 4 columns")
    assert_valueerror(
        dt0, -5,
        "Column index `-5` is invalid for a Frame with 4 columns")
    assert_typeerror(
        dt0, 10**30,
        "A floating-point value cannot be used as a column selector")



#-------------------------------------------------------------------------------
# String-valued `j`
#-------------------------------------------------------------------------------

def test_j_string(dt0, tbl0):
    for s in "ABCD":
        dt1 = dt0[:, s]
        assert dt1.shape == (6, 1)
        assert dt1.names == (s, )
        assert not isview(dt1)
        assert dt1.to_list()[0] == tbl0["ABCD".index(s)]


def test_j_string_error(dt0):
    assert_valueerror(
        dt0, "Z",
        "Column `Z` does not exist in the Frame; did you mean `A`, `B` or `C`?")


def test_j_bytes_error(dt0):
    assert_typeerror(
        dt0, b'A',
        "An object of type <class 'bytes'> cannot be used in an Expr")



#-------------------------------------------------------------------------------
# Integer slice
#
# Test selector in the form of an integer slice:
#     DT[:, 2:5]
#     DT[:, ::-1]
# Such slice should select columns according to the standard Python's semantic
# as if it was applied to a list or a string. Note that using out-of-bounds
# indices is allowed, since that's Python's semantic too:
#     DT[:, :1000]
#-------------------------------------------------------------------------------

def test_j_intslice1(dt0, tbl0):
    assert dt0[:, :].to_list() == tbl0
    assert dt0[:, :3].to_list() == tbl0[:3]
    assert dt0[:, :-3].to_list() == tbl0[:-3]
    assert dt0[:, 1:].to_list() == tbl0[1:]
    assert dt0[:, -2:].to_list() == tbl0[-2:]
    assert dt0[:, ::2].to_list() == tbl0[::2]
    assert dt0[:, ::-2].to_list() == tbl0[::-2]


@pytest.mark.parametrize('s', [slice(-1), slice(1, 4), slice(2, 2),
                               slice(None, None, 2), slice(1000),
                               slice(100, None), slice(-100, None), slice(-100),
                               slice(1, None, -1), slice(None, None, -2),
                               slice(None, None, -3)])
def test_j_intslice2(dt0, tbl0, s):
    dt1 = dt0[:, s]
    assert dt1.to_list() == tbl0[s]
    assert dt1.names == dt0.names[s]
    assert dt1.stypes == dt0.stypes[s]
    assert not isview(dt1)



#-------------------------------------------------------------------------------
# String slice
#
# Test column selector in the form of a string slice:
#     dt[:, "B":]
#     dt[:, "D":"A"]
# These slices use the last column *inclusive* (i.e. `dt["A":"C"]` would
# select columns "A", "B" and "C"). Strided column selection is not allowed
# for string slices.
#-------------------------------------------------------------------------------

def test_j_strslice1(dt0, tbl0):
    assert dt0[:, "A":"D"].to_list() == tbl0
    assert dt0[:, "D":"A"].to_list() == tbl0[::-1]
    assert dt0[:, "B":].to_list() == tbl0[1:]
    assert dt0[:, :"C"].to_list() == tbl0[:3]


def test_j_slice_error1(dt0):
    assert_valueerror(
        dt0, slice("a", "D"),
        "Column `a` does not exist in the Frame; did you mean `A`")


@pytest.mark.parametrize("s", [slice("A", "D", 2), slice("A", 3),
                               slice(0, 3, "A"), slice(0, 2, 0.5),
                               slice(None, None, "B")])
def test_j_slice_error2(dt0, s):
    assert_typeerror(
        dt0, s,
        str(s) + " is neither integer- nor string- valued")



#-------------------------------------------------------------------------------
# Type selector
#-------------------------------------------------------------------------------

def test_j_select_by_type1(dt0):
    dt1 = dt0[:, int]
    assert dt1.shape == (6, 2)
    assert dt1.ltypes == (dt.ltype.int, dt.ltype.int)
    assert dt1.names == ("A", "B")


def test_j_select_by_type2(dt0):
    dt2 = dt0[:, float]
    assert dt2.shape == (6, 1)
    assert dt2.stypes == (dt.stype.float64, )
    assert dt2.names == ("C", )


def test_j_select_by_type3(dt0):
    dt3 = dt0[:, dt.ltype.str]
    assert dt3.shape == (6, 0)
    assert dt3.to_list() == []


def test_j_select_by_type4(dt0):
    dt4 = dt0[:, dt.stype.bool8]
    assert dt4.shape == (6, 1)
    assert dt4.stypes == (dt.stype.bool8, )
    assert dt4.names == ("D", )


@pytest.mark.parametrize("t", [bool, int, float, str, object])
def test_j_type(t, dt1):
    DT2 = dt1[:, t]
    sl2 = (slice(0, 1) if t == bool else
           slice(1, 5) if t == int else
           slice(5, 7) if t == float else
           slice(7, 9) if t == str else
           slice(9, 10))
    assert DT2.names == dt1.names[sl2]
    assert DT2.stypes == dt1.stypes[sl2]


@pytest.mark.parametrize("t", ltype)
def test_j_ltype(t, dt1):
    if t == ltype.time:
        return
    DT2 = dt1[:, t]
    sl2 = (slice(0, 1) if t == ltype.bool else
           slice(1, 5) if t == ltype.int else
           slice(5, 7) if t == ltype.real else
           slice(7, 9) if t == ltype.str else
           slice(9, 10) if t == ltype.obj else slice(0, 0))
    assert DT2.names == dt1.names[sl2]
    assert DT2.stypes == dt1.stypes[sl2]


@pytest.mark.parametrize("t", stype)
def test_j_stype(t, dt1):
    DT2 = dt1[:, t]
    if t in dt1.stypes:
        i = dt1.stypes.index(t)
        assert DT2.names == (dt1.names[i],)
        assert DT2.stypes == (t,)
    else:
        assert DT2.ncols == 0


def test_j_type_bad(dt0):
    assert_valueerror(
        dt0, type,
        "Unknown type <class 'type'> used as a column selector")



#-------------------------------------------------------------------------------
# List[int] selector
#
# Test selecting multiple columns using a list/tuple of integers or integer
# slices:
#     dt[:, [0, 3, 1, 2]]
#-------------------------------------------------------------------------------

def test_j_intlist1(dt0, tbl0):
    dt1 = dt0[:, [3, 1, 0]]
    assert dt1.shape == (6, 3)
    assert dt1.names == ("D", "B", "A")
    assert not isview(dt1)
    assert dt1.to_list() == [tbl0[3], tbl0[1], tbl0[0]]


def test_j_intlist2(dt0, tbl0):
    df1 = dt0[:, [slice(None, None, 2), slice(1, None, 2)]]
    assert df1.shape == (6, 4)
    assert df1.names == tuple("ACBD")
    assert not isview(df1)
    assert df1.to_list() == [tbl0[0], tbl0[2], tbl0[1], tbl0[3]]


def test_j_intlist3(dt0, tbl0):
    df1 = dt0[:, [3, 1, slice(None, None, 2)]]
    assert df1.shape == (6, 4)
    assert df1.names == ("D", "B", "A", "C")
    assert df1.to_list() == [tbl0[3], tbl0[1], tbl0[0], tbl0[2]]


def test_j_intlist_mixed(dt0):
    assert_typeerror(
        dt0, [2, "A"],
        "Mixed selector types are not allowed. Element 1 is of type string, "
        "whereas the previous element(s) were of type integer")




#-------------------------------------------------------------------------------
# List[str] selector
#-------------------------------------------------------------------------------

def test_j_strlist1(dt0, tbl0):
    df1 = dt0[:, ("D", "C", "B", "A")]
    assert df1.shape == (6, 4)
    assert df1.names == tuple("DCBA")
    assert not isview(df1)
    assert df1.to_list() == tbl0[::-1]


def test_j_strlist2(dt0, tbl0):
    df1 = dt0[:, (a.upper() for a in 'cda')]
    assert df1.shape == (6, 3)
    assert df1.names == ("C", "D", "A")
    assert df1.to_list() == [tbl0[2], tbl0[3], tbl0[0]]


def test_j_strlist_mixed(dt0):
    assert_typeerror(
        dt0, ["A", f.B],
        "Mixed selector types are not allowed. Element 1 is of type expression,"
        " whereas the previous element(s) were of type string")



#-------------------------------------------------------------------------------
# List[bool] selector
#-------------------------------------------------------------------------------

def test_j_list_bools(dt0, tbl0):
    df1 = dt0[:, [True, False, False, True]]
    assert df1.shape == (6, 2)
    assert df1.names == ("A", "D")
    assert df1.to_list() == [tbl0[0], tbl0[3]]


def test_j_list_bools_error1(dt0):
    assert_valueerror(
        dt0, [True] * 5,
        "The length of boolean list in `j` selector does not match the number "
        "of columns in the Frame")


def test_j_list_bools_mixed(dt0):
    assert_typeerror(
        dt0, [True, False, [], True],
        "Mixed selector types are not allowed. Element 2 is of type ?, whereas "
        "the previous element(s) were of type bool")



#-------------------------------------------------------------------------------
# List[type] selector
#-------------------------------------------------------------------------------

def test_j_list_types(dt0, tbl0):
    df1 = dt0[:, [int, float]]
    assert df1.shape == (6, 3)
    assert df1.names == ("A", "B", "C")
    assert df1.to_list() == tbl0[:3]


def test_j_list_types2(dt0, tbl0):
    df1 = dt0[:, [stype.bool8, float]]
    assert df1.shape == (6, 2)
    assert df1.names == ("D", "C")
    assert df1.to_list() == [tbl0[3], tbl0[2]]




#-------------------------------------------------------------------------------
# Dict selector
#
# Test selecting multiple columns using a dictionary:
#     dt[:, {"x": f.A, "y": f.B}]
#-------------------------------------------------------------------------------

def test_j_dict(dt0, tbl0):
    dt1 = dt0[:, {"x": f[0], "y": f["D"]}]
    frame_integrity_check(dt1)
    assert dt1.shape == (6, 2)
    assert same_iterables(dt1.names, ("x", "y"))
    assert not isview(dt1)
    assert same_iterables(dt1.to_list(), [tbl0[0], tbl0[3]])


def test_j_dict_bad1(dt0):
    assert_typeerror(
        dt0, {1: f.A, 2: f.B + f.C},
        "Keys in the dictionary must be strings")


def test_j_dict_of_literals(dt0):
    DT1 = dt0[:, {"a": "A", "b": "B", "c": 7.5}]
    assert_equals(DT1, dt.Frame(a=["A"], b=["B"], c=[7.5]))




#-------------------------------------------------------------------------------
# Expr selector
#
# Check that it is possible to select computed columns:
#     dt[:, [f.A + f.B]]
#-------------------------------------------------------------------------------

def test_j_colselector1(dt0, tbl0):
    dt1 = dt0[:, f.B]
    frame_integrity_check(dt1)
    assert dt1.shape == (6, 1)
    assert dt1.names == ("B", )
    assert not isview(dt1)
    assert dt1.to_list() == [tbl0[1]]


def test_j_colselector2(dt0, tbl0):
    dt2 = dt0[:, [f.A, f.C]]
    frame_integrity_check(dt2)
    assert dt2.shape == (6, 2)
    assert dt2.names == ("A", "C")
    assert not isview(dt2)
    assert dt2.to_list() == [tbl0[0], tbl0[2]]


def test_j_colselector3(dt0, tbl0):
    dt3 = dt0[:, {"x": f.A, "y": f.D}]
    frame_integrity_check(dt3)
    assert dt3.shape == (6, 2)
    assert same_iterables(dt3.names, ("x", "y"))
    assert not isview(dt3)


def test_j_expression(dt0, tbl0):
    dt1 = dt0[:, f.A + f.B]
    frame_integrity_check(dt1)
    assert dt1.shape == (6, 1)
    assert dt1.ltypes == (ltype.int, )
    assert dt1.to_list() == [[tbl0[0][i] + tbl0[1][i] for i in range(6)]]
    dt2 = dt0[:, [f.A + f.B, f.C - f.D, f.A / f.C, f.B * f.D]]
    frame_integrity_check(dt2)
    assert dt2.shape == (6, 4)
    assert dt2.ltypes == (ltype.int, ltype.real, ltype.real, ltype.int)
    assert dt2.to_list() == [[tbl0[0][i] + tbl0[1][i] for i in range(6)],
                            [tbl0[2][i] - tbl0[3][i] for i in range(6)],
                            [tbl0[0][i] / tbl0[2][i] for i in range(6)],
                            [tbl0[1][i] * tbl0[3][i] for i in range(6)]]
    dt3 = dt0[:, {"foo": f.A + f.B - f.C * 10, "a": f.A, "b": f[1], "c": f[2]}]
    frame_integrity_check(dt3)
    assert dt3.shape == (6, 4)
    assert same_iterables(dt3.names, ("foo", "a", "b", "c"))
    assert same_iterables(dt3.ltypes,
                          (ltype.real, ltype.int, ltype.int, ltype.real))
    assert dt3[:, "foo"].to_list() == [[tbl0[0][i] + tbl0[1][i] - tbl0[2][i] * 10
                                       for i in range(6)]]


def test_j_expression2():
    # In Py3.6+ we could have used a regular dictionary here...
    from collections import OrderedDict
    selector = OrderedDict([("foo", f.A), ("bar", -f.A)])
    f0 = dt.Frame({"A": range(10)})
    f1 = f0[:, selector]
    frame_integrity_check(f1)
    assert f1.names == ("foo", "bar")
    assert f1.stypes == f0.stypes * 2
    assert f1.to_list() == [list(range(10)), list(range(0, -10, -1))]




#-------------------------------------------------------------------------------
# Special cases
#-------------------------------------------------------------------------------

def test_j_bad_arguments(dt0):
    """
    Check certain arguments that would be invalid as column selectors
    """
    assert_typeerror(
        dt0, 1.000001,
        "A floating-point value cannot be used as a column selector")
    assert_typeerror(dt0,
        slice(1, 2, "A"),
        "slice(1, 2, 'A') is neither integer- nor string- valued")
    assert_typeerror(
        dt0, [0, 0.5, 1],
        "A floating value cannot be used as a column selector")
    assert_typeerror(
        dt0, True,
        "A boolean value cannot be used as a column selector")
    assert_typeerror(
        dt0, False,
        "A boolean value cannot be used as a column selector")


def test_j_from_view(dt0):
    d1 = dt0[:3, :]
    assert isview(d1)
    d2 = d1[:, "B"]
    frame_integrity_check(d2)
    assert d2.to_list() == [[2, 3, 2]]


def test_j_on_empty_frame():
    df = dt.Frame()
    d1 = df[:, :]
    d2 = df[::-1, :5]
    frame_integrity_check(d1)
    frame_integrity_check(d2)
    assert d1.shape == (0, 0)
    assert d2.shape == (0, 0)


def test_empty_selector1(dt0):
    d1 = dt0[:, []]
    frame_integrity_check(d1)
    assert d1.nrows == dt0.nrows
    assert d1.ncols == 0


def test_empty_selector2(dt0):
    d1 = dt0[-3:, []]
    frame_integrity_check(d1)
    assert d1.nrows == 3
    assert d1.ncols == 0
