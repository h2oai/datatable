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
import math
import pytest
import types
import datatable as dt
from tests import assert_equals
from datatable import stype, DatatableWarning
from datatable.internal import frame_integrity_check


#-------------------------------------------------------------------------------
# rbind() API
#-------------------------------------------------------------------------------

def test_rbind_exists():
    dt0 = dt.Frame([1, 2, 3])
    assert isinstance(dt0.rbind, types.BuiltinMethodType)
    assert len(dt0.rbind.__doc__) > 1700


def test_rbind_simple():
    dt0 = dt.Frame([1, 2, 3])
    dt1 = dt.Frame([5, 9])
    q = dt0.rbind(dt1)
    assert_equals(dt0, dt.Frame([1, 2, 3, 5, 9]))
    assert q is None


def test_rbind_empty():
    dt0 = dt.Frame([1, 2, 3])
    dt0.rbind()
    dt0.rbind(dt.Frame())
    assert_equals(dt0, dt.Frame([1, 2, 3]))


def test_rbind_array():
    dt0 = dt.Frame(range(5))
    dt0.rbind([dt.Frame(range(3)), dt.Frame(range(4))])
    assert dt0.to_list() == [[0, 1, 2, 3, 4, 0, 1, 2, 0, 1, 2, 3]]


def test_rbind_array2():
    dt0 = dt.Frame([0, 1, 2, 3])
    dt0.rbind([dt.Frame([4])], dt.Frame([5]),
              [dt.Frame([6, 7, 8]), dt.Frame([9])])
    assert dt0.to_list() == [list(range(10))]


def test_rbind_columns_mismatch():
    dt0 = dt.Frame({"A": [1, 2, 3]})
    dt1 = dt.Frame({"A": [5], "B": [7]})
    dt2 = dt.Frame({"C": [10, -1]})

    with pytest.raises(ValueError) as e:
        dt0.rbind(dt1)
    assert "Cannot rbind frame with 2 columns to a frame " \
           "with 1 column" in str(e.value)
    assert "`force=True`" in str(e.value)

    with pytest.raises(ValueError) as e:
        dt0.rbind(dt1, bynames=False)
    assert "Cannot rbind frame with 2 columns to a frame " \
           "with 1 column" in str(e.value)
    assert "`force=True`" in str(e.value)

    with pytest.raises(ValueError) as e:
        dt1.rbind(dt0)
    assert "Cannot rbind frame with 1 column to a frame " \
           "with 2 columns" in str(e.value)
    assert "`force=True`" in str(e.value)

    with pytest.raises(ValueError) as e:
        dt0.rbind(dt2)
    assert "Column `C` is not found in the original frame" in str(e.value)
    assert "`force=True`" in str(e.value)


def test_rbind_error1():
    dt0 = dt.Frame(range(5))
    with pytest.raises(TypeError) as e:
        dt0.rbind(123)
    assert ("`Frame.rbind()` expects a list or sequence of Frames as an "
            "argument; instead item 0 was a <class 'int'>" == str(e.value))


def test_rbind_error2():
    dt0 = dt.Frame(range(5))
    with pytest.raises(TypeError) as e:
        dt0.rbind(dt.Frame(range(1)), [dt.Frame(range(4)), 123])
    assert ("`Frame.rbind()` expects a list or sequence of Frames as an "
            "argument; instead item 2 was a <class 'int'>" == str(e.value))



#-------------------------------------------------------------------------------
# Simple test cases
#-------------------------------------------------------------------------------

def test_rbind_bynumbers1():
    dt0 = dt.Frame([[1, 2, 3], [7, 7, 0]], names=["A", "V"])
    dt1 = dt.Frame([[10, -1], [3, -1]], names=["C", "Z"])
    dtr = dt.Frame([[1, 2, 3, 10, -1], [7, 7, 0, 3, -1]],
                   names=["A", "V"])
    dt0.rbind(dt1, bynames=False)
    assert_equals(dt0, dtr)


def test_rbind_bynumbers2():
    dt0 = dt.Frame({"X": [8]})
    dt1 = dt.Frame([[10, -1], [3, -1]], names=["C", "Z"])
    dtr = dt.Frame({"X": [8, 10, -1], "Z": [None, 3, -1]})
    dt0.rbind(dt1, bynames=False, force=True)
    assert_equals(dt0, dtr)


