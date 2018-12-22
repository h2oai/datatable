#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
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
from tests import same_iterables, has_llvm
from datatable import ltype, f


#-------------------------------------------------------------------------------
# Prepare fixtures & helper functions
#-------------------------------------------------------------------------------

@pytest.fixture()
def tbl0():
    return [
        [1,   7,   9,    0,   -4,   3],  # int
        [2,   3,   2,    1,   10,   0],  # int
        [0.5, 0.1, 0.7, -0.3,  0.1, 1],  # real
        [1,   0,   1,    1,    0,   1],  # bool
    ]

@pytest.fixture()
def dt0(tbl0):
    return dt.Frame(tbl0, names=["A", "B", "C", "D"])


def assert_valueerror(datatable, cols, error_message):
    with pytest.raises(ValueError) as e:
        datatable[:, cols]
    assert str(e.type) == "<class 'datatable.ValueError'>"
    assert error_message in str(e.value)

def assert_typeerror(datatable, cols, error_message):
    with pytest.raises(TypeError) as e:
        datatable[:, cols]
    assert str(e.type) == "<class 'datatable.TypeError'>"
    assert error_message in str(e.value)



#-------------------------------------------------------------------------------
# Run the tests
#-------------------------------------------------------------------------------

def test_dt_properties(dt0):
    assert dt0.shape == (6, 4)
    assert dt0.names == ("A", "B", "C", "D")
    assert dt0.ltypes == (ltype.int, ltype.int, ltype.real, ltype.bool)


def test_cols_ellipsis(dt0):
    dt1 = dt0[:, ...]
    assert dt1.shape == dt0.shape
    assert dt1.names == dt0.names
    assert dt1.stypes == dt0.stypes
    assert not dt1.internal.isview


def test_cols_none(dt0):
    dt1 = dt0[:, None]
    assert dt1.shape == dt0.shape
    assert dt1.names == dt0.names
    assert dt1.stypes == dt0.stypes
    assert not dt1.internal.isview


def test_cols_integer(dt0, tbl0):
    """
    Test column selectors which are simple integers, eg:
        dt(-3)
    should select the 3rd column from the end
    """
    for i in range(-4, 4):
        dt1 = dt0[:, i]
        assert dt1.shape == (6, 1)
        assert dt1.names == ("ABCD"[i], )
        assert not dt1.internal.isview
        assert dt1.to_list()[0] == tbl0[i]
    assert_valueerror(
        dt0, 4,
        "Column index `4` is invalid for a Frame with 4 columns")
    assert_valueerror(
        dt0, -5,
        "Column index `-5` is invalid for a Frame with 4 columns")


def test_cols_string(dt0, tbl0):
    for s in "ABCD":
        dt1 = dt0[:, s]
        assert dt1.shape == (6, 1)
        assert dt1.names == (s, )
        assert not dt1.internal.isview
        assert dt1.to_list()[0] == tbl0["ABCD".index(s)]
    assert_valueerror(dt0, "Z", "Column `Z` does not exist in the Frame")


def test_cols_intslice(dt0, tbl0):
    """
    Test selector in the form of an integer slice:
        dt[:, 2:5]
        dt[:, ::-1]
    Such slice should select columns according to the standard Python's semantic
    as if it was applied to a list or a string. Note that using out-of-bounds
    indices is allowed, since that's Python's semantic too:
        dt[:, :1000]
    """
    assert dt0[:, :].to_list() == tbl0
    assert dt0[:, :3].to_list() == tbl0[:3]
    assert dt0[:, :-3].to_list() == tbl0[:-3]
    assert dt0[:, 1:].to_list() == tbl0[1:]
    assert dt0[:, -2:].to_list() == tbl0[-2:]
    assert dt0[:, ::2].to_list() == tbl0[::2]
    assert dt0[:, ::-2].to_list() == tbl0[::-2]
    for s in [slice(-1), slice(1, 4), slice(2, 2), slice(None, None, 2),
              slice(1000), slice(100, None), slice(-100, None), slice(-100),
              slice(1, None, -1), slice(None, None, -2), slice(None, None, -3)]:
        dt1 = dt0[:, s]
        assert dt1.to_list() == tbl0[s]
        assert dt1.names == dt0.names[s]
        assert dt1.stypes == dt0.stypes[s]
        assert not dt1.internal.isview


