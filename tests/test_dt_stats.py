#!/usr/bin/env python
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
import random
import statistics
from datatable import stype, ltype
from datatable.internal import frame_integrity_check
from math import inf, nan, isnan
from tests import list_equals


srcs_bool = [[False, True, False, False, True],
             [True, None, None, True, False],
             [True], [False], [None] * 10]
srcs_int = [[5, -3, 6, 3, 0],
            [None, -1, 0, 26, -3],
            [129, 38, 27, -127, 8],
            [385, None, None, -3, -89],
            [-192, 32769, 683, 94, 0],
            [None, -32788, -4, -44444, 5],
            [30, -284928, 59, 3, 2147483649],
            [2147483648, None, None, None, None],
            [-1, 1], [100], [0]]
srcs_real = [[9.5, 0.2, 5.4857301, -3.14159265358979],
             [1.1, 2.3e12, -.5, None, inf, 0.0],
             [3.5, 2.36, nan, 696.9, 4097],
             [3.1415926535897932], [nan]]

srcs_str = [["foo", None, "bar", "baaz", None],
            ["a", "c", "d", None, "d", None, None, "a", "e", "c", "a", "a"],
            ["leeeeeroy!"],
            (dt.str64, ["abc", None, "def", "abc", "a", None, "a", "ab"]),
            (dt.str64, [None, "integrity", None, None, None, None, None]),
            (dt.str64, ["f", "c", "e", "a", "c", "d", "f", "c", "e", "A", "a"])]

srcs_obj = [[1, None, "haha", nan, inf, None, (1, 2)],
            ["a", "bc", "def", None, -2.5, 3.7]]

srcs_numeric = srcs_bool + srcs_int + srcs_real
srcs_all = srcs_numeric + srcs_str



#-------------------------------------------------------------------------------
# Minimum function dt.min()
#-------------------------------------------------------------------------------

def t_min(t):
    t = [i for i in t if i is not None and not isnan(i)]
    if len(t) == 0:
        return None
    else:
        return min(t)


@pytest.mark.parametrize("src", srcs_numeric)
def test_min(src):
    dt0 = dt.Frame(src)
    dtr = dt0.min()
    frame_integrity_check(dtr)
    assert dtr.names == dt0.names
    assert dtr.stypes == dt0.stypes
    assert dtr.shape == (1, 1)
    assert dtr.to_list() == [[t_min(src)]]
    assert dtr[0, 0] == dt0.min1()


def test_dt_str():
    dt0 = dt.Frame([[1, 5, 3, 9, -2], list("abcde")])
    dtr = dt0.min()
    frame_integrity_check(dtr)
    assert dtr.to_list() == [[-2], [None]]



#-------------------------------------------------------------------------------
# Maximum function dt.max()
#-------------------------------------------------------------------------------

def t_max(t):
    t = [i for i in t if i is not None and not isnan(i)]
    if len(t) == 0:
        return None
    else:
        return max(t)


@pytest.mark.parametrize("src", srcs_numeric)
def test_max(src):
    dt0 = dt.Frame(src)
    dtr = dt0.max()
    frame_integrity_check(dtr)
    assert dtr.names == dt0.names
    assert dtr.stypes == dt0.stypes
    assert dtr.shape == (1, 1)
    assert dtr.to_list() == [[t_max(src)]]
    assert dtr[0, 0] == dt0.max1()



#-------------------------------------------------------------------------------
# Sum function dt.sum()
#-------------------------------------------------------------------------------

def t_sum(t):
    t = [i for i in t if i is not None and not isnan(i)]
    if len(t) == 0:
        return 0
    else:
        return sum(t)


@pytest.mark.parametrize("src", srcs_numeric)
def test_sum(src):
    dt0 = dt.Frame(src)
    dtr = dt0.sum()
    frame_integrity_check(dtr)
    assert dtr.stypes == (stype.float64, ) * dt0.ncols
    assert dtr.shape == (1, dt0.ncols)
    assert dt0.names == dtr.names
    assert list_equals(dtr.to_list(), [[t_sum(src)]])
    assert list_equals([dtr[0, 0]], [dt0.sum1()])



