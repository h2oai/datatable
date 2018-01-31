#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
# Write booleans
#-------------------------------------------------------------------------------
import datatable
import pytest
import tempfile
import os


@pytest.fixture(params=[1000, 10**4, 10**5, 10**6, 10**7, 10**8],
                scope="module")
def src(numpy, request):
    n = request.param
    cols = [
        numpy.random.randn(n),
        numpy.random.randn(n),
        numpy.random.randn(n) * (numpy.random.randn(n) > 0),
        numpy.random.randn(n) * 10,
        numpy.random.randn(n) * 10,
        numpy.random.randn(n) * 100,
        numpy.random.randn(n) * 1000000,
        numpy.random.randn(n) * 10.0**numpy.random.randint(-308, 308, size=n),
        numpy.random.randn(n) * 10.0**numpy.random.randint(-308, 308, size=n),
        numpy.random.randn(n) * 10.0**numpy.random.randint(-10, 10, size=n),
    ]
    dts = [datatable.DataTable(x, names=["C%d" % i])
           for i, x in enumerate(cols)]
    return datatable.DataTable().cbind(*dts)

@pytest.fixture(scope="module")
def srcfile(pandas, src):
    fd, name = tempfile.mkstemp(suffix=".csv",
                                prefix="dtbench%d_" % src.nrows)
    src.to_csv(name)
    yield (name, src.nrows)
    os.remove(name)


#-------------------------------------------------------------------------------

def test_tocsv_double_datatable(benchmark, src, pivot, capsys):
    benchmark.name = "datatable"
    benchmark.group = "tocsv(double):%d" % src.nrows
    d = src
    assert d.stypes == (datatable.stype.float64,) * 10
    benchmark(lambda: d.to_csv("out.csv"))
    pivot.save(benchmark)
    # Hack: make `capsys` fixture available to pivot
    pivot.capsys = capsys


def test_tocsv_double_pandas(benchmark, src, pandas, pivot):
    if src.nrows > 10**6: return
    benchmark.name = "pandas"
    benchmark.group = "tocsv(double):%d" % src.nrows
    p = src.topandas()
    benchmark(lambda: p.to_csv("out.csv"))
    pivot.save(benchmark)


def test_tocsv_double_fwrite(benchmark, pivot, srcfile, external_test):
    filename, n = srcfile
    iters = max(int(1000000 / n), 1)
    run, timer = external_test([
        "R", "-e",
        "require(data.table);"
        "DT=fread('%s',header=T);"
        "stopifnot(nrow(DT) == %d);"
        "bb=%d;"
        "tt=system.time(for(b in 1:bb){fwrite(DT,'out.csv')});"
        "te=as.numeric(tt['elapsed'])/bb;"
        "cat(paste('Time taken:', te, '\\\\n'));"
        % (filename, n, iters)
    ])
    benchmark.name = "fwrite"
    benchmark.group = "tocsv(double):%d" % n
    benchmark._timer = timer
    benchmark.pedantic(run, rounds=5, iterations=1)
    pivot.save(benchmark)
