#!/usr/bin/env python
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
import math
import os
import pytest
import random
import datatable as dt
from datatable import stype, ltype, sort, by, f
from datatable.internal import frame_integrity_check
from math import inf, nan
from tests import list_equals, random_string, assert_equals, isview



#-------------------------------------------------------------------------------
# Simple sort
#-------------------------------------------------------------------------------

def test_sort_len0():
    d0 = dt.Frame([[]])
    assert d0.shape == (0, 1)
    d1 = d0.sort(0)
    frame_integrity_check(d1)
    assert d1.shape == (0, 1)


def test_sort_len1():
    d0 = dt.Frame([10**6])
    assert d0.shape == (1, 1)
    d1 = d0.sort(0)
    frame_integrity_check(d1)
    assert d1.to_list() == [[10**6]]


def test_sort_len1_view():
    d0 = dt.Frame([range(10), range(10, 0, -1)])
    d1 = d0[6, :].sort(0)
    assert d1.to_list() == [[6], [4]]
    d2 = d0[[7], :].sort(0)
    assert d2.to_list() == [[7], [3]]
    d3 = d0[2:3, :].sort(0)
    assert d3.to_list() == [[2], [8]]
    d4 = d0[4::2, :].sort(1, 0)
    assert d4.shape == (3, 2)
    assert d4.to_list() == [[8, 6, 4], [2, 4, 6]]


def test_sort_len2():
    d0 = dt.Frame([None, 10000000])
    d1 = d0.sort(0)
    frame_integrity_check(d1)
    assert d1.to_list() == [[None, 10000000]]
    d0 = dt.Frame([10000000, None])
    d2 = d0.sort(0)
    assert d1.to_list() == d2.to_list()


def test_sort_simple():
    d0 = dt.Frame([random.randint(0, 20) for _ in range(100)])
    d1 = d0[:, :, sort(0)]
    frame_integrity_check(d1)
    assert d1.shape == d0.shape
    assert d1.to_list() == [sorted(d0.to_list()[0])]


def test_nonfirst_column():
    """Check that sorting by n-th column works too..."""
    d0 = dt.Frame([range(100),
                   [random.randint(0, 50) for _ in range(100)]],
                  names=["A", "B"])
    d1 = d0.sort("B")
    frame_integrity_check(d0)
    frame_integrity_check(d1)
    assert isview(d1)
    assert d0.shape == d1.shape == (100, 2)
    assert d0.names == d1.names == ("A", "B")
    a0, a1 = d1.to_list()
    assert sorted(a0) == list(range(100))
    assert a0 != list(range(100))
    assert a1 == sorted(a1)



#-------------------------------------------------------------------------------
# Int32
#-------------------------------------------------------------------------------

def test_int32_small():
    d0 = dt.Frame([17, 2, 96, 245, 847569, 34, -45, None, 1])
    assert d0.stypes == (stype.int32, )
    d1 = d0.sort(0)
    assert d1.stypes == d0.stypes
    assert isview(d1)
    frame_integrity_check(d1)
    assert d1.to_list() == [[None, -45, 1, 2, 17, 34, 96, 245, 847569]]


def test_int32_small_stable():
    d0 = dt.Frame([
        [5, 3, 5, None, 1000000, None, 3, None],
        [1, 5, 10, 20, 50, 100, 200, 500]
    ], names=["A", "B"])
    d1 = d0.sort("A")
    frame_integrity_check(d1)
    assert d1.to_list() == [
        [None, None, None, 3, 3, 5, 5, 1000000],
        [20, 100, 500, 5, 200, 1, 10, 50],
    ]


def test_int32_large():
    # p1, p2 should both be prime, with p1 < p2. Here we rely on the fact that
    # a multiplicative group of integers modulo n is cyclic with order n-1 when
    # n is prime.
    p1 = 1000003
    p2 = 2000003
    src = [((n + 1) * p2) % p1 for n in range(p1)]
    d0 = dt.Frame(src)
    assert d0.stypes == (stype.int32, )
    d1 = d0.sort(0)
    assert d1.to_list() == [list(range(p1))]