def test_rbind_bynames1():
    dt0 = dt.Frame({"A": [1, 2, 3], "V": [7, 7, 0]})
    dt1 = dt.Frame({"V": [13], "A": [77]})
    dtr = dt.Frame({"A": [1, 2, 3, 77], "V": [7, 7, 0, 13]})
    dt0.rbind(dt1)
    assert_equals(dt0, dtr)


def test_rbind_bynames2():
    dt0 = dt.Frame({"A": [5, 1]})
    dt1 = dt.Frame({"C": [4], "D": [10]})
    dt2 = dt.Frame({"A": [-1], "D": [4]})
    dtr = dt.Frame({"A": [5, 1, None, -1],
                    "C": [None, None, 4, None],
                    "D": [None, None, 10, 4]})
    dt0.rbind(dt1, dt2, force=True)
    assert_equals(dt0, dtr)


def test_rbind_bynames3():
    dt0 = dt.Frame({"A": [7, 4], "B": [-1, 1]})
    dt1 = dt.Frame({"A": [3]})
    dt2 = dt.Frame({"B": [4]})
    dtr = dt.Frame({"A": [7, 4, 3, None], "B": [-1, 1, None, 4]})
    dt0.rbind(dt1, force=True)
    dt0.rbind(dt2, force=True)
    assert_equals(dt0, dtr)


def test_rbind_bynames4():
    dt0 = dt.Frame({"A": [13]})
    dt1 = dt.Frame({"B": [6], "A": [3], "E": [7]})
    dtr = dt.Frame({"A": [13, 3], "B": [None, 6], "E": [None, 7]})
    dt0.rbind(dt1, force=True)
    assert_equals(dt0, dtr)


def test_not_inplace():
    dt0 = dt.Frame({"A": [5, 1], "B": [4, 4]})
    dt1 = dt.Frame({"A": [22], "B": [11]})
    dtr = dt.rbind(dt0, dt1)
    assert_equals(dtr, dt.Frame({"A": [5, 1, 22], "B": [4, 4, 11]}))
    assert_equals(dt0, dt.Frame({"A": [5, 1], "B": [4, 4]}))


def test_repeating_names():
    # Warnings about repeated names -- ignore
    with pytest.warns(DatatableWarning):
        dt0 = dt.Frame([[5], [6], [7], [4]], names=["x", "y", "x", "x"])
        dt1 = dt.Frame([[4], [3], [2]], names=["y", "x", "x"])
        dtr = dt.Frame([[5, 3], [6, 4], [7, 2], [4, None]],
                       names=["x", "y", "x.0", "x.1"])
        dt0.rbind(dt1, force=True)
        assert_equals(dt0, dtr)

        dt0 = dt.Frame({"a": [23]})
        dt1 = dt.Frame([[2], [4], [8]], names=["a"] * 3)
        dtr = dt.Frame([[23, 2], [None, 4], [None, 8]], names=("a",) * 3)
        dt0.rbind(dt1, force=True)
        assert_equals(dt0, dtr)

        dt0 = dt.Frame([[22], [44], [88]], names=list("aba"))
        dt1 = dt.Frame([[2], [4], [8]], names=list("aaa"))
        dtr = dt.Frame([[22, 2], [44, None], [88, 4], [None, 8]],
                       names=["a", "b", "a", "a"])
        dt0.rbind(dt1, force=True)
        assert_equals(dt0, dtr)


def test_rbind_strings1():
    dt0 = dt.Frame({"A": ["Nothing's", "wrong", "with", "this", "world"]})
    dt1 = dt.Frame({"A": ["something", "wrong", "with", "humans"]})
    dt0.rbind(dt1)
    dtr = dt.Frame({"A": ["Nothing's", "wrong", "with", "this", "world",
                          "something", "wrong", "with", "humans"]})
    assert_equals(dt0, dtr)


def test_rbind_strings2():
    dt0 = dt.Frame(["a", "bc", None])
    dt1 = dt.Frame([None, "def", "g", None])
    dt0.rbind(dt1)
    dtr = dt.Frame(["a", "bc", None, None, "def", "g", None])
    assert_equals(dt0, dtr)


