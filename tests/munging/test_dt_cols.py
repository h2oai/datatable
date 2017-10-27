#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import pytest
import datatable as dt
from tests import same_iterables
from datatable import stype, ltype


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
    return dt.DataTable(tbl0, names=["A", "B", "C", "D"])

def as_list(datatable):
    nrows, ncols = datatable.shape
    return datatable.internal.window(0, nrows, 0, ncols).data

def assert_valueerror(datatable, cols, error_message):
    with pytest.raises(ValueError) as e:
        datatable(select=cols)
    assert str(e.type) == "<class 'dt.ValueError'>"
    assert error_message in str(e.value)

def assert_typeerror(datatable, cols, error_message):
    with pytest.raises(TypeError) as e:
        datatable(select=cols)
    assert str(e.type) == "<class 'dt.TypeError'>"
    assert error_message in str(e.value)


#-------------------------------------------------------------------------------
# Run the tests
#-------------------------------------------------------------------------------

def test_dt_properties(dt0):
    assert dt0.shape == (6, 4)
    assert dt0.names == ("A", "B", "C", "D")
    assert dt0.ltypes == (ltype.int, ltype.int, ltype.real, ltype.bool)


def test_cols_ellipsis(dt0):
    dt1 = dt0(select=...)
    assert dt1.shape == dt0.shape
    assert dt1.names == dt0.names
    assert dt1.stypes == dt0.stypes
    assert not dt1.internal.isview


def test_cols_none(dt0):
    dt1 = dt0(select=None)
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
        dt1 = dt0[i]
        assert dt1.shape == (6, 1)
        assert dt1.names == ("ABCD"[i], )
        assert not dt1.internal.isview
        assert as_list(dt1)[0] == tbl0[i]
    assert_valueerror(dt0, 4, "Column index `4` is invalid for a datatable "
                              "with 4 columns")
    assert_valueerror(dt0, -5, "Column index `-5` is invalid")


def test_cols_string(dt0, tbl0):
    for s in "ABCD":
        dt1 = dt0[s]
        assert dt1.shape == (6, 1)
        assert dt1.names == (s, )
        assert not dt1.internal.isview
        assert as_list(dt1)[0] == tbl0["ABCD".index(s)]
    assert_valueerror(dt0, "Z", "Column `Z` does not exist in <DataTable")


def test_cols_intslice(dt0, tbl0):
    """
    Test selector in the form of an integer slice:
        dt[2:5]
        dt[::-1]
    Such slice should select columns according to the standard Python's semantic
    as if it was applied to a list or a string. Note that using out-of-bounds
    indices is allowed, since that's Python's semantic too:
        dt[:1000]
    """
    assert as_list(dt0[:]) == tbl0
    assert as_list(dt0[:3]) == tbl0[:3]
    assert as_list(dt0[:-3]) == tbl0[:-3]
    assert as_list(dt0[1:]) == tbl0[1:]
    assert as_list(dt0[-2:]) == tbl0[-2:]
    assert as_list(dt0[::2]) == tbl0[::2]
    assert as_list(dt0[::-2]) == tbl0[::-2]
    for s in [slice(-1), slice(1, 4), slice(2, 2), slice(None, None, 2),
              slice(1000), slice(100, None), slice(-100, None), slice(-100),
              slice(1, None, -1), slice(None, None, -2), slice(None, None, -3)]:
        dt1 = dt0(select=s)
        assert as_list(dt1) == tbl0[s]
        assert dt1.names == dt0.names[s]
        assert dt1.stypes == dt0.stypes[s]
        assert not dt1.internal.isview


def test_cols_strslice(dt0, tbl0):
    """
    Test column selector in the form of a string slice:
        dt["B":]
        dt["D":"A"]
    These slices use the last column *inclusive* (i.e. `dt["A":"C"]` would
    select columns "A", "B" and "C"). Strided column selection is not allowed
    for string slices.
    """
    assert as_list(dt0["A":"D"]) == tbl0
    assert as_list(dt0["D":"A"]) == tbl0[::-1]
    assert as_list(dt0["B":]) == tbl0[1:]
    assert as_list(dt0[:"C"]) == tbl0[:3]
    assert_valueerror(dt0, slice("a", "D"), "Column `a` does not exist")
    assert_valueerror(dt0, slice("A", "D", 2),
                      "Column name slices cannot use strides")
    assert_valueerror(dt0, slice("A", 3),
                      "cannot mix numeric and string column names")


