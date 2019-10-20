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
import sys
from datatable import f
from datatable.internal import frame_integrity_check
from math import inf, nan
from tests import list_equals


#-------------------------------------------------------------------------------
# Frame.replace() signature
#-------------------------------------------------------------------------------

def test_replace_scalar_scalar():
    df = dt.Frame([1, 2, 3])
    df.replace(1, 5)
    assert df.to_list() == [[5, 2, 3]]


def test_replace_list_scalar():
    df = dt.Frame([1, 2, 3])
    df.replace([1, 2, 7], 5)
    assert df.to_list() == [[5, 5, 3]]


def test_replace_none_list():
    df = dt.Frame([1, 2, 3, None])
    df.replace(None, [0, 0.0, ""])
    assert df.to_list() == [[1, 2, 3, 0]]


def test_replace_list_list():
    df = dt.Frame([1, 2, 3])
    df.replace([1, 2, 7], [6, 2, 5])
    assert df.to_list() == [[6, 2, 3]]


def test_replace_emptylist():
    df = dt.Frame([1, 2, 3])
    df.replace([], 0)
    assert df.to_list() == [[1, 2, 3]]


def test_replace_dict():
    df = dt.Frame([1, 2, 3])
    df.replace({3: 1, 1: 3})
    assert df.to_list() == [[3, 2, 1]]




#-------------------------------------------------------------------------------
# Replacing in bool columns
#-------------------------------------------------------------------------------

def test_replace_bool_simple():
    df = dt.Frame([[True, False, None], [True] * 3, [False] * 3])
    df.replace({True: False, False: True})
    frame_integrity_check(df)
    assert df.stypes == (dt.bool8,) * 3
    assert df.to_list() == [[False, True, None], [False] * 3, [True] * 3]


def test_replace_bool_na():
    df = dt.Frame([True, False, None])
    df.replace(None, False)
    frame_integrity_check(df)
    assert df.to_list() == [[True, False, False]]




#-------------------------------------------------------------------------------
# Replacing in int columns
#-------------------------------------------------------------------------------

def test_replace_int_simple():
    df = dt.Frame(range(5))
    df.replace(0, -1)
    frame_integrity_check(df)
    assert df.to_list() == [[-1, 1, 2, 3, 4]]


@pytest.mark.parametrize("st", dt.ltype.int.stypes)
def test_replace_ints(st):
    df = dt.Frame(A=[1, 2, 3, 5, 9, 0], B=[0, 2, 1, 3, 2, 1], stype=st)
    df.replace({0: 100, 1: -99, 2: 10})
    frame_integrity_check(df)
    assert df.stypes == (st, st)
    assert df[:, "A"].to_list() == [[-99, 10, 3, 5, 9, 100]]
    assert df[:, "B"].to_list() == [[100, 10, -99, 3, 10, -99]]


def test_replace_int_with_upcast():
    df = dt.Frame(range(10), stype=dt.int8)
    assert df.stypes == (dt.stype.int8,)
    df.replace(5, 1000)
    frame_integrity_check(df)
    assert df.stypes == (dt.stype.int32,)
    assert df.to_list() == [[0, 1, 2, 3, 4, 1000, 6, 7, 8, 9]]
    df.replace(9, 10**10)
    assert df.stypes == (dt.stype.int64,)
    assert df.to_list() == [[0, 1, 2, 3, 4, 1000, 6, 7, 8, 10**10]]


@pytest.mark.parametrize("st", dt.ltype.int.stypes)
def test_replace_into_int(st):
    df = dt.Frame(A=[0, 5, 9, 0, 3, 1], stype=st)
    df.replace([0, 1], None)
    assert df.to_list() == [[None, 5, 9, None, 3, None]]


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_replace_large(seed):
    random.seed(seed)
    st = random.choice(dt.ltype.int.stypes)
    n = int(100 + random.expovariate(0.00005))
    src = [random.randint(-10, 100) for i in range(n)]
    df = dt.Frame({"A": src}, stype=st)
    frame_integrity_check(df)
    assert df.sum1() == sum(src)
    replacements = {25: -1, 77: -2, 0: 1, 1: 0}
    df.replace(replacements)
    for i in range(n):
        src[i] = replacements.get(src[i], src[i])
    frame_integrity_check(df)
    assert df.to_list() == [src]
    assert df.stypes == (st,)
    assert df.names == ("A",)
    assert df.sum1() == sum(src)