#-------------------------------------------------------------------------------
# Mean function dt.mean()
#-------------------------------------------------------------------------------

def t_mean(t):
    t = [i for i in t if i is not None and not isnan(i)]
    if len(t) == 0:
        return None
    else:
        return statistics.mean(t)

@pytest.mark.parametrize("src", srcs_numeric)
def test_dt_mean(src):
    dt0 = dt.Frame(src)
    dtr = dt0.mean()
    frame_integrity_check(dtr)
    assert dt0.names == dtr.names
    assert dtr.stypes == (stype.float64,)
    assert dtr.shape == (1, 1)
    assert list_equals(dtr.to_list(), [[t_mean(src)]])
    assert list_equals([dtr[0, 0]], [dt0.mean1()])


@pytest.mark.parametrize("src, res", [([1, 3, 5, None], 3),
                                      ([], None),
                                      ([10], 10),
                                      ([1.5], 1.5),
                                      ([2, 4], 3),
                                      ([None, 1.3], 1.3),
                                      ([None, None, None], None),
                                      ([inf, inf, inf], inf),
                                      ([5, 1, inf], inf),
                                      ([inf, 4, 2], inf),
                                      ([-inf, -inf, -inf, -inf], -inf),
                                      ([-inf, 0, inf], None),
                                      ])
def test_dt_mean_special_cases(src, res):
    dt0 = dt.Frame([src])
    dtr = dt0.mean()
    frame_integrity_check(dtr)
    assert dt0.names == dtr.names
    assert dtr.stypes == (stype.float64,)
    assert dtr.shape == (1, 1)
    assert list_equals(dtr.to_list(), [[res]])



#-------------------------------------------------------------------------------
# Standard Deviation function dt.sd()
#-------------------------------------------------------------------------------

def t_sd(t):
    t = [i for i in t
         if i is not None and not isnan(i)]
    if len(t) == 0:
        return None
    elif len(t) == 1:
        return 0
    else:
        res = statistics.stdev(t)
        return res if not isnan(res) else None

@pytest.mark.parametrize("src", srcs_numeric)
def test_dt_sd(src):
    dt0 = dt.Frame(src)
    dtr = dt0.sd()
    frame_integrity_check(dtr)
    assert dtr.stypes == (stype.float64, )
    assert dtr.shape == (1, 1)
    assert dt0.names == dtr.names
    assert list_equals(dtr.to_list(), [[t_sd(src)]])
    assert list_equals([dtr[0, 0]], [dt0.sd1()])


@pytest.mark.parametrize("src, res", [([1, 3, 5, None], 2.0),
                                      ([], None),
                                      ([3.0], 0),
                                      ([None, None, None], None),
                                      ([inf, inf, inf], None),
                                      ([-inf, 0, inf], None),
                                      ])
def test_dt_sd_special_cases(src, res):
    dt0 = dt.Frame([src])
    dtr = dt0.sd()
    frame_integrity_check(dtr)
    assert dtr.stypes == (stype.float64, )
    assert dtr.shape == (1, 1)
    assert dt0.names == dtr.names
    assert list_equals(dtr.to_list(), [[res]])



#-------------------------------------------------------------------------------
# Count_na function dt.count_na()
#-------------------------------------------------------------------------------

def t_count_na(arr):
    return sum(x is None or (isinstance(x, float) and isnan(x))
               for x in arr)


@pytest.mark.parametrize("src", srcs_all + srcs_obj)
def test_dt_count_na(src):
    if isinstance(src, tuple):
        dt0 = dt.Frame(src[1], stype=src[0])
        ans = t_count_na(src[1])
    else:
        dt0 = dt.Frame(src)
        ans = t_count_na(src)
    dtr = dt0.countna()
    frame_integrity_check(dtr)
    assert dtr.stypes == (stype.int64, )
    assert dtr.shape == (1, 1)
    assert dt0.names == dtr.names
    assert dtr.to_list() == [[ans]]
    assert dtr[0, 0] == dt0.countna1()



#-------------------------------------------------------------------------------
# N_unique function
#-------------------------------------------------------------------------------

