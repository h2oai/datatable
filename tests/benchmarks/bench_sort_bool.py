#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
#-------------------------------------------------------------------------------
# Sort booleans
#-------------------------------------------------------------------------------
import datatable
import pytest
import os
import tempfile
import subprocess
import re



@pytest.fixture(params=[10**4, 10**6, 10**8])
def src(numpy, benchmark, request):
    n = request.param
    benchmark.group = "sort(dtype=bool, n=%d)" % n
    a = numpy.random.randn(n) > 0
    b = numpy.random.randn(n) > 1
    assert a.dtype == numpy.dtype("bool")
    assert b.dtype == numpy.dtype("bool")
    return numpy.ma.array(a, mask=b)

@pytest.fixture()
def srctxt(numpy, src):
    fd, name = tempfile.mkstemp(suffix="csv",
                                prefix="dtbench%d_" % src.shape[0])
    f = os.fdopen(fd, "wb")
    numpy.savetxt(f, src, fmt="%r")
    f.close()
    return name

class ManualTimer(object):

    def __init__(self):
        self._elapsed_time = 0

    def __call__(self):
        t = self._elapsed_time
        self._elapsed_time = 0
        return t

    def set(self, t):
        self._elapsed_time += t



def test_datatable(benchmark, src):
    benchmark.name = "datatable"
    d = datatable.DataTable(src)
    assert d.types == ("bool",)
    benchmark(lambda: d(sort=0))


def test_numpy(benchmark, src, numpy):
    benchmark.name = "numpy"
    a = src
    benchmark(lambda: numpy.sort(a))


def test_pandas(benchmark, src, pandas):
    benchmark.name = "pandas"
    p = pandas.DataFrame(src)
    benchmark(lambda: p.sort_values(0))


def test_python(benchmark, src):
    benchmark.name = "python"
    p = src.tolist()
    benchmark(lambda: sorted(p))


def test_rdatatable(benchmark, srctxt):
    mm = re.search(r"dtbench(\d+)_", srctxt)
    n = int(mm.group(1))
    iters = max(int(1000000 / n), 1)
    timer = ManualTimer()
    script = ("require(data.table);"
              "DT=fread('%s',header=F);"
              "bb=%d;"
              "tt=system.time(for(b in 1:bb){DC=copy(DT);setkey(DC,V1)});"
              "te=as.numeric(tt['elapsed'])/bb;"
              "cat(paste('Time taken:', te, '\\\\n'));"
              % (srctxt, iters))

    def run():
        out = subprocess.check_output(["R", "-e", script]).decode()
        mm = re.search(r"Time taken: (\d*(?:\.\d*)?(?:[eE][-+]?\d+)?)", out)
        if mm:
            timer.set(float(mm.group(1)))
        else:
            raise RuntimeError(out)

    benchmark.name = "R data.table"
    benchmark._timer = timer
    benchmark.pedantic(run, rounds=5, iterations=1)


# def test_h2o(benchmark, src, h2o):
#     benchmark.name = "h2o"
#     h = h2o.H2OFrame(src)
#     benchmark(lambda: h.sort(0).refresh())