def test_cols_strslice(dt0, tbl0):
    """
    Test column selector in the form of a string slice:
        dt[:, "B":]
        dt[:, "D":"A"]
    These slices use the last column *inclusive* (i.e. `dt["A":"C"]` would
    select columns "A", "B" and "C"). Strided column selection is not allowed
    for string slices.
    """
    assert dt0[:, "A":"D"].to_list() == tbl0
    assert dt0[:, "D":"A"].to_list() == tbl0[::-1]
    assert dt0[:, "B":].to_list() == tbl0[1:]
    assert dt0[:, :"C"].to_list() == tbl0[:3]
    assert_valueerror(
        dt0, slice("a", "D"),
        "Column `a` does not exist in the Frame; did you mean `A`")
    assert_typeerror(
        dt0, slice("A", "D", 2),
        "slice('A', 'D', 2) is neither integer- nor string-valued")
    assert_typeerror(
        dt0, slice("A", 3),
        "slice('A', 3, None) is neither integer- nor string-valued")


def test_cols_list(dt0, tbl0):
    """
    Test selecting multiple columns using a list of selectors:
        dt[:, [0, 3, 1, 2]]
    """
    dt1 = dt0[:, [3, 1, 0]]
    assert dt1.shape == (6, 3)
    assert dt1.names == ("D", "B", "A")
    assert not dt1.internal.isview
    assert dt1.to_list() == [tbl0[3], tbl0[1], tbl0[0]]
    dt2 = dt0[:, ("D", "C", "B", "A")]
    assert dt2.shape == (6, 4)
    assert dt2.names == tuple("DCBA")
    assert not dt2.internal.isview
    assert dt2.to_list() == tbl0[::-1]
    dt3 = dt0[:, [slice(None, None, 2), slice(1, None, 2)]]
    assert dt3.shape == (6, 4)
    assert dt3.names == tuple("ACBD")
    assert not dt3.internal.isview
    assert dt3.to_list() == [tbl0[0], tbl0[2], tbl0[1], tbl0[3]]


def test_cols_select_by_type(dt0):
    dt1 = dt0[:, int]
    assert dt1.shape == (6, 2)
    assert dt1.ltypes == (dt.ltype.int, dt.ltype.int)
    assert dt1.names == ("A", "B")
    dt2 = dt0[:, float]
    assert dt2.shape == (6, 1)
    assert dt2.stypes == (dt.stype.float64, )
    assert dt2.names == ("C", )
    dt3 = dt0[:, dt.ltype.str]
    assert dt3.shape == (0, 0)
    assert dt3.to_list() == []
    dt4 = dt0[:, dt.stype.bool8]
    assert dt4.shape == (6, 1)
    assert dt4.stypes == (dt.stype.bool8, )
    assert dt4.names == ("D", )


def test_cols_dict(dt0, tbl0):
    """
    Test selecting multiple columns using a dictionary:
        dt[:, {"x": "A", "y": "B"}]
    """
    dt1 = dt0[:, {"x": f[0], "y": f["D"]}]
    dt1.internal.check()
    assert dt1.shape == (6, 2)
    assert same_iterables(dt1.names, ("x", "y"))
    assert not dt1.internal.isview
    assert same_iterables(dt1.to_list(), [tbl0[0], tbl0[3]])


def test_cols_colselector(dt0, tbl0):
    """
    Check that a "column selector" expression is equivalent to directly indexing
    the column:
        dt[:, f.A]
    """
    dt1 = dt0[:, f.B]
    dt1.internal.check()
    assert dt1.shape == (6, 1)
    assert dt1.names == ("B", )
    assert not dt1.internal.isview
    assert dt1.to_list() == [tbl0[1]]
    dt2 = dt0[:, [f.A, f.C]]
    dt2.internal.check()
    assert dt2.shape == (6, 2)
    assert dt2.names == ("A", "C")
    assert not dt2.internal.isview
    assert dt2.to_list() == [tbl0[0], tbl0[2]]
    dt3 = dt0[:, {"x": f.A, "y": f.D}]
    dt3.internal.check()
    assert dt3.shape == (6, 2)
    assert same_iterables(dt3.names, ("x", "y"))
    assert not dt3.internal.isview


