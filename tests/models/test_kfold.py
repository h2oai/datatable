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
from tests import assert_type_error, assert_value_error



def test_kfold_api():
    assert_type_error(lambda: kfold(),
        "Required parameter `nrows` is missing")

    assert_type_error(lambda: kfold(nrows=5),
        "Required parameter `nsplits` is missing")

    assert_type_error(lambda: kfold(nrows=5, nsplits=2, seed=12345),
        "kfold() got an unexpected keyword argument `seed`")

    assert_type_error(lambda: kfold(5, 2),
        "kfold() takes no positional arguments, but 2 were given")

    assert_type_error(lambda: kfold(nrows=5, nsplits=3.3),
        "Argument `nsplits` in kfold() should be an integer")

    assert_type_error(lambda: kfold(nrows=None, nsplits=7),
        "Argument `nrows` in kfold() should be an integer")


def test_bad_args2():
    assert_value_error(lambda: kfold(nrows=-1, nsplits=1),
        "Argument `nrows` in kfold() cannot be negative")

    assert_value_error(lambda: kfold(nrows=3, nsplits=-3),
        "Argument `nsplits` in kfold() cannot be negative")

    assert_value_error(lambda: kfold(nrows=5, nsplits=0),
        "The number of splits cannot be less than two")

    assert_value_error(lambda: kfold(nrows=1, nsplits=2),
        "The number of splits cannot exceed the number of rows")



def test_kfold_simple():
    splits = kfold(nrows=2, nsplits=2)
    assert splits == [(range(1, 2), range(0, 1)),
                      (range(0, 1), range(1, 2))]


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_kfold_k_2(seed):
    random.seed(seed)
    n = 2 + int(random.expovariate(0.01))
    h = n // 2
    splits = kfold(nrows=n, nsplits=2)
    assert splits == [(range(h, n), range(0, h)),
                      (range(0, h), range(h, n))]


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_kfold_k_3(seed):
    random.seed(seed)
    n = 3 + int(random.expovariate(0.01))
    h1 = n // 3
    h2 = 2 * n // 3
    splits = kfold(nrows=n, nsplits=3)
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
    splits = kfold(nrows=n, nsplits=k)
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
