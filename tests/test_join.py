#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable as dt
import pytest
import random
from datatable import join



def test_join_simple():
    d0 = dt.Frame([[1, 3, 2, 1, 1, 2, 0], list("abcdefg")], names=("A", "B"))
    d1 = dt.Frame([range(4), ["zero", "one", "two", "three"]], names=("A", "V"))
    d1.key = "A"
    res = d0[:, :, join(d1)]
    res.internal.check()
    assert res.shape == (7, 3)
    assert res.names == ("A", "B", "V")
    assert res.topython() == [
        [1, 3, 2, 1, 1, 2, 0],
        ["a", "b", "c", "d", "e", "f", "g"],
        ["one", "three", "two", "one", "one", "two", "zero"]]


def test_join_strings():
    d0 = dt.Frame([[1, 3, 2, 1, 1, 2, 0], list("cabdabb")], names=("A", "B"))
    d1 = dt.Frame([list("abcd"), range(0, 20, 5)], names=("B", "V"))
    d1.key = "B"
    res = d0[:, :, join(d1)]
    res.internal.check()
    assert res.shape == (7, 3)
    assert res.names == ("A", "B", "V")
    assert res.topython() == [
        [1, 3, 2, 1, 1, 2, 0],
        ["c", "a", "b", "d", "a", "b", "b"],
        [10, 0, 5, 15, 0, 5, 5]]


def test_join_missing_levels():
    d0 = dt.Frame(A=[1, 2, 3])
    d1 = dt.Frame(A=[1, 2], K=[True, False])
    d1.key = "A"
    res = d0[:, :, join(d1)]
    res.internal.check()
    assert res.topython() == [[1, 2, 3], [True, False, None]]


def test_join_errors():
    d0 = dt.Frame(A=[1, 2, 3])
    d1 = dt.Frame(B=range(10), stype=dt.float64)
    with pytest.raises(ValueError) as e:
        d0[:, :, join(d1)]
    assert "The join frame is not keyed" in str(e.value)
    d1.key = "B"
    with pytest.raises(ValueError) as e:
        d0[:, :, join(d1)]
    assert "Key column `B` does not exist in the left Frame" in str(e.value)
    d1.names = ("A",)
    with pytest.raises(TypeError) as e:
        d0[:, :, join(d1)]
    assert ("Join column `A` has type int in the left Frame, and type real "
            "in the right Frame" in str(e.value))
