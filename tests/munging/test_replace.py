#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#-------------------------------------------------------------------------------
import datatable as dt
import pytest
import random
from math import inf, nan
from tests import list_equals


#-------------------------------------------------------------------------------
# Replacing in int columns
#-------------------------------------------------------------------------------

def test_replace_int_simple():
    df = dt.Frame(range(5))
    df.replace(0, -1)
    df.internal.check()
    assert df.topython() == [[-1, 1, 2, 3, 4]]


@pytest.mark.parametrize("st", dt.ltype.int.stypes)
def test_replace_ints(st):
    df = dt.Frame(A=[1, 2, 3, 5, 9, 0], B=[0, 2, 1, 3, 2, 1], stype=st)
    df.replace({0: 100, 1: -99, 2: 10})
    df.internal.check()
    assert df.stypes == (st, st)
    assert df["A"].topython() == [[-99, 10, 3, 5, 9, 100]]
    assert df["B"].topython() == [[100, 10, -99, 3, 10, -99]]


def test_replace_int_with_upcast():
    df = dt.Frame(range(10), stype=dt.int8)
    assert df.stypes == (dt.stype.int8,)
    df.replace(5, 1000)
    df.internal.check()
    assert df.stypes == (dt.stype.int32,)
    assert df.topython() == [[0, 1, 2, 3, 4, 1000, 6, 7, 8, 9]]
    df.replace(9, 10**10)
    assert df.stypes == (dt.stype.int64,)
    assert df.topython() == [[0, 1, 2, 3, 4, 1000, 6, 7, 8, 10**10]]


@pytest.mark.parametrize("st", dt.ltype.int.stypes)
def test_replace_into_int(st):
    df = dt.Frame(A=[0, 5, 9, 0, 3, 1], stype=st)
    df.replace([0, 1], None)
    assert df.topython() == [[None, 5, 9, None, 3, None]]


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_replace_large(seed):
    random.seed(seed)
    st = random.choice(dt.ltype.int.stypes)
    n = int(100 + random.expovariate(0.00005))
    src = [random.randint(-10, 100) for i in range(n)]
    df = dt.Frame({"A": src}, stype=st)
    df.internal.check()
    assert df.sum1() == sum(src)
    replacements = {25: -1, 77: -2, 0: 1, 1: 0}
    df.replace(replacements)
    for i in range(n):
        src[i] = replacements.get(src[i], src[i])
    df.internal.check()
    assert df.topython() == [src]
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
    df.internal.check()
    assert df.stypes == (st, st)
    res = [[1.1, 0.0, 5e10, -1.0, -2.0],
           [-inf, -2.0, -2.0, 3.99, 7.0]]
    pydt = df.topython()
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
    df.internal.check()
    assert df.topython() == [[1.0, None, None]] * 2
    assert df.stypes == (dt.float32, dt.float64)


def test_replace_float_with_upcast():
    df = dt.Frame([1.5, 2.0, 3.5, 4.0], stype=dt.float32)
    assert df.stypes == (dt.float32,)
    df.replace(2.0, 1.5e100)
    assert df.stypes == (dt.float64,)
    assert df.topython() == [[1.5, 1.5e100, 3.5, 4.0]]



#-------------------------------------------------------------------------------
# Mixed column types
#-------------------------------------------------------------------------------

def test_replace_nas():
    df = dt.Frame([[1, None, 5, 10], [2.7, nan, None, 1e5]])
    df.replace(None, [77, 9.999])
    df.internal.check()
    assert df.topython() == [[1, 77, 5, 10], [2.7, 9.999, 9.999, 1e5]]


def test_replace_nas2():
    # Check that `nan` can be used as a replacement target too
    df = dt.Frame([1.0, None, 2.2, nan, -nan])
    df.replace(nan, 0.0)
    df.internal.check()
    assert df.topython() == [[1.0, 0.0, 2.2, 0.0, 0.0]]



#-------------------------------------------------------------------------------
# Test bad arguments
#-------------------------------------------------------------------------------

def test_replace_bad1():
    with pytest.raises(TypeError) as e:
        df0 = dt.Frame(A=range(10))
        df0.replace({0: 500}, 17)
    assert ("When the first argument to Frame.replace() is a dict, there "
            "should be no second argument" == str(e.value))


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


def test_replace_nonunique():
    with pytest.raises(ValueError) as e:
        df0 = dt.Frame(A=range(10))
        df0.replace([0, 9, 11, 0], -1)
    assert ("Replacement target `0` was specified more than once in "
            "Frame.replace()" == str(e.value))
