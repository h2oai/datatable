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
import datatable as dt
import pytest
import random
from datatable.internal import frame_integrity_check
from tests import assert_equals



#-------------------------------------------------------------------------------
# General functionality
#-------------------------------------------------------------------------------
set_fns = [dt.union, dt.intersect, dt.setdiff, dt.symdiff]


@pytest.mark.parametrize("fn", set_fns)
def test_setfns_0(fn):
    res = fn()
    frame_integrity_check(res)
    assert res.shape == (0, 0)


@pytest.mark.parametrize("fn", set_fns)
def test_setfns_1(fn):
    dt0 = dt.Frame([1, 2, 3, 1])
    res = fn(dt0)
    frame_integrity_check(res)
    assert res.shape == (3, 1)
    assert res.to_list() == [[1, 2, 3]]


@pytest.mark.parametrize("fn", set_fns)
def test_setfns_array_arg(fn):
    dt0 = dt.Frame([1, 2, 3, 4, 5])
    dt1 = dt.Frame([3, 5, 7, 9])
    dt2 = dt.Frame([2, 7, 11])
    res1 = fn(dt0, dt1, dt2)
    res2 = fn([dt0, dt1, dt2])
    frame_integrity_check(res1)
    frame_integrity_check(res2)
    assert res1.names == res2.names
    assert res1.stypes == res2.stypes
    assert res1.to_list() == res2.to_list()


@pytest.mark.parametrize("fn", set_fns)
def test_setfns_colname(fn):
    # The name from the first of the frames shall be retained
    dt0 = dt.Frame(A=[2, 3, 5])
    dt1 = dt.Frame(B=range(4))
    res1 = fn(dt0, dt1)
    res2 = fn(dt1, dt0)
    assert res1.stypes == res2.stypes
    assert res1.names == ("A",)
    assert res2.names == ("B",)


@pytest.mark.parametrize("fn", set_fns)
def test_setfns_ignore_empty_frames(fn):
    dt1 = dt.Frame([2, 5, 7, 2, 3])
    dt2 = dt.Frame([3, 4, 2, 5])
    res1 = fn(dt1, dt2)
    res2 = fn(dt1, dt.Frame(), dt2)
    assert res1.to_list() == res2.to_list()


@pytest.mark.parametrize("fn", set_fns)
def test_setfns_between_empty_frames1(fn):
    DT = dt.Frame()
    res = fn(DT, DT)
    assert_equals(res, DT)


@pytest.mark.parametrize("fn", set_fns)
def test_setfns_between_empty_frames2(fn):
    DT = dt.Frame(A=[])
    res = fn(DT, DT)
    assert_equals(res, DT)



#-------------------------------------------------------------------------------
# union()
#-------------------------------------------------------------------------------

def test_union2():
    dt1 = dt.Frame([2, 5, 7, 2, 3])
    dt2 = dt.Frame([3, 4, 2, 5])
    res = dt.union(dt1, dt2)
    assert res.to_list() == [[2, 3, 4, 5, 7]]
    frame_integrity_check(res)


def test_union3():
    dt1 = dt.Frame([2, 5, 7, 2, 3])
    dt2 = dt.Frame([3, 4, 2, 5])
    dt3 = dt.Frame([0, 3, 2, 2, 2, 2, 2, 2, 0])
    res = dt.union(dt3, dt1, dt2)
    assert res.to_list() == [[0, 2, 3, 4, 5, 7]]
    frame_integrity_check(res)



#-------------------------------------------------------------------------------
# intersect()
#-------------------------------------------------------------------------------

def test_intersect2():
    dt1 = dt.Frame([2, 5, 7, 2, 3])
    dt2 = dt.Frame([3, 4, 2, 5])
    res = dt.intersect(dt1, dt2)
    assert res.to_list() == [[2, 3, 5]]
    frame_integrity_check(res)


def test_intersect3():
    dt1 = dt.Frame([2, 5, 7, 2, 3])
    dt2 = dt.Frame([3, 4, 2, 5])
    dt3 = dt.Frame([0, 3, 2, 2, 2, 2, 2, 2, 0])
    res = dt.intersect(dt3, dt1, dt2)
    assert res.to_list() == [[2, 3]]
    frame_integrity_check(res)



#-------------------------------------------------------------------------------
# setdiff()
#-------------------------------------------------------------------------------

def test_setdiff2():
    dt1 = dt.Frame([2, 5, 7, 2, 3])
    dt2 = dt.Frame([3, 4, 2, 5])
    res = dt.setdiff(dt1, dt2)
    assert res.to_list() == [[7]]
    frame_integrity_check(res)

def test_setdiff3():
    dt1 = dt.Frame([2, 5, 7, 2, 3, 6, 0])
    dt2 = dt.Frame([3, 4, 2, 5])
    dt3 = dt.Frame([0, 3, 2, 2, 2, 2, 2, 2, 0])
    res = dt.setdiff(dt1, dt2, dt3)
    assert res.to_list() == [[6, 7]]
    frame_integrity_check(res)



#-------------------------------------------------------------------------------
# symdiff()
#-------------------------------------------------------------------------------

def test_symdiff2():
    dt1 = dt.Frame([2, 5, 7, 2, 3])
    dt2 = dt.Frame([3, 4, 2, 5])
    res = dt.symdiff(dt1, dt2)
    assert res.to_list() == [[4, 7]]
    frame_integrity_check(res)

def test_symdiff3():
    dt1 = dt.Frame([2, 5, 7, 2, 3, 6, 0])
    dt2 = dt.Frame([3, 4, 2, 5])
    dt3 = dt.Frame([0, 3, 2, 2, 2, 2, 2, 2, 0])
    res = dt.symdiff(dt1, dt2, dt3)
    assert res.to_list() == [[2, 3, 4, 6, 7]]
    frame_integrity_check(res)



#-------------------------------------------------------------------------------
# Random test
#-------------------------------------------------------------------------------

def union(srcs):
    res = set()
    for src in srcs:
        res.update(src)
    return sorted(res)

def intersect(srcs):
    res = set(srcs[0])
    for src in srcs[1:]:
        res.intersection_update(src)
    return sorted(res)

def setdiff(srcs):
    res = set(srcs[0])
    for src in srcs[1:]:
        res.difference_update(src)
    return sorted(res)

def symdiff(srcs):
    res = set(srcs[0])
    for src in srcs[1:]:
        res.symmetric_difference_update(src)
    return sorted(res)


@pytest.mark.parametrize("seed", [random.getrandbits(32) for _ in range(20)])
def test_setfns_random(seed):
    random.seed(seed)
    nsets = 2 + int(random.expovariate(0.2))
    span = random.randint(5, nsets * 100)
    srcs = []
    for _ in range(nsets):
        nrows = int(random.expovariate(0.02))
        src = [random.randint(1, span) for _ in range(nrows)]
        srcs.append(src)
    dtfun, pyfun = random.choice([(dt.union, union),
                                  (dt.intersect, intersect),
                                  (dt.setdiff, setdiff),
                                  (dt.symdiff, symdiff)])
    dts = [dt.Frame([src]) for src in srcs]
    dtres = dtfun(dts)
    pyres = pyfun(srcs)
    assert dtres.to_list()[0] == pyres
