#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import pytest
import types
import datatable as dt
from tests import assert_equals
from datatable import stype, DatatableWarning


def dt_compute_stats(*dts):
    """
    Force computing all Stats on the datatable.

    Currently, computing a single stat causes all other stats to be computed
    as well.
    """
    for d in dts:
        d.min()


#-------------------------------------------------------------------------------
# Run the tests
#-------------------------------------------------------------------------------

def test_cbind_exists():
    dt0 = dt.Frame([1, 2, 3])
    assert isinstance(dt0.cbind, types.BuiltinMethodType)
    assert len(dt0.cbind.__doc__) > 1400


def test_cbind_simple():
    d0 = dt.Frame([1, 2, 3])
    d1 = dt.Frame([4, 5, 6])
    dt_compute_stats(d0, d1)
    with pytest.warns(DatatableWarning):
        d0.cbind(d1)
    dr = dt.Frame([[1, 2, 3], [4, 5, 6]], names=["C0", "C1"])
    assert_equals(d0, dr)


def test_cbind_empty1():
    d0 = dt.Frame([1, 2, 3])
    dt_compute_stats(d0)
    d0.cbind()
    assert_equals(d0, dt.Frame([1, 2, 3]))


def test_cbind_empty2():
    d0 = dt.Frame([1, 2, 3])
    dt_compute_stats(d0)
    d0.cbind(dt.Frame())
    assert_equals(d0, dt.Frame([1, 2, 3]))


def test_cbind_self():
    d0 = dt.Frame({"fun": [1, 2, 3]})
    dt_compute_stats(d0)
    with pytest.warns(DatatableWarning):
        d0.cbind(d0)
        d0.cbind(d0)
        d0.cbind(d0)
    dr = dt.Frame([[1, 2, 3]] * 8,
                  names=["fun"] + ["fun.%d" % i for i in range(1, 8)])
    assert_equals(d0, dr)


def test_cbind_notinplace():
    d0 = dt.Frame({"A": [1, 2, 3]})
    d1 = dt.Frame({"B": [4, 5, 6]})
    dt_compute_stats(d0, d1)
    dd = dt.cbind(d0, d1)
    dr = dt.Frame({"A": [1, 2, 3], "B": [4, 5, 6]})
    assert_equals(dd, dr)
    assert_equals(d0, dt.Frame({"A": [1, 2, 3]}))
    assert_equals(d1, dt.Frame({"B": [4, 5, 6]}))


def test_cbind_notforced():
    d0 = dt.Frame([1, 2, 3])
    d1 = dt.Frame([4, 5])
    with pytest.raises(ValueError) as e:
        d0.cbind(d1)
    assert ("Cannot merge frame with 2 rows to a frame with 3 rows"
            in str(e.value))


def test_cbind_forced1():
    d0 = dt.Frame([1, 2, 3])
    d1 = dt.Frame([4, 5])
    dt_compute_stats(d0, d1)
    with pytest.warns(DatatableWarning):
        d0.cbind(d1, force=True)
    dr = dt.Frame([[1, 2, 3], [4, 5, None]], names=["C0", "C1"])
    assert_equals(d0, dr)


def test_cbind_forced2():
    d0 = dt.Frame({"A": [1, 2, 3], "B": [7, None, -1]})
    d1 = dt.Frame({"C": list("abcdefghij")})
    dt_compute_stats(d0, d1)
    d0.cbind(d1, force=True)
    dr = dt.Frame({"A": [1, 2, 3] + [None] * 7,
                   "B": [7, None, -1] + [None] * 7,
                   "C": list("abcdefghij")})
    assert_equals(d0, dr)


def test_cbind_forced3():
    d0 = dt.Frame({"A": range(10)})
    d1 = dt.Frame({"B": ["one", "two", "three"]})
    dt_compute_stats(d0, d1)
    d0.cbind(d1, force=True)
    dr = dt.Frame({"A": range(10),
                   "B": ["one", "two", "three"] + [None] * 7})
    assert_equals(d0, dr)


def test_cbind_onerow1():
    d0 = dt.Frame({"A": [1, 2, 3, 4, 5]})
    d1 = dt.Frame({"B": [100.0]})
    dt_compute_stats(d0, d1)
    d0.cbind(d1)
    dr = dt.Frame({"A": [1, 2, 3, 4, 5], "B": [100.0] * 5})
    assert_equals(d0, dr)


def test_cbind_onerow2():
    d0 = dt.Frame({"A": ["mu"]})
    d1 = dt.Frame({"B": [7, 9, 10, 15]})
    dt_compute_stats(d0, d1)
    d0.cbind(d1)
    dr = dt.Frame({"A": ["mu"] * 4, "B": [7, 9, 10, 15]})
    assert_equals(d0, dr)