def test_rbind_strings3():
    dt0 = dt.Frame({"A": [1, 5], "B": ["ham", "eggs"]})
    dt1 = dt.Frame({"A": [25], "C": ["spam"]})
    dt0.rbind(dt1, force=True)
    dtr = dt.Frame({"A": [1, 5, 25], "B": ["ham", "eggs", None],
                    "C": [None, None, "spam"]})
    assert_equals(dt0, dtr)


def test_rbind_strings4():
    dt0 = dt.Frame({"A": ["alpha", None], "C": ["eta", "theta"]})
    dt1 = dt.Frame({"A": [None, "beta"], "B": ["gamma", "delta"]})
    dt2 = dt.Frame({"D": ["psi", "omega"]})
    dt0.rbind(dt1, dt2, force=True)
    dtr = dt.Frame({"A": ["alpha", None, None, "beta", None, None],
                    "C": ["eta", "theta", None, None, None, None],
                    "B": [None, None, "gamma", "delta", None, None],
                    "D": [None, None, None, None, "psi", "omega"]})
    assert_equals(dt0, dtr)


def test_rbind_strings5():
    f0 = dt.Frame([1, 2, 3])
    f1 = dt.Frame(["foo", "bra"])
    f1.rbind(f0)
    assert f1.to_list() == [["foo", "bra", "1", "2", "3"]]




#-------------------------------------------------------------------------------
# Advanced test cases
#-------------------------------------------------------------------------------

def test_rbind_self():
    dt0 = dt.Frame({"A": [1, 5, 7], "B": ["one", "two", None]})
    dt0.rbind(dt0, dt0, dt0)
    dtr = dt.Frame({"A": [1, 5, 7] * 4, "B": ["one", "two", None] * 4})
    assert_equals(dt0, dtr)


def test_rbind_mmapped(tempfile_jay):
    dt0 = dt.Frame({"A": [1, 5, 7], "B": ["one", "two", None]})
    dt0.to_jay(tempfile_jay)
    del dt0
    dt1 = dt.fread(tempfile_jay)
    dt2 = dt.Frame({"A": [-1], "B": ["zero"]})
    dt1.rbind(dt2)
    dtr = dt.Frame({"A": [1, 5, 7, -1], "B": ["one", "two", None, "zero"]})
    assert_equals(dt1, dtr)


def test_rbind_views0():
    # view + data
    dt0 = dt.Frame({"d": range(10), "s": list("abcdefghij")})
    dt0 = dt0[3:7, :]
    dt1 = dt.Frame({"d": [-1, -2], "s": ["the", "end"]})
    dt0.rbind(dt1)
    dtr = dt.Frame([[3, 4, 5, 6, -1, -2],
                    ["d", "e", "f", "g", "the", "end"]],
                   names=["d", "s"], stypes=[stype.int32, stype.str32])
    assert_equals(dt0, dtr)


def test_rbind_views1():
    # data + view
    dt0 = dt.Frame({"A": [1, 1, 2, 3, 5],
                    "B": ["a", "b", "c", "d", "e"]})
    dt1 = dt.Frame({"A": [8, 13, 21], "B": ["x", "y", "z"]})
    dt0.rbind(dt1[[0, -1], :])
    dtr = dt.Frame({"A": [1, 1, 2, 3, 5, 8, 21],
                    "B": ["a", "b", "c", "d", "e", "x", "z"]})
    assert_equals(dt0, dtr)


def test_rbind_views2():
    # view + view
    dt0 = dt.Frame({"A": [1, 5, 7, 12, 0, 3],
                    "B": ["one", "two", None, "x", "z", "omega"]})
    dt1 = dt0[:3, :]
    dt2 = dt0[-1, :]
    dt1.rbind(dt2)
    dtr = dt.Frame({"A": [1, 5, 7, 3], "B": ["one", "two", None, "omega"]})
    assert_equals(dt1, dtr)


def test_rbind_views3():
    # view + view
    dt0 = dt.Frame({"A": [129, 4, 73, 86],
                    "B": ["eenie", None, "meenie", "teenie"]})
    dt0 = dt0[::2, :]
    dt1 = dt.Frame({"A": [365, -9],
                    "B": ["mo", "miney"]})
    dt1 = dt1[::-1, :]
    dt0.rbind(dt1)
    dtr = dt.Frame({"A": [129, 73, -9, 365],
                    "B": ["eenie", "meenie", "miney", "mo"]})
    assert_equals(dt0, dtr)