#-------------------------------------------------------------------------------
# Replacing in float columns
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("st", [dt.stype.float32, dt.stype.float64])
def test_replace_floats(st):
    df = dt.Frame([[1.1, 2.2, 5e10, inf, nan], [-inf, nan, None, 3.99, 7]],
                  stype=st, names=["A", "B"])
    df.replace([2.2, inf, None], [0.0, -1.0, -2.0])
    frame_integrity_check(df)
    assert df.stypes == (st, st)
    res = [[1.1, 0.0, 5e10, -1.0, -2.0],
           [-inf, -2.0, -2.0, 3.99, 7.0]]
    pydt = df.to_list()
    if st == dt.stype.float64:
        assert pydt == res
    else:
        assert list_equals(pydt[0], res[0])
        assert list_equals(pydt[1], res[1])


def test_replace_infs():
    df = dt.Frame([[1.0, inf, -inf]] * 2,
                  stypes=[dt.float32, dt.float64], names=["A", "B"])
    assert df.stypes == (dt.float32, dt.float64)
    df.replace([inf, -inf], None)
    frame_integrity_check(df)
    assert df.to_list() == [[1.0, None, None]] * 2
    assert df.stypes == (dt.float32, dt.float64)


def test_replace_infs2():
    df = dt.Frame([[1.0, inf, -inf]] * 2,
                  stypes=[dt.float32, dt.float64], names=["A", "B"])
    assert df.stypes == (dt.float32, dt.float64)
    df.replace(inf, None)
    frame_integrity_check(df)
    assert df.to_list() == [[1.0, None, -inf]] * 2
    assert df.stypes == (dt.float32, dt.float64)
    df.replace(-inf, 3.5)
    frame_integrity_check(df)
    assert df.to_list() == [[1.0, None, 3.5]] * 2
    assert df.stypes == (dt.float32, dt.float64)


def test_replace_almost_inf():
    maxfloat = sys.float_info.max
    df = dt.Frame([10.0, maxfloat, -maxfloat, inf, -inf, None])
    df.replace(maxfloat, -maxfloat)
    frame_integrity_check(df)
    assert df.to_list() == [[10.0, -maxfloat, -maxfloat, inf, -inf, None]]
    df.replace(-maxfloat, 0.0)
    assert df.to_list() == [[10.0, 0.0, 0.0, inf, -inf, None]]



def test_replace_float_with_upcast():
    df = dt.Frame([1.5, 2.0, 3.5, 4.0], stype=dt.float32)
    assert df.stypes == (dt.float32,)
    df.replace(2.0, 1.5e100)
    assert df.stypes == (dt.float64,)
    assert df.to_list() == [[1.5, 1.5e100, 3.5, 4.0]]




#-------------------------------------------------------------------------------
# Replacing in string columns
#-------------------------------------------------------------------------------

def test_replace_str_simple():
    df = dt.Frame(["foo", "bar", "buzz"])
    df.replace("bar", "oomph")
    assert df.to_list() == [["foo", "oomph", "buzz"]]


def test_replace_str_none():
    df = dt.Frame(["A", "BC", None, "DEF", None, "G"])
    df.replace(["A", None], [None, "??"])
    assert df.to_list() == [[None, "BC", "??", "DEF", "??", "G"]]


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_replace_str_large(seed):
    random.seed(seed)
    nums = ["one", "two", "three", "four", "five"]
    src = [random.choice(nums) for _ in range(10000)]
    df = dt.Frame(src)
    df.replace(["two", "five"], ["2", "1+1+1+1+1"])
    frame_integrity_check(df)
    for i, word in enumerate(src):
        if word == "two":
            src[i] = "2"
        elif word == "five":
            src[i] = "1+1+1+1+1"
    assert df.to_list() == [src]


def test_replace_str_huge():
    # Issue #1694: the output column is too large to fit into str32, so it
    # must be converted into str64.
    n = 10000000
    src = ["a"] * n
    src[-1] = None
    src[-100] = None
    src[-10000] = None
    src[-1000000] = None
    DT = dt.Frame(src)
    assert DT.stypes == (dt.str32,)
    DT.replace("a", "A" * 250)
    frame_integrity_check(DT)
    assert DT.stypes == (dt.str64,)
    assert DT.shape == (n, 1)
    assert DT[-2, 0] == "A" * 250
    assert DT[-1, 0] is None
    assert DT[-100, 0] is None