def test_cols_expression(dt0, tbl0):
    """
    Check that it is possible to select computed columns:
        dt[:, [f.A + f.B]]
    """
    dt1 = dt0[:, f.A + f.B]
    dt1.internal.check()
    assert dt1.shape == (6, 1)
    assert dt1.ltypes == (ltype.int, )
    assert dt1.to_list() == [[tbl0[0][i] + tbl0[1][i] for i in range(6)]]
    dt2 = dt0[:, [f.A + f.B, f.C - f.D, f.A / f.C, f.B * f.D]]
    dt2.internal.check()
    assert dt2.shape == (6, 4)
    assert dt2.ltypes == (ltype.int, ltype.real, ltype.real, ltype.int)
    assert dt2.to_list() == [[tbl0[0][i] + tbl0[1][i] for i in range(6)],
                            [tbl0[2][i] - tbl0[3][i] for i in range(6)],
                            [tbl0[0][i] / tbl0[2][i] for i in range(6)],
                            [tbl0[1][i] * tbl0[3][i] for i in range(6)]]
    dt3 = dt0[:, {"foo": f.A + f.B - f.C * 10, "a": f.A, "b": f[1], "c": f[2]}]
    dt3.internal.check()
    assert dt3.shape == (6, 4)
    assert same_iterables(dt3.names, ("foo", "a", "b", "c"))
    assert same_iterables(dt3.ltypes,
                          (ltype.real, ltype.int, ltype.int, ltype.real))
    assert not dt3.internal.isview
    assert dt3[:, "foo"].to_list() == [[tbl0[0][i] + tbl0[1][i] - tbl0[2][i] * 10
                                       for i in range(6)]]


def test_cols_expression2():
    # In Py3.6+ we could have used a regular dictionary here...
    from collections import OrderedDict
    selector = OrderedDict([("foo", f.A), ("bar", -f.A)])
    f0 = dt.Frame({"A": range(10)})
    f1 = f0[:, selector]
    f1.internal.check()
    assert f1.names == ("foo", "bar")
    assert f1.stypes == f0.stypes * 2
    assert f1.to_list() == [list(range(10)), list(range(0, -10, -1))]
    # if has_llvm():
    #     f2 = f0(select=selector, engine="llvm")
    #     f2.internal.check()
    #     assert f2.stypes == f1.stypes
    #     assert f2.names == f1.names
    #     assert f2.to_list() == f1.to_list()


def test_cols_bad_arguments(dt0):
    """
    Check certain arguments that would be invalid as column selectors
    """
    assert_typeerror(
        dt0, 1.000001,
        "Unsupported `j` selector of type <class 'float'>")
    assert_typeerror(dt0,
        slice(1, 2, "A"),
        "slice(1, 2, 'A') is neither integer- nor string-valued")
    assert_typeerror(
        dt0, [0, 0.5, 1],
        "Element 1 in the `j` selector list has type `<class 'float'>`")
    assert_typeerror(
        dt0, True,
        "Unsupported `j` selector of type <class 'bool'>")
    assert_typeerror(
        dt0, False,
        "Unsupported `j` selector of type <class 'bool'>")


def test_cols_from_view(dt0):
    d1 = dt0[:3, :]
    assert d1.internal.isview
    d2 = d1[:, "B"]
    d2.internal.check()
    assert d2.to_list() == [[2, 3, 2]]


def test_cols_on_empty_frame():
    df = dt.Frame()
    d1 = df[:, :]
    d2 = df[::-1, :5]
    d1.internal.check()
    d2.internal.check()
    assert d1.shape == (0, 0)
    assert d2.shape == (0, 0)