def test_rbind_view4():
    DT = dt.Frame(A=list('abcdefghijklmnop'))
    DTempty = dt.Frame(A=[])
    DTempty.nrows = 2
    assert_equals(dt.rbind(DT[:3, :], DT[:-4:-1, :]),
                  dt.Frame(A=list('abcpon')))
    assert_equals(dt.rbind(DT[2:4, :], DTempty, DT[2:5, :]),
                  dt.Frame(A=['c', 'd', None, None, 'c', 'd', 'e']))


def test_rbind_different_stypes1():
    dt0 = dt.Frame([[1, 5, 24, 100]], stype=dt.int8)
    dt1 = dt.Frame([[1000, 2000]], stype=dt.int16)
    dt2 = dt.Frame([[134976130]])
    assert dt0.stypes[0] == stype.int8
    assert dt1.stypes[0] == stype.int16
    assert dt2.stypes[0] == stype.int32
    dt0.rbind(dt1, dt2)
    dtr = dt.Frame([[1, 5, 24, 100, 1000, 2000, 134976130]])
    assert_equals(dt0, dtr)


def test_rbind_different_stypes2():
    dt0 = dt.Frame([[True, False, True]])
    dt1 = dt.Frame([[1, 2, 3]], stype = dt.int8)
    dt2 = dt.Frame([[0.1, 0.5]])
    assert dt0.stypes[0] == stype.bool8
    assert dt1.stypes[0] == stype.int8
    assert dt2.stypes[0] == stype.float64
    dt0.rbind(dt1, dt2)
    dtr = dt.Frame([[1, 0, 1, 1, 2, 3, 0.1, 0.5]])
    assert_equals(dt0, dtr)


def test_rbind_different_stypes3():
    dt0 = dt.Frame([range(10),
                    range(100, 200, 10),
                    range(10, 0, -1),
                    range(-50, 0, 5)],
                   stypes=[stype.int8, stype.int16, stype.int32, stype.int64])
    dt1 = dt.Frame([["alpha"], ["beta"], ["gamma"], ["delta"]])
    frame_integrity_check(dt0)
    frame_integrity_check(dt1)
    assert dt0.stypes == (stype.int8, stype.int16, stype.int32, stype.int64)
    assert dt1.stypes == (stype.str32,) * 4
    dt0.rbind(dt1)
    assert dt0.stypes == (stype.str32,) * 4
    assert dt0.to_list() == [
        ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "alpha"],
        ["100", "110", "120", "130", "140", "150", "160", "170", "180", "190",
         "beta"],
        ["10", "9", "8", "7", "6", "5", "4", "3", "2", "1", "gamma"],
        ["-50", "-45", "-40", "-35", "-30", "-25", "-20", "-15", "-10", "-5",
         "delta"]
    ]


def test_rbind_different_stypes4():
    dt0 = dt.Frame([[True, False, False, None, True],
                    [0.0, -12.34, None, 7e7, 1499],
                    [None, 4.998, 5.14, -34e4, 1.333333]],
                   stypes=[stype.bool8, stype.float32, stype.float64])
    dt1 = dt.Frame([["vega", "altair", None],
                    [None, "wren", "crow"],
                    ["f", None, None]])
    frame_integrity_check(dt0)
    frame_integrity_check(dt1)
    assert dt0.stypes == (stype.bool8, stype.float32, stype.float64)
    assert dt1.stypes == (stype.str32, ) * 3
    dt0.rbind(dt1)
    assert dt0.stypes == (stype.str32, ) * 3
    assert dt0.to_list() == [
        ["True", "False", "False", None, "True", "vega", "altair", None],
        ["0.0", "-12.34", None, "70000000.0", "1499.0", None, "wren", "crow"],
        [None, "4.998", "5.14", "-340000.0", "1.333333", "f", None, None]
    ]


def test_rbind_str32_str64():
    DT1 = dt.Frame(A=list('abcd'), stype=dt.str32)
    DT2 = dt.Frame(A=list('efghij'), stype=dt.str64)
    DT3 = dt.Frame(A=list('klm'), stype=dt.str32)
    DTR = dt.rbind(DT1, DT2, DT3)
    # It would be better if the result was str32
    assert_equals(DTR, dt.Frame(A=list('abcdefghijklm'), stype=dt.str64))


