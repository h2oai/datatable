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
from datatable import stype


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
    dt0 = dt.DataTable([1, 2, 3])
    assert isinstance(dt0.cbind, types.MethodType)
    assert len(dt0.cbind.__doc__) > 2000


def test_cbind_simple():
    d0 = dt.DataTable([1, 2, 3])
    d1 = dt.DataTable([4, 5, 6])
    dt_compute_stats(d0, d1)
    with pytest.warns(UserWarning):
        d0.cbind(d1)
    dr = dt.DataTable([[1, 2, 3], [4, 5, 6]], names=["C1", "C2"])
    assert_equals(d0, dr)


def test_cbind_empty1():
    d0 = dt.DataTable([1, 2, 3])
    dt_compute_stats(d0)
    d0.cbind()
    assert_equals(d0, dt.DataTable([1, 2, 3]))


def test_cbind_empty2():
    d0 = dt.DataTable([1, 2, 3])
    dt_compute_stats(d0)
    d0.cbind(dt.DataTable())
    assert_equals(d0, dt.DataTable([1, 2, 3]))


def test_cbind_self():
    d0 = dt.DataTable({"fun": [1, 2, 3]})
    dt_compute_stats(d0)
    with pytest.warns(UserWarning):
        d0.cbind(d0).cbind(d0).cbind(d0)
    dr = dt.DataTable([[1, 2, 3]] * 8,
                      names=["fun"] + ["fun.%d" % i for i in range(1, 8)])
    assert_equals(d0, dr)


def test_cbind_notinplace():
    d0 = dt.DataTable({"A": [1, 2, 3]})
    d1 = dt.DataTable({"B": [4, 5, 6]})
    dt_compute_stats(d0, d1)
    dd = d0.cbind(d1, inplace=False)
    dr = dt.DataTable({"A": [1, 2, 3], "B": [4, 5, 6]})
    assert_equals(dd, dr)
    assert_equals(d0, dt.DataTable({"A": [1, 2, 3]}))
    assert_equals(d1, dt.DataTable({"B": [4, 5, 6]}))


def test_cbind_notforced():
    d0 = dt.DataTable([1, 2, 3])
    d1 = dt.DataTable([4, 5])
    with pytest.raises(ValueError) as e:
        d0.cbind(d1)
    assert ("Cannot merge datatable with 2 rows to a datatable with 3 rows"
            in str(e.value))


def test_cbind_forced1():
    d0 = dt.DataTable([1, 2, 3])
    d1 = dt.DataTable([4, 5])
    dt_compute_stats(d0, d1)
    with pytest.warns(UserWarning):
        d0.cbind(d1, force=True)
    dr = dt.DataTable([[1, 2, 3], [4, 5, None]], names=["C1", "C2"])
    assert_equals(d0, dr)


def test_cbind_forced2():
    d0 = dt.DataTable({"A": [1, 2, 3], "B": [7, None, -1]})
    d1 = dt.DataTable({"C": list("abcdefghij")})
    dt_compute_stats(d0, d1)
    d0.cbind(d1, force=True)
    dr = dt.DataTable({"A": [1, 2, 3] + [None] * 7,
                       "B": [7, None, -1] + [None] * 7,
                       "C": list("abcdefghij")})
    assert_equals(d0, dr)


def test_cbind_forced3():
    d0 = dt.DataTable({"A": list(range(10))})
    d1 = dt.DataTable({"B": ["one", "two", "three"]})
    dt_compute_stats(d0, d1)
    d0.cbind(d1, force=True)
    dr = dt.DataTable({"A": list(range(10)),
                       "B": ["one", "two", "three"] + [None] * 7})
    assert_equals(d0, dr)


def test_cbind_onerow1():
    d0 = dt.DataTable({"A": [1, 2, 3, 4, 5]})
    d1 = dt.DataTable({"B": [100.0]})
    dt_compute_stats(d0, d1)
    d0.cbind(d1)
    dr = dt.DataTable({"A": [1, 2, 3, 4, 5], "B": [100.0] * 5})
    assert_equals(d0, dr)


def test_cbind_onerow2():
    d0 = dt.DataTable({"A": ["mu"]})
    d1 = dt.DataTable({"B": [7, 9, 10, 15]})
    dt_compute_stats(d0, d1)
    d0.cbind(d1)
    dr = dt.DataTable({"A": ["mu"] * 4, "B": [7, 9, 10, 15]})
    assert_equals(d0, dr)


def test_bad_arguments():
    d0 = dt.DataTable([1, 2, 3])
    d1 = dt.DataTable([5])
    with pytest.raises(TypeError):
        d0.cbind(100)
    with pytest.raises(TypeError):
        d0.cbind(d1, inplace=3)
    with pytest.raises(TypeError):
        d0.cbind(d1, force=None)


def test_cbind_views1():
    d0 = dt.DataTable({"A": list(range(100))})
    d1 = d0[:5, :]
    assert d1.internal.isview
    d2 = dt.DataTable({"B": [3, 6, 9, 12, 15]})
    d1.cbind(d2)
    assert not d1.internal.isview
    dr = dt.DataTable({"A": list(range(5)), "B": list(range(3, 18, 3))})
    assert_equals(d1, dr)


def test_cbind_views2():
    d0 = dt.DataTable({"A": list(range(10))})
    d1 = d0[2:5, :]
    assert d1.internal.isview
    d2 = dt.DataTable({"B": list("abcdefghij")})
    d3 = d2[-3:, :]
    assert d3.internal.isview
    d1.cbind(d3)
    assert not d1.internal.isview
    dr = dt.DataTable({"A": [2, 3, 4], "B": ["h", "i", "j"]})
    assert_equals(d1, dr)


def test_cbind_multiple():
    d0 = dt.DataTable({"A": [1, 2, 3]})
    d1 = dt.DataTable({"B": ["doo"]})
    d2 = dt.DataTable({"C": [True, False]})
    d3 = dt.DataTable({"D": [10, 9, 8, 7]})
    d4 = dt.DataTable({"E": [1]})[:0, :]
    dt_compute_stats(d0, d1, d2, d3, d4)
    d0.cbind(d1, d2, d3, d4, force=True)
    dr = dt.DataTable({"A": [1, 2, 3, None],
                       "B": ["doo", "doo", "doo", "doo"],
                       "C": [True, False, None, None],
                       "D": [10, 9, 8, 7],
                       "E": [None, None, None, None]})
    assert_equals(d0, dr)


def test_cbind_1row_none():
    # Special case: append a single-row string column containing value NA
    d0 = dt.DataTable({"A": [1, 2, 3]})
    d1 = dt.DataTable({"B": [None, "doo"]})[0, :]
    dt_compute_stats(d0, d1)
    assert d1.shape == (1, 1)
    assert d1.stypes == (stype.str32, )
    d0.cbind(d1)
    dr = dt.DataTable({"A": [1, 2, 3, 4], "B": [None, None, None, "f"]})[:-1, :]
    assert_equals(d0, dr)
