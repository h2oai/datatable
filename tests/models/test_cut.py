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
#
# Test cut()
#
#-------------------------------------------------------------------------------
import pandas as pd
import pytest
import random
from datatable import dt, stype
from datatable.models import cut
from tests import assert_equals


def test_cut_error_noargs():
    msg = "Required parameter frame is missing"
    with pytest.raises(ValueError, match=msg):
        cut()


def test_cut_error_wrong_column_types():
    DT = dt.Frame([[1, 0], ["1", "0"]])
    msg = "All frame columns must be numeric, instead column 1 has stype str32"
    with pytest.raises(TypeError, match=msg):
        cut(DT)


def test_cut_error_zero_bins():
    msg = r"Argument bins in cut\(\) should be positive: 0"
    DT = dt.Frame(range(10))
    with pytest.raises(ValueError, match=msg):
        cut(DT, 0)


def test_cut_error_negative_bins():
    msg = "Integer value cannot be negative"
    DT = dt.Frame([[3, 1, 4], [1, 5, 9]])
    with pytest.raises(ValueError, match=msg):
        cut(DT, [10, -1])


def test_cut_error_inconsistent_bins():
    msg = ("When bins is a list or a tuple, its length must be the same as "
    	   "the number of columns in the frame, i.e. 2, instead got: 1")
    DT = dt.Frame([[3, 1, 4], [1, 5, 9]])
    with pytest.raises(ValueError, match=msg):
        cut(DT, [10])


def test_cut_empty_frame():
	DT_cut = cut(dt.Frame())
	assert_equals(DT_cut, dt.Frame())


def test_cut_trivial():
	DT = dt.Frame({"trivial": range(10)})
	DT_cut = cut(DT)
	assert_equals(DT, DT_cut)


def test_cut_one_row():
	DT = dt.Frame([[True], [404], [3.1415926], [None]])
	DT_cut = cut(DT)
	assert(DT_cut.to_list() == [[4], [4], [4], [None]])


def test_cut_simple():
	DT = dt.Frame({
	       "bool": [True, None, False, False, True, None],
	       "int": [3, None, 4, 1, 5, 4],
	       "float": [None, 1.4, 4.1, 1.5, 5.9, 1.4]
	     })
	DT_ref = dt.Frame({
       "bool": [1, None, 0, 0, 1, None],
       "int": [1, None, 2, 0, 2, 2],
       "float": [None, 0, 5, 0, 9, 0]
     }, stypes = [stype.int32, stype.int32, stype.int32])

	DT_cut_list = cut(DT, bins = [2, 3, 10])
	DT_cut_tuple = cut(DT, bins = (2, 3, 10))
	assert_equals(DT_ref, DT_cut_list)
	assert_equals(DT_ref, DT_cut_tuple)


@pytest.mark.xfail(reason="Pandas inconsistency")
def test_cut_pandas_failure():
	nbins = 42
	data = [-97, 0, 97]

	DT = dt.Frame(data)
	DT_cut = cut(DT, nbins)
	PD_cut = pd.cut(data, nbins, labels = False)
	assert(DT_cut.to_list() == [list(PD_cut)])


@pytest.mark.parametrize("seed", [random.getrandbits(32) for _ in range(10)])
def test_cut_random(seed):
	random.seed(seed)
	max_size = 100
	max_value = 1000

	n = random.randint(1, max_size)

	bins = [random.randint(1, max_size) for _ in range(3)]
	data = [[], [], []]

	for _ in range(n):
		data[0].append(random.randint(0, 1))
		data[1].append(random.randint(-max_value, max_value))
		data[2].append(random.random() * 2 * max_value - max_value)

	DT = dt.Frame(data, stypes = [stype.bool8, stype.int32, stype.float64])
	DT_cut = cut(DT, bins)

	PD = [pd.cut(data[i], bins[i], labels=False) for i in range(3)]

	assert([list(PD[i]) for i in range(3)] == DT_cut.to_list())
