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
from datatable.models import kfold


def assert_type_error(args, msg):
    with pytest.raises(TypeError) as e:
        kfold(*args)
    assert msg in str(e.value)

def assert_value_error(args, msg):
    with pytest.raises(ValueError) as e:
        kfold(*args)
    assert msg in str(e.value)


def test_bad_args1():
    assert_type_error((), "Required parameter `k` is missing")
    assert_type_error((5,), "Required parameter `n` is missing")
    assert_type_error((5, 3.3), "Argument `n` in kfold() should be an integer")
    assert_type_error((None, 7), "Argument `k` in kfold() should be an integer")
    assert_type_error((1, 1, 1), "kfold() takes at most 2 positional arguments")


def test_bad_args2():
    assert_value_error((-1, 1), "Argument `k` in kfold() cannot be negative")
    assert_value_error((3, -3), "Argument `n` in kfold() cannot be negative")
    assert_value_error((0, 5), "The number of splits `k` cannot be less than 2")
    assert_value_error((2, 1), "The number of splits `k` cannot exceed the "
                               "number of rows `n`")

def test_kfold_simple():
    splits = kfold(2, 2)
    assert splits == [(range(1, 2), range(0, 1)),
                      (range(0, 1), range(1, 2))]


def test_kfold_api():
    splits1 = kfold(2, 3)
    splits2 = kfold(2, n=3)
    splits3 = kfold(k=2, n=3)
    assert splits1 == splits2 == splits3


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_kfold_k_2(seed):
    random.seed(seed)
    n = 2 + int(random.expovariate(0.01))
    h = n // 2
    splits = kfold(2, n)
    assert splits == [(range(h, n), range(0, h)),
                      (range(0, h), range(h, n))]


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_kfold_k_3(seed):
    random.seed(seed)
    n = 3 + int(random.expovariate(0.01))
    h1 = n // 3
    h2 = 2 * n // 3
    splits = kfold(3, n)
    assert len(splits) == 3
    assert splits[0] == (range(h1, n), range(0, h1))
    assert splits[1][1] == range(h1, h2)
    assert splits[2] == (range(0, h2), range(h2, n))
    assert isinstance(splits[1][0], dt.Frame)
    assert splits[1][0].nrows == h1 + n - h2
    assert splits[1][0].to_list()[0] == list(range(0, h1)) + list(range(h2, n))


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_kfold_k_any(seed):
    random.seed(seed)
    k = 2 + int(random.expovariate(0.01))
    n = k + int(random.expovariate(0.0001))
    splits = kfold(k, n)
    h1 = n // k
    h2 = n * (k-1) // k
    assert len(splits) == k
    assert splits[0] == (range(h1, n), range(0, h1))
    assert splits[-1] == (range(0, h2), range(h2, n))
    for j, split in enumerate(splits[1:-1], 1):
        hl = j * n // k
        hu = (j + 1) * n // k
        assert split[1] == range(hl, hu)
        assert isinstance(split[0], dt.Frame)
        assert split[0].nrows + len(split[1]) == n
        assert split[0].ncols == 1
        assert split[0].to_list()[0] == list(range(0, hl)) + list(range(hu, n))