def test_rbind_all_stypes():
    sources = {
        dt.bool8: [True, False, True, None, False, None],
        dt.int8: [3, -5, None, 17, None, 99, -99],
        dt.int16: [None, 245, 872, -333, None],
        dt.int32: [10000, None, 1, 0, None, 34, -2222222],
        dt.int64: [None, 9348571093841, -394, 3053867, 111334],
        dt.float32: [None, 3.3, math.inf, -7.123e20, 34098.79],
        dt.float64: [math.inf, math.nan, 341.0, -34985.94872, 1e310],
        dt.str32: ["first", None, "third", "asblkhblierb", ""],
        dt.str64: ["red", "orange", "blue", "purple", "magenta", None],
        dt.obj64: [1, False, "yey", math.nan, (3, "foo"), None, 2.33],
    }
    all_stypes = list(sources.keys())
    for st1 in all_stypes:
        for st2 in all_stypes:
            f1 = dt.Frame(sources[st1], stype=st1)
            f2 = dt.Frame(sources[st2], stype=st2)
            f3 = dt.rbind(f1, f2)
            f1.rbind(f2)
            frame_integrity_check(f1)
            frame_integrity_check(f2)
            frame_integrity_check(f3)
            assert f1.nrows == len(sources[st1]) + len(sources[st2])
            assert f3.shape == f1.shape
            assert f1.to_list() == f3.to_list()
            del f1
            del f2
            del f3


def test_rbind_modulefn():
    f0 = dt.Frame([1, 5409, 204])
    f1 = dt.Frame([109813, None, 9385])
    f3 = dt.rbind(f0, f1)
    frame_integrity_check(f3)
    assert f3.to_list()[0] == f0.to_list()[0] + f1.to_list()[0]



#-------------------------------------------------------------------------------
# Rbind to a keyed frame
#-------------------------------------------------------------------------------

def test_rbind_empty_frame():
    DT = dt.Frame(A=range(100))
    DT1 = dt.Frame({"A" : []})
    DT.key = "A"
    DT.rbind(DT1)
    assert DT.key == ("A",)
    frame_integrity_check(DT)
    assert DT.to_list() == [list(range(100))]


def test_rbind_filled_frame():
    DT = dt.Frame(A=range(100))
    DT1 = dt.Frame(A=range(10))
    DT.key = "A"
    with pytest.raises(ValueError, match = "Cannot rbind to a keyed frame"):
        DT.rbind(DT1)
    frame_integrity_check(DT)
    assert DT.key == ("A",)
    assert DT.to_list() == [list(range(100))]



#-------------------------------------------------------------------------------
# Issues
#-------------------------------------------------------------------------------

def test_issue1292():
    f0 = dt.Frame([None, None, None, "foo"])
    f0.nrows = 2
    f0.rbind(f0)
    frame_integrity_check(f0)
    assert f0.to_list() == [[None] * 4]
    assert f0.stypes == (stype.str32,)


def test_issue1594():
    DT = dt.Frame([range(5), list("abcde")])
    DT = DT[::-1, :]
    DT.rbind(DT)
    assert DT.to_list() == [[4, 3, 2, 1, 0, 4, 3, 2, 1, 0],
                            list("edcbaedcba")]


def test_issue1607():
    DT = dt.Frame([['eqjmvcgdriqmw', 'dih', 'ejm', 'gzrwhvbqi', 'bydqoss', 'loytilw', 'odswt']])
    DT.nrows = 6
    DT = DT[:5:3, :]
    del DT[[0, 1], :]
    with pytest.warns(DatatableWarning):
        DT.cbind(DT, DT)
    DT.rbind(DT, DT)
    frame_integrity_check(DT)
    assert DT.shape == (0, 3)


def test_issue2026():
    DT = dt.Frame(A=range(10))
    DT.rbind(DT)
    DT.nrows = 8
    DT.rbind(DT)
    frame_integrity_check(DT)
    assert DT.to_list() == [[0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7]]


def test_issue2030():
    DT = dt.Frame(B=[None, 'tzgu', 'dsedpz', 'a', 'weeeeeeeee'])
    DT.nrows = 3
    DT.rbind(DT, DT)
    frame_integrity_check(DT)
    assert DT.to_list()[0] == [None, 'tzgu', 'dsedpz'] * 3