def n_unique(arr):
    return len(set(arr) - {None, nan})

@pytest.mark.parametrize("src", srcs_all)
def test_dt_n_unique(src):
    if isinstance(src, tuple):
        dt0 = dt.Frame(src[1], stype=src[0])
        ans = n_unique(src[1])
    else:
        dt0 = dt.Frame(src)
        ans = n_unique(src)
    dtr = dt0.nunique()
    frame_integrity_check(dtr)
    assert dtr.stypes == (stype.int64, )
    assert dtr.shape == (1, 1)
    assert dtr.names == dt0.names
    assert dtr[0, 0] == ans
    assert dtr[0, 0] == dt0.nunique1()



#-------------------------------------------------------------------------------
# Mode function
#-------------------------------------------------------------------------------

def t_mode(arr):
    """
    Returns mode of arr + count of modal values, the mode is returned as a set
    of possible values (since it is not unique).
    """
    counts = {}
    for x in arr:
        if x is None or (isinstance(x, float) and isnan(x)):
            continue
        if x not in counts:
            counts[x] = 0
        counts[x] += 1
    if not counts:
        return 0, set()
    else:
        maxcnt = max(counts.values())
        return maxcnt, set(x for x in counts if counts[x] == maxcnt)

@pytest.mark.parametrize("src", srcs_all)
def test_mode(src):
    if isinstance(src, tuple): return
    f0 = dt.Frame(src)
    dtm = f0.mode()
    dtn = f0.nmodal()
    modal_count, modal_values = t_mode(src)
    frame_integrity_check(dtm)
    frame_integrity_check(dtn)
    assert dtm.shape == dtn.shape == (1, 1)
    assert dtm.names == dtn.names == f0.names
    assert dtm.stypes == f0.stypes
    assert dtn.stypes == (stype.int64, )
    if modal_count:
        assert dtm[0, 0] in modal_values
        assert dtm[0, 0] == f0.mode1()
        assert dtn[0, 0] == modal_count
        assert f0.nmodal1() == modal_count
    else:
        assert dtm[0, 0] is None
        assert f0.mode1() is None
        assert dtn[0, 0] == 0
        assert f0.nmodal1() == 0


def test_mode_empty1():
    DT = dt.Frame()
    RZ = DT.mode()
    assert RZ.shape == (0, 0)


def test_mode_empty2():
    DT = dt.Frame(A=[], B=[], C=[], stypes=(dt.int16, dt.float32, dt.str64))
    assert DT.shape == (0, 3)
    RZ = DT.mode()
    assert RZ.names == DT.names
    assert RZ.stypes == DT.stypes
    assert RZ.to_list() == [[None]] * 3



#-------------------------------------------------------------------------------
# Special cases
#-------------------------------------------------------------------------------

@pytest.mark.parametrize(
    "src, view, exp",
    [([-3, 6, 1, 0, 4], slice(2, 5), 0),
     ([-3, 6, 1, 0, 4], [1, 4], 4),
     ([3.5, -182, None, 2, 3], slice(2, None, 2), 3),
     ([3.5, -182, None, 2, 3], [0, 2, 4], 3),
     ([True, True, True, None, False], slice(4), True),
     ([True, True, True, None, False], [0, 3], True)])
def test_stats_on_view(src, view, exp):
    dt0 = dt.Frame(src)
    dt_view = dt0[view, :]
    res = dt_view.min()
    assert res[0, 0] == exp


def test_bad_call():
    f0 = dt.Frame([range(10), range(10)])
    assert f0.ncols == 2
    with pytest.raises(ValueError) as e:
        f0.min1()
    assert "This method can only be applied to a 1-column Frame" in str(e.value)


@pytest.mark.parametrize("st", [dt.int8, dt.int16, dt.int32, dt.int64,
                                dt.float32, dt.float64, dt.str32, dt.str64])
def test_empty_frame(st):
    f0 = dt.Frame([[]], stype=st)
    f1 = dt.Frame([None], stype=st)
    frame_integrity_check(f0)
    frame_integrity_check(f1)
    assert f0.shape == (0, 1)
    assert f1.shape == (1, 1)
    assert f0.stypes == f1.stypes == (st, )
    assert f0.countna1() == 0
    assert f0.nunique1() == 0
    assert f1.countna1() == 1
    assert f1.nunique1() == 0
    assert f1.mode1() is None
    assert f1.nmodal1() == 0


