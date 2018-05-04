#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import tempfile
import shutil
import pytest
import types
import datatable as dt
from tests import assert_equals
from datatable import stype


#-------------------------------------------------------------------------------
# Run the tests
#-------------------------------------------------------------------------------

def test_rbind_exists():
    dt0 = dt.Frame([1, 2, 3])
    assert isinstance(dt0.rbind, types.MethodType)
    assert len(dt0.rbind.__doc__) > 1900
    assert dt0.append == dt0.rbind


def test_rbind_simple():
    dt0 = dt.Frame([1, 2, 3])
    dt1 = dt.Frame([5, 9])
    dt0.rbind(dt1)
    assert_equals(dt0, dt.Frame([1, 2, 3, 5, 9]))


def test_rbind_empty():
    dt0 = dt.Frame([1, 2, 3])
    dt0.rbind()
    dt0.rbind(dt.Frame())
    assert_equals(dt0, dt.Frame([1, 2, 3]))


def test_rbind_fails():
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
    assert "Column `C` is not found in the source frame" in str(e.value)
    assert "`force=True`" in str(e.value)


def test_rbind_bynumbers():
    dt0 = dt.Frame([[1, 2, 3], [7, 7, 0]], names=["A", "V"])
    dt1 = dt.Frame([[10, -1], [3, -1]], names=["C", "Z"])
    dtr = dt.Frame([[1, 2, 3, 10, -1], [7, 7, 0, 3, -1]],
                   names=["A", "V"])
    dt0.rbind(dt1, bynames=False)
    assert_equals(dt0, dtr)

    dt0 = dt.Frame({"X": [8]})
    dtr = dt.Frame({"X": [8, 10, -1], "Z": [None, 3, -1]})
    dt0.rbind(dt1, bynames=False, force=True)
    assert_equals(dt0, dtr)


def test_rbind_bynames():
    dt0 = dt.Frame({"A": [1, 2, 3], "V": [7, 7, 0]})
    dt1 = dt.Frame({"V": [13], "A": [77]})
    dtr = dt.Frame({"A": [1, 2, 3, 77], "V": [7, 7, 0, 13]})
    dt0.rbind(dt1)
    assert_equals(dt0, dtr)

    dt0 = dt.Frame({"A": [5, 1]})
    dt1 = dt.Frame({"C": [4], "D": [10]})
    dt2 = dt.Frame({"A": [-1], "D": [4]})
    dtr = dt.Frame({"A": [5, 1, None, -1],
                    "C": [None, None, 4, None],
                    "D": [None, None, 10, 4]})
    dt0.rbind(dt1, dt2, force=True)
    assert_equals(dt0, dtr)

    dt0 = dt.Frame({"A": [7, 4], "B": [-1, 1]})
    dt1 = dt.Frame({"A": [3]})
    dt2 = dt.Frame({"B": [4]})
    dtr = dt.Frame({"A": [7, 4, 3, None], "B": [-1, 1, None, 4]})
    dt0.rbind(dt1, force=True)
    dt0.rbind(dt2, force=True)
    assert_equals(dt0, dtr)

    dt0 = dt.Frame({"A": [13]})
    dt1 = dt.Frame({"B": [6], "A": [3], "E": [7]})
    dtr = dt.Frame({"A": [13, 3], "B": [None, 6], "E": [None, 7]})
    dt0.rbind(dt1, force=True)
    assert_equals(dt0, dtr)


def test_not_inplace():
    dt0 = dt.Frame({"A": [5, 1], "B": [4, 4]})
    dt1 = dt.Frame({"A": [22], "B": [11]})
    dtr = dt.Frame().rbind(dt0, dt1)
    assert_equals(dtr, dt.Frame({"A": [5, 1, 22], "B": [4, 4, 11]}))
    assert_equals(dt0, dt.Frame({"A": [5, 1], "B": [4, 4]}))