def test_replace_str64():
    Y = dt.Frame([["BLSD", "RY", "IO OUSEVOUY", "@"], [3, 4, 1, 2]],
                 names=["A", "B"], stypes=["str64", "int32"])
    Y[f.B < 100, f.A] = "*"
    frame_integrity_check(Y)
    assert Y.stypes == (dt.str64, dt.int32)
    assert Y.to_list() == [["*"] * 4, [3, 4, 1, 2]]


def test_replace_str64_2():
    Y = dt.Frame([["a"], [0]], names=["A", "B"], stypes=["str64", "int32"])
    Y[f.B < 100, f.A] = "hello"
    frame_integrity_check(Y)
    assert Y.stypes == (dt.str64, dt.int32)
    assert Y.to_list() == [["hello"], [0]]




#-------------------------------------------------------------------------------
# Misc
#-------------------------------------------------------------------------------

def test_replace_nothing():
    df = dt.Frame(range(5))
    df.replace([], [])
    df.replace({})
    assert df.to_list() == [list(range(5))]


def test_replace_nas():
    df = dt.Frame([[1, None, 5, 10],
                   [2.7, nan, None, 1e5],
                   [True, False, None, None]])
    df.replace(None, [77, 9.999, True])
    frame_integrity_check(df)
    assert df.to_list() == [[1, 77, 5, 10],
                            [2.7, 9.999, 9.999, 1e5],
                            [True, False, True, True]]


@pytest.mark.parametrize("st", [dt.float32, dt.float64])
def test_replace_nas2(st):
    # Check that `nan` can be used as a replacement target too
    df = dt.Frame([1.0, None, 2.5, nan, -nan], stype=st)
    assert df.stypes == (st,)
    df.replace(nan, 0.0)
    frame_integrity_check(df)
    assert df.stypes == (st,)
    assert df.to_list() == [[1.0, 0.0, 2.5, 0.0, 0.0]]


@pytest.mark.parametrize("nn", [0, 1, 2, 3, 5, 8])
@pytest.mark.parametrize("st", dt.ltype.int.stypes + dt.ltype.real.stypes)
def test_replace_multiple(nn, st):
    src = list(range(100)) + [None]
    df = dt.Frame(src, stype=st)
    assert df.stypes == (st,)
    replacements = {i: i * 2 for i in range(1, nn + 1)}
    res = src
    for i in range(1, nn + 1):
        res[i] = i * 2
    if nn == 0:
        replacements[None] = -1
        res[-1] = -1
    if st.ltype == dt.ltype.real:
        res = [float(x) for x in res[:-1]] + [None]
        if nn != 0:
            replacements = {float(k): float(v) for k, v in replacements.items()}
    df.replace(replacements)
    frame_integrity_check(df)
    assert df.stypes == (st,)
    assert df.to_list()[0] == res


def test_replace_in_copy():
    df1 = dt.Frame([[1, 2, 3], [5.5, 6.6, 7.7], ["A", "B", "C"]])
    df2 = df1.copy()
    df2.replace({3: 9, 5.5: 0.0, "B": "-"})
    assert df1.to_list() == [[1, 2, 3], [5.5, 6.6, 7.7], ["A", "B", "C"]]
    assert df2.to_list() == [[1, 2, 9], [0.0, 6.6, 7.7], ["A", "-", "C"]]


def test_replace_do_nothing():
    # Check that the case when there is nothing to replace works too
    df1 = dt.Frame([[1, 2], [3, 4], [5.0, 6.6], [-inf, inf]])
    df1.replace([-99, -inf, inf], None)
    frame_integrity_check(df1)
    assert df1.to_list() == [[1, 2], [3, 4], [5.0, 6.6], [None, None]]


def test_replace_do_nothing2(numpy):
    np = numpy
    a = numpy.array([[1, 2, 3, np.inf], [1, -np.inf, 4, 5]])
    df = dt.Frame(a)
    df.replace([-np.inf, np.inf], [np.nan, np.nan])
    frame_integrity_check(df)
    assert df.to_list() == [[1., 1.], [2., None], [3., 4.], [None, 5.]]