def test_bad_arguments():
    d0 = dt.Frame([1, 2, 3])
    d1 = dt.Frame([5])
    with pytest.raises(TypeError):
        d0.cbind(100)
    with pytest.raises(TypeError):
        d0.cbind(d1, inplace=3)
    with pytest.raises(TypeError):
        d0.cbind(d1, force=None)


def test_cbind_views1():
    d0 = dt.Frame({"A": range(100)})
    d1 = d0[:5, :]
    assert d1.internal.isview
    d2 = dt.Frame({"B": [3, 6, 9, 12, 15]}, stype=stype.int32)
    d1.cbind(d2)
    assert not d1.internal.isview
    dr = dt.Frame({"A": range(5), "B": range(3, 18, 3)})
    assert_equals(d1, dr)


def test_cbind_views2():
    d0 = dt.Frame({"A": range(10)}, stype=stype.int8)
    d1 = d0[2:5, :]
    assert d1.internal.isview
    d2 = dt.Frame({"B": list("abcdefghij")})
    d3 = d2[-3:, :]
    assert d3.internal.isview
    d1.cbind(d3)
    assert not d1.internal.isview
    dr = dt.Frame({"A": [2, 3, 4], "B": ["h", "i", "j"]})
    assert_equals(d1, dr)


def test_cbind_multiple():
    d0 = dt.Frame({"A": [1, 2, 3]})
    d1 = dt.Frame({"B": ["doo"]})
    d2 = dt.Frame({"C": [True, False]})
    d3 = dt.Frame({"D": [10, 9, 8, 7]})
    d4 = dt.Frame({"E": [1]})[:0, :]
    dt_compute_stats(d0, d1, d2, d3, d4)
    d0.cbind(d1, d2, d3, d4, force=True)
    dr = dt.Frame({"A": [1, 2, 3, None],
                   "B": ["doo", "doo", "doo", "doo"],
                   "C": [True, False, None, None],
                   "D": [10, 9, 8, 7]})
    assert_equals(d0, dr)


def test_cbind_1row_none():
    # Special case: append a single-row string column containing value NA
    d0 = dt.Frame({"A": [1, 2, 3]})
    d1 = dt.Frame({"B": [None, "doo"]})[0, :]
    dt_compute_stats(d0, d1)
    assert d1.shape == (1, 1)
    assert d1.stypes == (stype.str32, )
    d0.cbind(d1)
    dr = dt.Frame({"A": [1, 2, 3, 4], "B": [None, None, None, "f"]})[:-1, :]
    assert_equals(d0, dr)


def test_cbind_method():
    d0 = dt.Frame({"A": [1, 2, 3]})
    d1 = dt.Frame({"B": list('abc')})
    d2 = dt.Frame({"C": [5.6, 7.1, -3.3]})
    dr = dt.cbind(d0, d1, d2)
    assert dr.names == ("A", "B", "C")
    res = dt.Frame([[1, 2, 3], ["a", "b", "c"], [5.6, 7.1, -3.3]],
                   names=("A", "B", "C"))
    assert_equals(dr, res)


def test_cbind_array():
    d0 = dt.Frame(A=range(5))
    with pytest.warns(DatatableWarning):
        d0.cbind([d0] * 10)
    d0.internal.check()
    assert d0.shape == (5, 11)
    assert d0.names == ("A",) + tuple("A.%d" % (i + 1) for i in range(10))
    assert d0.to_list() == [[0, 1, 2, 3, 4]] * 11


def test_cbind_bad_things():
    d0 = dt.Frame(A=range(5))
    with pytest.raises(TypeError) as e:
        d0.cbind(12)
    assert ("Frame.cbind() expects a list or sequence of Frames, but got an "
            "argument of type <class 'int'>" == str(e.value))
    with pytest.raises(TypeError) as e:
        d0.cbind([d0, d0, 0])
    assert ("Frame.cbind() expects a list or sequence of Frames, but got an "
            "argument of type <class 'int'>" == str(e.value))


def test_cbind_correct_stypes():
    # Issue 1287: check that if stypes / ltypes were queried before doing cbind,
    # then they are updated correctly after the cbind.
    f0 = dt.Frame(range(5))
    s1 = f0.stypes   # cause stypes to be computed and cached
    l1 = f0.ltypes
    with pytest.warns(DatatableWarning):
        f0.cbind(f0, f0)
    s2 = f0.stypes
    l2 = f0.ltypes
    assert s2 == s1 * 3
    assert l2 == l1 * 3