def test_repeating_names():
    # Warnings about repeated names -- ignore
    with pytest.warns(UserWarning):
        dt0 = dt.Frame([[5], [6], [7], [4]], names=["x", "y", "x", "x"])
        dt1 = dt.Frame([[4], [3], [2]], names=["y", "x", "x"])
        dtr = dt.Frame([[5, 3], [6, 4], [7, 2], [4, None]],
                       names=["x", "y", "x.1", "x.2"])
        dt0.rbind(dt1, force=True)
        assert_equals(dt0, dtr)

        dt0 = dt.Frame({"a": [23]})
        dt1 = dt.Frame([[2], [4], [8]], names="aaa")
        dtr = dt.Frame([[23, 2], [None, 4], [None, 8]], names="aaa")
        dt0.rbind(dt1, force=True)
        assert_equals(dt0, dtr)

        dt0 = dt.Frame([[22], [44], [88]], names="aba")
        dt1 = dt.Frame([[2], [4], [8]], names="aaa")
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
    # TODO: fix this test once rbinding strings with non-strings properly
    #       implemented.
    f0 = dt.Frame([1, 2, 3])
    f1 = dt.Frame(["foo", "bra"])
    with pytest.raises(NotImplementedError):
        f1.rbind(f0)



def test_rbind_self():
    dt0 = dt.Frame({"A": [1, 5, 7], "B": ["one", "two", None]})
    dt0.rbind(dt0, dt0, dt0)
    dtr = dt.Frame({"A": [1, 5, 7] * 4, "B": ["one", "two", None] * 4})
    assert_equals(dt0, dtr)


def test_rbind_mmapped():
    dir0 = tempfile.mkdtemp()
    dt0 = dt.Frame({"A": [1, 5, 7], "B": ["one", "two", None]})
    dt.save(dt0, dir0)
    del dt0
    dt1 = dt.open(dir0)
    dt2 = dt.Frame({"A": [-1], "B": ["zero"]})
    dt1.rbind(dt2)
    dtr = dt.Frame({"A": [1, 5, 7, -1], "B": ["one", "two", None, "zero"]})
    assert_equals(dt1, dtr)
    shutil.rmtree(dir0)


def test_rbind_views0():
    # view + data
    dt0 = dt.Frame({"d": range(10), "s": list("abcdefghij")})
    dt0 = dt0[3:7, :]
    dt1 = dt.Frame({"d": [-1, -2], "s": ["the", "end"]})
    dt0.rbind(dt1)
    dtr = dt.Frame({"d": [3, 4, 5, 6, -1, -2],
                    "s": ["d", "e", "f", "g", "the", "end"]})
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


def test_rbind_different_stypes1():
    dt0 = dt.Frame([[1, 5, 24, 100]])
    dt1 = dt.Frame([[1000, 2000]])
    dt2 = dt.Frame([[134976130]])
    assert dt0.stypes[0] == stype.int8
    assert dt1.stypes[0] == stype.int16
    assert dt2.stypes[0] == stype.int32
    dt0.rbind(dt1, dt2)
    dtr = dt.Frame([[1, 5, 24, 100, 1000, 2000, 134976130]])
    assert_equals(dt0, dtr)


def test_rbind_different_stypes2():
    dt0 = dt.Frame([[True, False, True]])
    dt1 = dt.Frame([[1, 2, 3]])
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
    assert dt0.internal.check()
    assert dt1.internal.check()
    assert dt0.stypes == (stype.int8, stype.int16, stype.int32, stype.int64)
    assert dt1.stypes == (stype.str32,) * 4
    dt0.rbind(dt1)
    assert dt0.stypes == (stype.str32,) * 4
    assert dt0.topython() == [
        ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "alpha"],
        ["100", "110", "120", "130", "140", "150", "160", "170", "180", "190",
         "beta"],
        ["10", "9", "8", "7", "6", "5", "4", "3", "2", "1", "gamma"],
        ["-50", "-45", "-40", "-35", "-30", "-25", "-20", "-15", "-10", "-5",
         "delta"]
    ]


# TODO: add tests for appending categorical columns (requires merging levelsets)