def test_cols_list(dt0, tbl0):
    """
    Test selecting multiple columns using a list of selectors:
        dt[[0, 3, 1, 2]]
    """
    dt1 = dt0[[3, 1, 0]]
    assert dt1.shape == (6, 3)
    assert dt1.names == ("D", "B", "A")
    assert not dt1.internal.isview
    assert as_list(dt1) == [tbl0[3], tbl0[1], tbl0[0]]
    dt2 = dt0(select=("D", "C", "B", "A"))
    assert dt2.shape == (6, 4)
    assert dt2.names == tuple("DCBA")
    assert not dt2.internal.isview
    assert as_list(dt2) == tbl0[::-1]
    dt3 = dt0(select=[slice(None, None, 2), slice(1, None, 2)])
    assert dt3.shape == (6, 4)
    assert dt3.names == tuple("ACBD")
    assert not dt3.internal.isview
    assert as_list(dt3) == [tbl0[0], tbl0[2], tbl0[1], tbl0[3]]


def test_cols_dict(dt0, tbl0):
    """
    Test selecting multiple columns using a dictionary:
        dt[{"x": "A", "y": "B"}]
    """
    dt1 = dt0(select={"x": 0, "y": "D"})
    assert dt1.internal.check()
    assert dt1.shape == (6, 2)
    assert same_iterables(dt1.names, ("x", "y"))
    assert not dt1.internal.isview
    assert same_iterables(as_list(dt1), [tbl0[0], tbl0[3]])
    dt2 = dt0[{"_": slice(None)}]
    assert dt2.internal.check()
    assert dt2.shape == (6, 4)
    assert dt2.names == ("_", "_1", "_2", "_3")
    assert not dt2.internal.isview
    assert as_list(dt2) == tbl0


def test_cols_colselector(dt0, tbl0):
    """
    Check that a "column selector" expression is equivalent to directly indexing
    the column:
        dt[lambda f: f.A]
    """
    dt1 = dt0(select=lambda f: f.B)
    assert dt1.internal.check()
    assert dt1.shape == (6, 1)
    assert dt1.names == ("B", )
    assert not dt1.internal.isview
    assert as_list(dt1) == [tbl0[1]]
    dt2 = dt0(select=lambda f: [f.A, f.C])
    assert dt2.internal.check()
    assert dt2.shape == (6, 2)
    assert dt2.names == ("A", "C")
    assert not dt2.internal.isview
    assert as_list(dt2) == [tbl0[0], tbl0[2]]
    dt3 = dt0[lambda f: {"x": f.A, "y": f.D}]
    assert dt3.internal.check()
    assert dt3.shape == (6, 2)
    assert same_iterables(dt3.names, ("x", "y"))
    assert not dt3.internal.isview


def test_cols_expression(dt0, tbl0):
    """
    Check that it is possible to select computed columns:
        dt[lambda f: [f.A + f.B]]
    """
    dt1 = dt0[lambda f: f.A + f.B]
    assert dt1.internal.check()
    assert dt1.shape == (6, 1)
    assert dt1.ltypes == (ltype.int, )
    assert as_list(dt1) == [[tbl0[0][i] + tbl0[1][i] for i in range(6)]]
    dt2 = dt0[lambda f: [f.A + f.B, f.C - f.D, f.A / f.C, f.B * f.D]]
    assert dt2.internal.check()
    assert dt2.shape == (6, 4)
    assert dt2.ltypes == (ltype.int, ltype.real, ltype.real, ltype.int)
    assert as_list(dt2) == [[tbl0[0][i] + tbl0[1][i] for i in range(6)],
                            [tbl0[2][i] - tbl0[3][i] for i in range(6)],
                            [tbl0[0][i] / tbl0[2][i] for i in range(6)],
                            [tbl0[1][i] * tbl0[3][i] for i in range(6)]]
    dt3 = dt0[lambda f: {"foo": f.A + f.B - f.C * 10, "a": f.A, "b": 1, "c": 2}]
    assert dt3.internal.check()
    assert dt3.shape == (6, 4)
    assert same_iterables(dt3.names, ("foo", "a", "b", "c"))
    assert same_iterables(dt3.ltypes,
                          (ltype.real, ltype.int, ltype.int, ltype.real))
    assert not dt3.internal.isview
    assert as_list(dt3["foo"]) == [[tbl0[0][i] + tbl0[1][i] - tbl0[2][i] * 10
                                    for i in range(6)]]


def test_cols_bad_arguments(dt0):
    """
    Check certain arguments that would be invalid as column selectors
    """
    assert_valueerror(dt0, 1.000001, "Unknown `select` argument: 1.000001")
    assert_valueerror(dt0, slice(1, 2, "A"),
                      "slice(1, 2, 'A') is not integer-valued")
    assert_typeerror(dt0, [0, 0.5, 1], "Unknown column format: 0.5")
    assert_typeerror(dt0, True, "A boolean cannot be used as a column selector")
    assert_typeerror(dt0, False,
                     "A boolean cannot be used as a column selector")


def test_cols_from_view(dt0):
    d1 = dt0[:3, :]
    assert d1.internal.isview
    d2 = d1["B"]
    assert d2.internal.check()
    assert d2.topython() == [[2, 3, 2]]
