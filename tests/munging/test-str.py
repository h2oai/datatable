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
import pytest
import random
import re
from datatable import stype, f
from datatable.internal import frame_integrity_check
from tests import noop, random_string, assert_equals


def test_issue_1912():
    # Check that the expression `A==None` works if A is a string column
    DT = dt.Frame(A=["dfv", None, None, "adfknlkad", None])
    RES = DT[:, f.A == None]
    assert RES.to_list()[0] == [0, 1, 1, 0, 1]



#-------------------------------------------------------------------------------
# split_into_nhot
#-------------------------------------------------------------------------------

def test_split_into_nhot_noarg():
    with pytest.raises(ValueError) as e:
        noop(dt.str.split_into_nhot())
    assert ("Required parameter frame is missing" == str(e.value))


def test_split_into_nhot_none():
    f0 = dt.str.split_into_nhot(None)
    assert f0 is None


def test_split_into_nhot_empty():
    f0 = dt.str.split_into_nhot(dt.Frame(["", None]))
    assert_equals(f0, dt.Frame())


@pytest.mark.parametrize('sort', [False, True])
def test_split_into_nhot0(sort):
    f0 = dt.Frame(["cat, dog, mouse, peacock, frog",
                   "armadillo, fox, hedgehog",
                   None,
                   "dog, fox, mouse, cat, peacock",
                   "horse, raccoon, cat, frog, dog"])
    f1 = dt.str.split_into_nhot(f0, sort = sort)
    frame_integrity_check(f1)
    fr = dt.Frame({"cat":       [1, 0, None, 1, 1],
                   "dog":       [1, 0, None, 1, 1],
                   "mouse":     [1, 0, None, 1, 0],
                   "peacock":   [1, 0, None, 1, 0],
                   "frog":      [1, 0, None, 0, 1],
                   "armadillo": [0, 1, None, 0, 0],
                   "fox":       [0, 1, None, 1, 0],
                   "hedgehog":  [0, 1, None, 0, 0],
                   "horse":     [0, 0, None, 0, 1],
                   "raccoon":   [0, 0, None, 0, 1]})
    assert set(f1.names) == set(fr.names)
    if sort :
        assert list(f1.names) == sorted(fr.names)
    fr = fr[:, f1.names]
    assert f1.names == fr.names
    assert f1.stypes == (dt.stype.bool8, ) * f1.ncols
    assert f1.shape == fr.shape
    assert f1.to_list() == fr.to_list()


def test_split_into_nhot1():
    f0 = dt.Frame(["  meow  \n",
                   None,
                   "[ meow]",
                   "['meow' ,purr]",
                   '(\t"meow", \'purr\')',
                   "{purr}"])
    f1 = dt.str.split_into_nhot(f0)
    frame_integrity_check(f1)
    fr = dt.Frame(meow=[1, None, 1, 1, 1, 0], purr=[0, None, 0, 1, 1, 1])
    assert set(f1.names) == set(fr.names)
    fr = fr[..., f1.names]
    assert f1.shape == fr.shape == (6, 2)
    assert f1.stypes == (dt.stype.bool8, dt.stype.bool8)
    assert f1.to_list() == fr.to_list()


def test_split_into_nhot_sep():
    f0 = dt.Frame(["a|b|c", "b|a", None, "a|c"])
    f1 = dt.str.split_into_nhot(f0, sep="|")
    assert set(f1.names) == {"a", "b", "c"}
    fr = dt.Frame(a=[1, 1, None, 1], b=[1, 1, None, 0], c=[1, 0, None, 1])
    assert set(f1.names) == set(fr.names)
    assert f1.to_list() == fr[:, f1.names].to_list()


def test_split_into_nhot_quotes():
    f0 = dt.str.split_into_nhot(dt.Frame(['foo, "bar, baz"']))
    f1 = dt.str.split_into_nhot(dt.Frame(['foo, "bar, baz']))
    assert set(f0.names) == {"foo", "bar, baz"}
    assert set(f1.names) == {"foo", '"bar', "baz"}


def test_split_into_nhot_bad():
    f0 = dt.Frame([[1.25], ["foo"], ["bar"]])
    with pytest.raises(ValueError) as e:
        dt.str.split_into_nhot(f0)
    assert ("Function split_into_nhot() may only be applied to a single-column "
            "Frame of type string; got frame with 3 columns" == str(e.value))

    with pytest.raises(TypeError) as e:
        dt.str.split_into_nhot(f0[:, 0])
    assert ("Function split_into_nhot() may only be applied to a single-column "
            "Frame of type string; received a column of type float64" ==
            str(e.value))

    with pytest.raises(ValueError) as e:
        dt.str.split_into_nhot(f0[:, 1], sep=",;-")
    assert ("Parameter sep in split_into_nhot() must be a single character"
            in str(e.value))


@pytest.mark.parametrize("seed, st", [(random.getrandbits(32), stype.str32),
                                      (random.getrandbits(32), stype.str64)])
def test_split_into_nhot_long(seed, st):
    random.seed(seed)
    n = int(random.expovariate(0.0001) + 100)
    col1 = [random.getrandbits(1) for _ in range(n)]
    col2 = [random.getrandbits(1) for _ in range(n)]
    col3 = [random.getrandbits(1) for _ in range(n)]
    col4 = [0] * n

    data = [",".join(["liberty"] * col1[i] +
                     ["equality"] * col2[i] +
                     ["justice"] * col3[i]) for i in range(n)]

    # Introduce 1% of None's, making sure we preserve data at freedom_index
    na_indices = random.sample(range(n), n // 100)
    freedom_index = random.randint(0, n - 1)
    for i in na_indices:
        if i == freedom_index: continue
        col1[i] = None
        col2[i] = None
        col3[i] = None
        col4[i] = None
        data[i] = None
    col4[freedom_index] = 1
    data[freedom_index] += ", freedom"

    f0 = dt.Frame(data, stype=st)
    assert f0.stypes == (st,)
    assert f0.shape == (n, 1)
    f1 = dt.str.split_into_nhot(f0)
    assert f1.shape == (n, 4)
    fr = dt.Frame(liberty=col1, equality=col2, justice=col3, freedom=col4)
    assert set(f1.names) == set(fr.names)
    f1 = f1[..., fr.names]
    assert f1.to_list() == fr.to_list()


def test_split_into_nhot_view():
    f0 = dt.Frame(A=["cat,dog,mouse", "mouse", None, "dog, cat"])
    f1 = dt.str.split_into_nhot(f0[::-1, :])
    f2 = dt.str.split_into_nhot(f0[3, :])
    assert set(f1.names) == {"cat", "dog", "mouse"}
    assert f1[:, ["cat", "dog", "mouse"]].to_list() == \
           [[1, None, 0, 1], [1, None, 0, 1], [0, None, 1, 1]]
    assert set(f2.names) == {"cat", "dog"}
    assert f2[:, ["cat", "dog"]].to_list() == [[1], [1]]


def test_split_into_nhot_deprecated():
    DT = dt.Frame(["a, b, c"])
    with pytest.warns(FutureWarning):
        RES = dt.split_into_nhot(DT)
    EXP = dt.Frame([[1], [1], [1]], names=["a", "b", "c"], stype=dt.bool8)
    assert_equals(RES, EXP)
