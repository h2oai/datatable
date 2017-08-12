#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
#-------------------------------------------------------------------------------
# Sort booleans
#-------------------------------------------------------------------------------
import datatable
import pytest



@pytest.fixture(params=[10**4, 10**6, 10**8])
def src(numpy, benchmark, request):
    n = request.param
    benchmark.group = "sort(dtype=bool, n=%d)" % n
    a = numpy.random.randn(n) > 0
    assert a.dtype == numpy.dtype("bool")
    return a


def test_sort_bool_datatable(benchmark, src):
    benchmark.name = "datatable"
    d = datatable.DataTable(src)
    assert d.types == ("bool",)
    benchmark(lambda: d(sort=0))


def test_sort_bool_numpy(benchmark, src):
    benchmark.name = "numpy"
    a = src
    benchmark(lambda: a.sort())


def test_sort_bool_pandas(benchmark, src, pandas):
    benchmark.name = "pandas"
    p = pandas.DataFrame(src)
    benchmark(lambda: p.sort_values(0))


def test_sort_bool_python(benchmark, src):
    benchmark.name = "python"
    p = src.tolist()
    benchmark(lambda: sorted(p))


# def test_sort_bool_h2o(benchmark, src, h2o):
#     benchmark.name = "h2o"
#     h = h2o.H2OFrame(src)
#     benchmark(lambda: h.sort(0).refresh())
