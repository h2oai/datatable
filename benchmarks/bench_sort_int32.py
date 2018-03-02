#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
# Sort integers
#-------------------------------------------------------------------------------
import datatable as dt
import pytest



@pytest.fixture(params=[10**4, 10**6, 10**8])
def src(numpy, benchmark, request):
    n = request.param
    benchmark.group = "sort(dtype=int32, n=%d)" % n
    a = numpy.random.randint(2**31, size=n, dtype="int32")
    assert a.dtype == numpy.dtype("int32")
    return a


def test_sort_int32_datatable(benchmark, src):
    benchmark.name = "datatable"
    d = dt.Frame(src)
    assert d.types == ("int",)
    benchmark(lambda: d.sort(0))


def test_sort_int32_numpy(benchmark, src):
    benchmark.name = "numpy"
    a = src
    benchmark(lambda: a.sort())


def test_sort_int32_pandas(benchmark, src, pandas):
    benchmark.name = "pandas"
    p = pandas.DataFrame(src)
    benchmark(lambda: p.sort_values(0))