def test_replace_in_view1():
    DT = dt.Frame([str(x) for x in range(10)])[::2, :]
    assert DT.to_list()[0] == ['0', '2', '4', '6', '8']
    DT.replace('6', "boo!")
    assert DT.to_list()[0] == ['0', '2', '4', 'boo!', '8']


def test_replace_in_view2():
    names = ["Renegade", "Barack", "Renaissance", "Michelle",
             "Radiance", "Malia", "Rosebud", "Sasha"]
    DT = dt.Frame(N=names)
    DT2 = DT[::2, :]
    DT3 = DT[1::2, :]
    assert DT2.to_list()[0] == names[::2]
    assert DT3.to_list()[0] == names[1::2]
    DT2.replace("Renaissance", "Revival")
    assert DT2.to_list()[0] == ["Renegade", "Revival", "Radiance", "Rosebud"]
    DT2.replace("Radiance", "Brilliance")
    assert DT2.to_list()[0] == ["Renegade", "Revival", "Brilliance", "Rosebud"]
    DT3.replace("Sasha", "Natasha")
    assert DT3.to_list()[0] == ["Barack", "Michelle", "Malia", "Natasha"]
    DT3.replace("Malia", "Malia Ann")
    assert DT3.to_list()[0] == ["Barack", "Michelle", "Malia Ann", "Natasha"]
    assert DT.to_list() == [names]


#-------------------------------------------------------------------------------
# Replacing in a keyed frame
#-------------------------------------------------------------------------------

def test_replace_in_keyed_frame():
    DT = dt.Frame(A=range(100))
    DT.key = "A"
    with pytest.raises(ValueError, match = "Cannot replace values in a keyed frame"):
        DT.replace(1, 101)
    assert DT.key == ("A",)
    frame_integrity_check(DT)
    assert DT.to_list() == [list(range(100))]


#-------------------------------------------------------------------------------
# Test bad arguments
#-------------------------------------------------------------------------------

def test_replace_bad1():
    with pytest.raises(TypeError) as e:
        df0 = dt.Frame(A=range(10))
        df0.replace({0: 500}, 17)
    assert ("When the first argument to Frame.replace() is a dictionary, there "
            "should be no other arguments" == str(e.value))


def test_replace_bad2():
    with pytest.raises(ValueError) as e:
        df0 = dt.Frame(A=range(10))
        df0.replace([0, 5, 7], [2, 6])
    assert ("The `replace_what` and `replace_with` lists in Frame.replace() "
            "have different lengths: 3 and 2 respectively" == str(e.value))


def test_replace_bad3():
    with pytest.raises(TypeError) as e:
        df0 = dt.Frame(A=range(10))
        df0.replace({0: inf})
    assert ("Cannot replace integer value `0` with a value of type "
            "<class 'float'>" == str(e.value))


def test_replace_bad4():
    with pytest.raises(TypeError) as e:
        df0 = dt.Frame(B=[0.1] * 10)
        df0.replace({inf: 0})
    assert ("Cannot replace float value `inf` with a value of type "
            "<class 'int'>" == str(e.value))


def test_replace_bad5():
    with pytest.raises(TypeError) as e:
        df0 = dt.Frame([True, False])
        df0.replace({False: 0})
    assert ("Cannot replace boolean value `False` with a value of type "
            "<class 'int'>" == str(e.value))


def test_replace_nonunique():
    with pytest.raises(ValueError) as e:
        df0 = dt.Frame(A=range(10))
        df0.replace([0, 9, 11, 0], -1)
    assert ("Replacement target `0` was specified more than once in "
            "Frame.replace()" == str(e.value))


def test_issue1525_1():
    with pytest.raises(TypeError) as e:
        df0 = dt.Frame(["foo"])
        df0.replace()
    assert ("Missing the required argument `replace_what` in method "
            "Frame.replace()" == str(e.value))


def test_issue1525_2():
    with pytest.raises(TypeError) as e:
        df0 = dt.Frame(["foo"])
        df0.replace("foo")
    assert ("Missing the required argument `replace_with` in method "
            "Frame.replace()" == str(e.value))