def test_object_column():
    df = dt.Frame([None, nan, 3, "srsh"])
    frame_integrity_check(df)
    assert df.countna1() == 2
    assert df.min1() is None
    assert df.max1() is None
    assert df.mean1() is None
    assert df.sum1() is None
    assert df.sd1() is None
    assert df.mode1() is None
    assert df.nunique1() is None
    assert df.nmodal1() is None


def test_object_column2():
    df = dt.Frame([None, nan, 3, "srsh"])

    f0 = df.countna()
    frame_integrity_check(f0)
    assert f0.stypes == (stype.int64, )
    assert f0[0, 0] == 2

    f1 = df.min()
    frame_integrity_check(f1)
    assert f1.stypes == (stype.obj64, )
    assert f1[0, 0] is None

    f2 = df.max()
    frame_integrity_check(f2)
    assert f2.stypes == (stype.obj64, )
    assert f2[0, 0] is None

    f3 = df.sum()
    frame_integrity_check(f3)
    assert f3.stypes == (stype.float64, )
    assert f3[0, 0] is None

    f4 = df.mean()
    frame_integrity_check(f4)
    assert f4.stypes == (stype.float64, )
    assert f4[0, 0] is None

    f5 = df.sd()
    frame_integrity_check(f5)
    assert f5.stypes == (stype.float64, )
    assert f5[0, 0] is None

    assert df.mode()[0, 0] is None
    assert df.nunique()[0, 0] is None
    assert df.nmodal()[0, 0] is None


def test_issue1953():
    DT0 = dt.Frame([range(5), ['hey']*5])
    RES1 = DT0[-1, :].mode()
    assert RES1.to_list() == [[4], ['hey']]
    RES2 = DT0[::-2, :].mode()
    assert RES2.to_list() == [[0], ['hey']]



#-------------------------------------------------------------------------------
# Statistics on large arrays
#-------------------------------------------------------------------------------

def test_stats_bool_large(numpy):
    n = 12345678
    a = numpy.random.randint(2, size=n, dtype=numpy.bool8)
    dt0 = dt.Frame(a)
    assert dt0.sum().to_list() == [[a.sum()]]
    assert dt0.countna().to_list() == [[0]]
    assert list_equals(dt0.mean().to_list(), [[a.mean()]])
    assert list_equals(dt0.sd().to_list(), [[a.std(ddof=1)]])


def test_stats_int_large(numpy):
    n = 12345678
    a = numpy.random.randint(2**20, size=n, dtype=numpy.int32)
    dt0 = dt.Frame(a)
    assert dt0.min().to_list() == [[a.min()]]
    assert dt0.max().to_list() == [[a.max()]]
    assert dt0.sum().to_list() == [[a.sum()]]
    assert dt0.countna().to_list() == [[0]]
    assert list_equals(dt0.mean().to_list(), [[a.mean()]])
    assert list_equals(dt0.sd().to_list(), [[a.std(ddof=1)]])


def test_stats_float_large(numpy):
    n = 12345678
    a = numpy.random.random(size=n) * 1e6
    dt0 = dt.Frame(a)
    assert dt0.min().to_list() == [[a.min()]]
    assert dt0.max().to_list() == [[a.max()]]
    assert dt0.countna().to_list() == [[0]]
    assert list_equals(dt0.sum().to_list(), [[a.sum()]])
    assert list_equals(dt0.mean().to_list(), [[a.mean()]])
    assert list_equals(dt0.sd().to_list(), [[a.std(ddof=1)]])


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_stats_string_large(seed):
    random.seed(seed)
    n = 1324578
    a = ["%x" % random.getrandbits(11) for _ in range(n)]
    dt0 = dt.Frame(a)
    assert dt0.countna1() == 0
    assert dt0.nunique1() == n_unique(a)
    # assert dt0.mode1() in t_mode(a)[1]