@pytest.mark.parametrize("n", [30, 300, 3000, 30000, 60000, 120000])
def test_int32_large_stable(n):
    src = [None, 100, 100000] * (n // 3)
    d0 = dt.Frame([src, range(n)], names=["A", "B"])
    assert d0.stypes[0] == stype.int32
    d1 = d0[:, "B", sort("A")]
    assert d1.to_list() == [list(range(0, n, 3)) +
                            list(range(1, n, 3)) +
                            list(range(2, n, 3))]


@pytest.mark.parametrize("n", [5, 100, 500, 2500, 32767, 32768, 32769, 200000])
def test_int32_constant(n):
    tbl0 = [[100000] * n, list(range(n))]
    d0 = dt.Frame(tbl0)
    assert d0.stypes[0] == stype.int32
    d1 = d0.sort(0)
    assert d1.stypes == d0.stypes
    assert d1.to_list() == tbl0


def test_int32_reverse_list():
    step = 10
    d0 = dt.Frame(range(1000000, 0, -step))
    assert d0.stypes[0] == stype.int32
    d1 = d0.sort(0)
    assert d1.stypes == d0.stypes
    assert d1.to_list() == [list(range(step, 1000000 + step, step))]


@pytest.mark.parametrize("b", [32767, 1000000])
def test_int32_upper_range(b):
    # This test looks at an i4 array with small range but large absolute values.
    # After subtracting the mean it will be converted into uint16_t for sorting,
    # and we test here that such conversion gives the correct results.
    d0 = dt.Frame([b, b - 1, b + 1] * 1000)
    assert d0.stypes[0] == stype.int32
    d1 = d0.sort(0)
    assert d1.to_list() == [[b - 1] * 1000 + [b] * 1000 + [b + 1] * 1000]


@pytest.mark.parametrize("dc", [32765, 32766, 32767, 32768,
                                65533, 65534, 65535, 65536])
def test_int32_u2range(dc):
    # Test array with range close to 2^15 / 2^16 (looking for potential off-by-1
    # errors at the boundary of int16_t / uint16_t)
    a = 100000
    b = a + 10
    c = a + dc
    d0 = dt.Frame([c, b, a] * 1000)
    assert d0.stypes[0] == stype.int32
    d1 = d0.sort(0)
    assert d1.to_list() == [[a] * 1000 + [b] * 1000 + [c] * 1000]


def test_int32_unsigned():
    # In this test the range of values is 32 bits, so that after removing the
    # radix we would have full 16 bits remaining. At that point we should be
    # careful not to conflate unsigned sorting with signed sorting.
    tbl = sum(([t] * 100 for t in [0x00000000, 0x00000001, 0x00007FFF,
                                   0x00008000, 0x00008001, 0x0000FFFF,
                                   0x7FFF0000, 0x7FFF0001, 0x7FFF7FFF,
                                   0x7FFF8000, 0x7FFF8001, 0x7FFFFFFF]), [])
    d0 = dt.Frame(tbl)
    assert d0.stypes == (stype.int32, )
    d1 = d0.sort(0)
    frame_integrity_check(d1)
    assert d1.to_list() == [tbl]


def test_int32_issue220():
    d0 = dt.Frame([None] + [1000000] * 200 + [None])
    d1 = d0.sort(0)
    assert d1.to_list() == [[None, None] + [1000000] * 200]



#-------------------------------------------------------------------------------
# Int8
#-------------------------------------------------------------------------------

def test_int8_small():
    d0 = dt.Frame([17, 2, 96, 45, 84, 75, 69, 34, -45, None, 1], stype=dt.int8)
    assert d0.stypes == (stype.int8, )
    d1 = d0.sort(0)
    assert d1.stypes == d0.stypes
    assert isview(d1)
    frame_integrity_check(d1)
    assert d1.to_list() == [[None, -45, 1, 2, 17, 34, 45, 69, 75, 84, 96]]


def test_int8_small_stable():
    d0 = dt.Frame([
        [5, 3, 5, None, 100, None, 3, None],
        [1, 5, 10, 20, 50, 100, 200, 500]
    ], names=["A", "B"])
    d1 = d0[:, :, sort(f.A)]
    frame_integrity_check(d1)
    assert d1.to_list() == [
        [None, None, None, 3, 3, 5, 5, 100],
        [20, 100, 500, 5, 200, 1, 10, 50],
    ]


def test_int8_large():
    d0 = dt.Frame([(i * 1327) % 101 - 50 for i in range(1010)], stype=dt.int8)
    d1 = d0.sort(0)
    assert d1.stypes == (stype.int8, )
    frame_integrity_check(d1)
    assert d1.to_list() == [sum(([i] * 10 for i in range(-50, 51)), [])]


@pytest.mark.parametrize("n", [30, 303, 3333, 30000, 60009, 120000])
def test_int8_large_stable(n):
    src = [None, 10, -10] * (n // 3)
    d0 = dt.Frame([src, range(n)], names=("A", "B"), stypes={"A": "int8"})
    assert d0.stypes[0] == stype.int8
    d1 = d0[:, f.B, sort(f.A)]
    assert d1.to_list() == [list(range(0, n, 3)) +
                            list(range(2, n, 3)) +
                            list(range(1, n, 3))]



#-------------------------------------------------------------------------------

def test_bool8_small():
    d0 = dt.Frame([True, False, False, None, True, True, None])
    assert d0.stypes == (stype.bool8, )
    d1 = d0[:, :, sort("C0")]
    assert d1.stypes == d0.stypes
    assert isview(d1)
    frame_integrity_check(d1)
    assert d1.to_list() == [[None, None, False, False, True, True, True]]


def test_bool8_small_stable():
    d0 = dt.Frame([[True, False, False, None, True, True, None],
                   [1, 2, 3, 4, 5, 6, 7]],
                  names=["A", "B"], stypes={"B": "int8"})
    assert d0.stypes == (stype.bool8, stype.int8)
    d1 = d0[:, :, sort("A")]
    assert d1.stypes == d0.stypes
    assert d1.names == d0.names
    assert isview(d1)
    frame_integrity_check(d1)
    assert d1.to_list() == [[None, None, False, False, True, True, True],
                            [4, 7, 2, 3, 1, 5, 6]]


@pytest.mark.parametrize("n", [100, 512, 1000, 5000, 100000])
def test_bool8_large(n):
    d0 = dt.Frame([True, False, True, None, None, False] * n)
    assert d0.stypes == (stype.bool8, )
    d1 = d0.sort(0)
    assert d1.stypes == d0.stypes
    assert d1.names == d0.names
    assert isview(d1)
    frame_integrity_check(d0)
    frame_integrity_check(d1)
    nn = 2 * n
    assert d1.to_list() == [[None] * nn + [False] * nn + [True] * nn]


@pytest.mark.parametrize("n", [254, 255, 256, 257, 258, 1000, 10000])
def test_bool8_large_stable(n):
    d0 = dt.Frame([[True, False, None] * n, range(3 * n)], names=["A", "B"])
    assert d0.stypes[0] == stype.bool8
    d1 = d0[:, f.B, sort(f.A)]
    assert isview(d1)
    frame_integrity_check(d1)
    assert d1.to_list() == [list(range(2, 3 * n, 3)) +
                            list(range(1, 3 * n, 3)) +
                            list(range(0, 3 * n, 3))]



#-------------------------------------------------------------------------------

def test_int16_small():
    d0 = dt.Frame([0, -10, 100, -1000, 10000, 2, 999, None], stype=dt.int16)
    assert d0.stypes[0] == stype.int16
    d1 = d0.sort(0)
    frame_integrity_check(d1)
    assert d1.to_list() == [[None, -1000, -10, 0, 2, 100, 999, 10000]]


def test_int16_small_stable():
    d0 = dt.Frame([[0, 1000, 0, 0, 1000, 0, 0, 1000, 0],
                   [1, 2, 3, 4, 5, 6, 7, 8, 9]],
                  names=["A", "B"], stypes={"A": "int16"})
    assert d0.stypes[0] == stype.int16
    d1 = d0.sort(0)
    frame_integrity_check(d1)
    assert d1.to_list() == [[0, 0, 0, 0, 0, 0, 1000, 1000, 1000],
                            [1, 3, 4, 6, 7, 9, 2, 5, 8]]


def test_int16_large():
    d0 = dt.Frame([(i * 111119) % 10007 - 5003 for i in range(10007)],
                  stype=dt.int16)
    d1 = d0.sort(0)
    assert d1.stypes == (stype.int16, )
    frame_integrity_check(d1)
    assert d1.to_list() == [list(range(-5003, 5004))]


@pytest.mark.parametrize("n", [100, 150, 200, 500, 1000, 200000])
def test_int16_large_stable(n):
    d0 = dt.Frame([[-5, None, 5, -999, 1000] * n, range(n * 5)],
                  names=["A", "B"], stypes={"A": "int16"})
    assert d0.stypes[0] == stype.int16
    d1 = d0[:, f.B, sort(f.A)]
    frame_integrity_check(d1)
    assert d1.to_list() == [list(range(1, 5 * n, 5)) +
                            list(range(3, 5 * n, 5)) +
                            list(range(0, 5 * n, 5)) +
                            list(range(2, 5 * n, 5)) +
                            list(range(4, 5 * n, 5))]



#-------------------------------------------------------------------------------

def test_int64_small():
    d0 = dt.Frame([10**(i * 101 % 13) for i in range(13)] + [None])
    assert d0.stypes == (stype.int64, )
    d1 = d0.sort(0)
    assert d1.stypes == d0.stypes
    frame_integrity_check(d1)
    assert d1.to_list() == [[None] + [10**i for i in range(13)]]


def test_int64_small_stable():
    d0 = dt.Frame([[0, None, -1000, 11**11] * 3, range(12)])
    assert d0.stypes == (stype.int64, stype.int32)
    d1 = d0[:, 1, sort(0)]
    frame_integrity_check(d1)
    assert d1.to_list() == [[1, 5, 9, 2, 6, 10, 0, 4, 8, 3, 7, 11]]


@pytest.mark.parametrize("n", [16, 20, 30, 40, 50, 100, 500, 1000])
def test_int64_large0(n):
    a = -6654966461866573261
    b = -6655043958000990616
    c = 5207085498673612884
    d = 5206891724645893889
    d0 = dt.Frame([c, d, a, b] * n)
    d1 = d0.sort(0)
    frame_integrity_check(d0)
    frame_integrity_check(d1)
    assert isview(d1)
    assert b < a < d < c
    assert d0.to_list() == [[c, d, a, b] * n]
    assert d1.to_list() == [[b] * n + [a] * n + [d] * n + [c] * n]


@pytest.mark.parametrize("seed", [random.getrandbits(63) for i in range(10)])
def test_int64_large_random(seed):
    random.seed(seed)
    m = 2**63 - 1
    tbl = [random.randint(-m, m) for i in range(1000)]
    d0 = dt.Frame(tbl)
    assert d0.stypes == (stype.int64, )
    d1 = d0.sort(0)
    assert d1.to_list() == [sorted(tbl)]



#-------------------------------------------------------------------------------

def test_float32_small():
    d0 = dt.Frame([0, .4, .9, .2, .1, nan, -inf, -5, 3, 11, inf, 5.2],
                  stype="float32")
    assert d0.stypes == (stype.float32, )
    d1 = d0.sort(0)
    dr = dt.Frame([None, -inf, -5, 0, .1, .2, .4, .9, 3, 5.2, 11, inf])
    assert list_equals(d1.to_list(), dr.to_list())


def test_float32_nans():
    d0 = dt.Frame([nan, 0.5, nan, nan, -3, nan, 0.2, nan, nan, 1],
                  stype="float32")
    d1 = d0.sort(0)
    assert list_equals(d1.to_list(),
                       [[None, None, None, None, None, None, -3, 0.2, 0.5, 1]])


def test_float32_large():
    d0 = dt.Frame([-1000, 0, 1.5e10, 7.2, math.inf] * 100, stype="float32")
    assert d0.stypes == (stype.float32, )
    d1 = d0.sort(0)
    frame_integrity_check(d1)
    dr = dt.Frame([-1000] * 100 + [0] * 100 + [7.2] * 100 +
                  [1.5e10] * 100 + [math.inf] * 100)
    assert list_equals(d1.to_list(), dr.to_list())


@pytest.mark.parametrize("n", [15, 16, 17, 20, 50, 100, 1000, 100000])
def test_float32_random(numpy, n):
    a = numpy.random.rand(n).astype("float32")
    d0 = dt.Frame(a)
    assert d0.stypes == (stype.float32, )
    d1 = d0.sort(0)
    assert list_equals(d1.to_list()[0], sorted(a.tolist()))



#-------------------------------------------------------------------------------

def test_float64_small():
    d0 = dt.Frame([0.1, -0.5, 1.6, 0, None, -inf, inf, 3.3, 1e100])
    assert d0.stypes == (stype.float64, )
    d1 = d0.sort(0)
    frame_integrity_check(d1)
    dr = dt.Frame([None, -inf, -0.5, 0, 0.1, 1.6, 3.3, 1e100, inf])
    assert list_equals(d1.to_list(), dr.to_list())


def test_float64_zeros():
    z = 0.0
    d0 = dt.Frame([0.5] + [z, -z] * 100)
    assert d0.stypes == (stype.float64, )
    d1 = d0.sort(0)
    frame_integrity_check(d1)
    dr = dt.Frame([-z] * 100 + [z] * 100 + [0.5])
    assert str(d1.to_list()) == str(dr.to_list())


@pytest.mark.parametrize("n", [20, 100, 500, 2500, 20000])
def test_float64_large(n):
    d0 = dt.Frame([12.6, .3, inf, -5.1, 0, -inf, None] * n)
    assert d0.stypes == (stype.float64, )
    d1 = d0.sort(0)
    frame_integrity_check(d1)
    dr = dt.Frame([None] * n + [-inf] * n + [-5.1] * n +
                  [0] * n + [.3] * n + [12.6] * n + [inf] * n)
    assert list_equals(d1.to_list(), dr.to_list())


@pytest.mark.parametrize("n", [15, 16, 17, 20, 50, 100, 1000, 100000])
def test_float64_random(numpy, n):
    a = numpy.random.rand(n)
    d0 = dt.Frame(a)
    assert d0.stypes == (stype.float64, )
    d1 = d0.sort(0)
    assert list_equals(d1.to_list()[0], sorted(a.tolist()))



#-------------------------------------------------------------------------------
# Sort views
#-------------------------------------------------------------------------------

def test_sort_view1():
    d0 = dt.Frame([5, 10])
    d1 = d0[[i % 2 for i in range(10)], :]
    assert d1.shape == (10, 1)
    frame_integrity_check(d1)
    assert isview(d1)
    d2 = d1[:, :, sort(0)]
    assert d2.shape == d1.shape
    frame_integrity_check(d2)
    assert isview(d2)
    assert d2.to_list() == [[5] * 5 + [10] * 5]


def test_sort_view2():
    d0 = dt.Frame([4, 1, 0, 5, -3, 12, 99, 7])
    d1 = d0.sort(0)
    d2 = d1[:, :, sort(0)]
    frame_integrity_check(d2)
    assert d2.to_list() == d1.to_list()


def test_sort_view3():
    d0 = dt.Frame(range(1000))
    d1 = d0[::-5, :]
    d2 = d1[:, :, sort(0)]
    frame_integrity_check(d2)
    assert d2.shape == (200, 1)
    assert d2.to_list() == [list(range(4, 1000, 5))]


def test_sort_view4():
    d0 = dt.Frame(["foo", "bar", "baz", None, "", "lalala", "quo",
                   "rem", "aye", "nay"])
    d1 = d0[1::2, :].sort(0)
    d2 = d0[0::2, :].sort(0)
    frame_integrity_check(d1)
    frame_integrity_check(d2)
    assert d1.shape == d2.shape == (5, 1)
    assert d1.to_list() == [[None, "bar", "lalala", "nay", "rem"]]
    assert d2.to_list() == [["", "aye", "baz", "foo", "quo"]]


def test_sort_view_large_strs():
    d0 = dt.Frame(list("abcbpeiuqenvkjqperufhqperofin;d") * 100)
    d1 = d0[:, ::2].sort(0)
    frame_integrity_check(d1)
    elems = d1.to_list()[0]
    assert elems == sorted(elems)


@pytest.mark.parametrize("st", [stype.bool8, stype.int8, stype.int16,
                                stype.int32, stype.int64, stype.float32,
                                stype.float64, stype.str32, stype.str64])
def test_sort_view_all_stypes(st):
    def random_bool():
        return random.choice([True, True, False, False, None])

    def random_int():
        if random.random() < 0.1: return None
        return random.randint(-100000, 1999873)

    def random_real():
        if random.random() < 0.1: return None
        return random.normalvariate(5, 10)

    def random_str():
        if random.random() < 0.1: return None
        return random_string(random.randint(1, 20))

    def sortkey_num(x):
        return -1e10 if x is None else x

    def sortkey_str(x):
        return "" if x is None else x

    fn = (random_bool if st == stype.bool8 else
          random_int if st.ltype == ltype.int else
          random_real if st.ltype == ltype.real else
          random_str)
    sortkey = (sortkey_str if st.ltype == ltype.str else
               sortkey_num)
    n = 1000
    src = [fn() for _ in range(n)]
    d0 = dt.Frame(src, stype=st)
    if st in (stype.int8, stype.int16, stype.float32):
        src = d0.to_list()[0]
    d1 = d0[::3, :].sort(0)
    frame_integrity_check(d0)
    frame_integrity_check(d1)
    assert d1.shape == ((n + 2) // 3, 1)
    assert d1.stypes == (st, )
    assert d1.to_list()[0] == sorted(src[::3], key=sortkey)



#-------------------------------------------------------------------------------
# Str32/Str64
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("st", [dt.str32, dt.str64])
def test_strXX_small1(st):
    src = ["MOTHER OF EXILES. From her beacon-hand",
           "Glows world-wide welcome; her mild eyes command",
           "The air-bridged harbor that twin cities frame.",
           "'Keep ancient lands, your storied pomp!' cries she",
           "With silent lips. 'Give me your tired, your poor,",
           "Your huddled masses yearning to breathe free,",
           "The wretched refuse of your teeming shore.",
           "Send these, the homeless, tempest-tost to me,",
           "I lift my lamp beside the golden door!'"]
    d0 = dt.Frame(src, stype=st)
    assert d0.stypes == (st, )
    d1 = d0.sort(0)
    frame_integrity_check(d1)
    assert d1.to_list() == [sorted(src)]


@pytest.mark.parametrize("st", [dt.str32, dt.str64])
def test_strXX_small2(st):
    src = ["Welcome", "Welc", "", None, "Welc", "Welcome!", "Welcame"]
    d0 = dt.Frame(src, stype=st)
    assert d0.stypes == (st, )
    d1 = d0.sort(0)
    frame_integrity_check(d1)
    src.remove(None)
    assert d1.to_list() == [[None] + sorted(src)]


@pytest.mark.parametrize("st", [dt.str32, dt.str64])
def test_strXX_large1(st):
    src = list("dfbvoiqeubvqoiervblkdfbvqoiergqoiufbvladfvboqiervblq"
               "134ty1-394 8n09er8gy209rg hwdoif13-40t 9u13- jdpfver")
    d0 = dt.Frame(src, stype=st)
    assert d0.stypes == (st, )
    d1 = d0.sort(0)
    frame_integrity_check(d1)
    assert d1.to_list() == [sorted(src)]


@pytest.mark.parametrize("st", [dt.str32, dt.str64])
def test_strXX_large2(st):
    src = ["test-%d" % (i * 1379 % 200) for i in range(200)]
    d0 = dt.Frame(src, stype=st)
    assert d0.stypes == (st, )
    d1 = d0.sort(0)
    frame_integrity_check(d1)
    assert d1.to_list() == [sorted(src)]


@pytest.mark.parametrize("st", [dt.str32, dt.str64])
def test_strXX_large3(st):
    src = ["aa"] * 100 + ["abc", "aba", "abb", "aba", "abc"]
    d0 = dt.Frame(src, stype=st)
    assert d0.stypes == (st, )
    d1 = d0.sort(0)
    frame_integrity_check(d1)
    assert d1.to_list() == [sorted(src)]


@pytest.mark.parametrize("st", [dt.str32, dt.str64])
def test_strXX_large4(st):
    src = ["aa"] * 100 + ["ab%d" % (i // 10) for i in range(100)] + \
          ["bb", "cce", "dd"] * 25 + ["ff", "gg", "hhe"] * 10 + \
          ["ff3", "www"] * 2
    d0 = dt.Frame(src, stype=st)
    assert d0.stypes == (st, )
    d1 = d0.sort(0)
    frame_integrity_check(d1)
    assert d1.to_list() == [sorted(src)]


@pytest.mark.parametrize("st", [dt.str32, dt.str64])
def test_strXX_large5(st):
    src = ['acol', 'acols', 'acolu', 'acells', 'achar', 'bdatatable!'] * 20 + \
          ['bd', 'bdat', 'bdata', 'bdatr', 'bdatx', 'bdatatable'] * 5 + \
          ['bdt%d' % (i // 2) for i in range(30)]
    random.shuffle(src)
    dt0 = dt.Frame(src, stype=st)
    assert dt0.stypes == (st, )
    dt1 = dt0[:, :, sort(0)]
    frame_integrity_check(dt1)
    assert dt1.to_list()[0] == sorted(src)


@pytest.mark.parametrize("st", [dt.str32, dt.str64])
def test_strXX_large6(st):
    rootdir = os.path.join(os.path.dirname(__file__), "..", "c")
    assert os.path.isdir(rootdir)
    words = []
    for dirname, _, files in os.walk(rootdir):
        for filename in files:
            f = os.path.join(dirname, filename)
            with open(f, "r", encoding="utf-8") as inp:
                txt = inp.read()
            words.extend(txt.split())
    dt0 = dt.Frame(words, stype=st)
    assert dt0.stypes == (st, )
    dt1 = dt0.sort(0)
    frame_integrity_check(dt1)
    assert dt1.to_list()[0] == sorted(words)



#-------------------------------------------------------------------------------
# Sort by multiple columns
#-------------------------------------------------------------------------------

def test_sort_len0_multi():
    d0 = dt.Frame([[], [], []], names=["E", "R", "G"])
    assert d0.shape == (0, 3)
    d1 = d0.sort(0, 2, 1)
    frame_integrity_check(d1)
    assert d1.shape == (0, 3)
    assert d1.names == d0.names


def test_sort_len1_multi():
    d0 = dt.Frame([[17], [2.99], ["foo"]], names=["Q", "u", "a"])
    assert d0.shape == (1, 3)
    d1 = d0.sort(0, 1, 2)
    frame_integrity_check(d1)
    assert d1.shape == (1, 3)
    assert d1.names == d0.names
    assert d1.to_list() == d0.to_list()


def test_int32_small_multi():
    src = [
        [1, 3, 2, 7, 2, 1, 1, 7, 2, 1, 1, 7],
        [5, 1, 9, 4, 1, 0, 3, 2, 7, 5, 8, 1]
    ]
    d0 = dt.Frame(src, names=["A", "B"], stype=dt.stype.int32)
    d1 = d0.sort("A", "B")
    order = sorted(range(len(src[0])), key=lambda i: (src[0][i], src[1][i]))
    frame_integrity_check(d1)
    assert d1.names == d0.names
    assert d1.to_list() == [[src[0][i] for i in order],
                            [src[1][i] for i in order]]


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_bool8_2cols_multi(seed):
    random.seed(seed)
    n = int(random.expovariate(0.001) + 200)
    data = [[random.choice([True, False]) for _ in range(n)] for j in range(2)]
    n0 = sum(data[0])
    n10 = sum(data[1][i] for i in range(n) if data[0][i] is False)
    n11 = sum(data[1][i] for i in range(n) if data[0][i] is True)
    d0 = dt.Frame(data)
    d1 = d0.sort(0, 1)
    assert d1.to_list() == [[False] * (n - n0) + [True] * n0,
                            [False] * (n - n0 - n10) + [True] * n10 +
                            [False] * (n0 - n11) + [True] * n11]


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_bool8_manycols_multi(seed):
    random.seed(seed)
    nrows = int(random.expovariate(0.005) + 100)
    ncols = int(random.expovariate(0.01) + 2)
    data = [[random.choice([True, False]) for _ in range(nrows)]
            for j in range(ncols)]
    d0 = dt.Frame(data)
    d1 = d0.sort(*list(range(ncols)))
    d0_sorted = sorted(d0.to_csv().split("\n")[1:-1])
    d1_sorted = d1.to_csv().split("\n")[1:-1]
    assert d0_sorted == d1_sorted


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_multisort_bool_real(seed):
    random.seed(seed)
    n = int(random.expovariate(0.001) + 200)
    col0 = [random.choice([True, False]) for _ in range(n)]
    col1 = [random.randint(1, 10) / 97 for _ in range(n)]
    d0 = dt.Frame([col0, col1])
    d1 = d0.sort(0, 1)
    frame_integrity_check(d1)
    n0 = sum(col0)
    assert d1.to_list() == [
        [False] * (n - n0) + [True] * n0,
        sorted([col1[i] for i in range(n) if col0[i] is False]) +
        sorted([col1[i] for i in range(n) if col0[i] is True])]


@pytest.mark.parametrize("seed", [random.getrandbits(32) for _ in range(10)])
def test_sort_random_multi(seed):
    def random_bool():
        return random.choice([True, False])

    def random_int():
        return random.randint(-10, 10)

    def random_real():
        return random.choice([-1.95, 0.1, 0.3, 1.1, 7.3, -2.99, 9.13, 4.555])

    def random_str():
        return random.choice(["", "foo", "enlkq", "n1-48", "fooba", "enkyi",
                              "zweb", "z", "vn;e", "skdjhfb", "amruye", "f"])

    random.seed(seed)
    n = int(random.expovariate(0.01) + 2)
    data = []
    for _ in range(5):
        fn = random.choice([random_bool, random_int, random_real, random_str])
        data.append([fn() for _ in range(n)])
    data.append([random.random() for _ in range(n)])
    order = sorted(list(range(n)),
                   key=lambda x: (data[1][x], data[2][x], data[3][x], x))
    sorted_data = [[col[j] for j in order] for col in data]
    d0 = dt.Frame(data, names=list("ABCDEF"))
    frame_integrity_check(d0)
    d1 = d0.sort("B", "C", "D")
    assert d1.to_list() == sorted_data



#-------------------------------------------------------------------------------
# Sort in reverse order
#-------------------------------------------------------------------------------

def test_sort_bools_reverse():
    DT = dt.Frame(A=[True, None, False, None, True, None], B=list('abcdef'))
    assert_equals(DT[:, :, sort(-f.A)],
                  dt.Frame(A=[None, None, None, True, True, False],
                           B=['b', 'd', 'f', 'a', 'e', 'c']))


@pytest.mark.parametrize("st", dt.ltype.int.stypes)
def test_sort_ints_reverse(st):
    DT = dt.Frame(A=[5, 17, 9, -12, 0, 111, 3, 5], B=list('abcdefgh'),
                  stypes={"A": st, "B": dt.str32})
    assert_equals(DT[:, :, sort(-f.A)],
                  dt.Frame(A=[111, 17, 9, 5, 5, 3, 0, -12],
                           B=list('fbcahged'),
                           stypes={"A": st, "B": dt.str32}))


def test_sort_doubles_reverse():
    DT = dt.Frame(A=[0.0, 0.1, -0.5, 1.6, -0.0, None, -inf, inf, 3.3, 1e100])
    assert_equals(DT[:, :, sort(-f.A)],
                  dt.Frame(A=[None, inf, 1e100, 3.3, 1.6, 0.1, 0.0, -0.0,
                              -0.5, -inf]))


def test_sort_double_stable_nans():
    DT = dt.Frame(A=[nan, -nan, nan, -inf, None, inf, 9.99, None],
                  B=list('abcdefgh'))
    assert_equals(DT[:, :, sort(-f.A)],
                  dt.Frame(A=[None] * 5 + [inf, 9.99, -inf],
                           B=list('abcehfgd')))


@pytest.mark.parametrize("st", [dt.str32, dt.str64])
def test_sort_strings_reverse(st):
    DT = dt.Frame(A=['aye', '', 'zebra', 'zulu', 'nautilus', None, 'oxen'],
                  stype=st)
    RES = dt.Frame(A=[None, 'zulu', 'zebra', 'oxen', 'nautilus', 'aye', ''],
                   stype=st)
    assert_equals(DT[:, :, sort(-f.A)], RES)


def test_sort_strings_reverse_large():
    src = ['klein', 'nim', 'toapr', 'f', '', 'zleu', '?34', '.............']
    src *= 10
    src += ['adferg', 'reneeas', 'ldodls', 'qu', 'zleuss', 'ni'] * 7
    src *= 25
    src += ['shoo!', 'zzZzzZ' * 5]
    DT = dt.Frame(A=src)
    RES = dt.Frame(A=sorted(src, reverse=True))
    assert_equals(DT[:, :, sort(-f.A)], RES)




#-------------------------------------------------------------------------------
# Misc issues
#-------------------------------------------------------------------------------

def test_sort_api():
    df = dt.Frame([[1, 2, 1, 2], [3.3, 2.7, 0.1, 4.5]], names=["A", "B"])
    df1 = df.sort("A")
    df2 = df.sort("B")
    df3 = df.sort("A", "B")
    df4 = df.sort(["A", "B"])
    df5 = df.sort()  # issue 1354
    assert df1.to_list() == [[1, 1, 2, 2], [3.3, 0.1, 2.7, 4.5]]
    assert df2.to_list() == [[1, 2, 1, 2], [0.1, 2.7, 3.3, 4.5]]
    assert df3.to_list() == [[1, 1, 2, 2], [0.1, 3.3, 2.7, 4.5]]
    assert df4.to_list() == df5.to_list() == df3.to_list()


def test_issue1401():
    col = [9.0, 21.0, 23.0, 40.0, 5.0, 49.0, 16.0, 2.0, 49.0, 39.0, 46.0, 21.0,
           42.0, 47.0, 43.0, 20.0, 44.0, 26.0, 3.0, 44.0, 40.0, 42.0, 31.0, 7.0,
           35.0, 15.0, 28.0, 32.0, 41.0, 44.0, 49.0, 21.0, 8.0, 35.0, 44.0, 44,
           33.0, 26.0, 21.0, 46.0, 15.0, 41.0, 34.0, 10.0, 19.0, 21.0, 27.0, 13,
           20.0, 35.0, 23.0, 32.0, 47.0, 27.0, 39.0, 3.0, 36.0, 6.0, 13.0, 38.0,
           8.0, 33.0, 27.0, 32.0, 11.0]
    DT = dt.Frame([['a'] * 65, col], names=["A", "B"])
    res = DT.sort("A", "B")
    assert res.to_list()[1] == sorted(col)


def test_issue1857(numpy):
    nrows = 3620
    numpy.random.seed(364)
    DT = dt.Frame(n1=numpy.random.rand(nrows).astype(numpy.float32),
                  g1=numpy.random.randint(0, 10, nrows),
                  g2=numpy.random.randint(0, 10, nrows))
    agg1 = DT[:, {"M": dt.median(f.n1)}, by(f.g1, f.g2)]
    assert agg1.shape == (100, 3)
    assert agg1.names == ("g1", "g2", "M")
    assert agg1.stypes == (stype.int64, stype.int64, stype.float32)
    assert agg1.sum().to_tuples()[0] == (450, 450, 51.63409462571144)


def test_sort_expr():
    df = dt.Frame(A=[1, 2, 1, 2], B=[3.9, 2.7, 0.1, 4.5])
    assert_equals(df[:, :, sort("A")],
                  dt.Frame(A=[1, 1, 2, 2], B=[3.9, 0.1, 2.7, 4.5]))
    assert_equals(df[:, :, sort(f.B)],
                  dt.Frame(A=[1, 2, 1, 2], B=[0.1, 2.7, 3.9, 4.5]))
    assert_equals(df[:, 'B', by("A"), sort("B")],
                  dt.Frame(A=[1, 1, 2, 2], B=[0.1, 3.9, 2.7, 4.5]))


def test_h2oai7014(tempfile_jay):
    data = dt.Frame([[None, 't'], [3580, 1047]], names=["ID", "count"])
    data.to_jay(tempfile_jay)
    # The data has to be opened from file
    counts = dt.fread(tempfile_jay)
    counts = counts[1:, :]
    counts = counts[:, :, sort("count")]
    counts.materialize()
    assert counts.to_list() == [['t'], [1047]]
