#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
#-------------------------------------------------------------------------------
# Write integers
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
        numpy.random.randint(-5, 10, size=n, dtype="int32"),
        numpy.random.randint(0, 101, size=n, dtype="int32"),
        numpy.random.randint(-100, 101, size=n, dtype="int32"),
        numpy.random.randint(0, 1001, size=n, dtype="int32"),
        numpy.random.randint(-1000, 1001, size=n, dtype="int32"),
        numpy.random.randint(1000000, size=n, dtype="int32"),
        numpy.random.randint(1000000, size=n, dtype="int32"),
        numpy.random.randint(2**31, size=n, dtype="int32"),
        numpy.random.randint(2**31, size=n, dtype="int32"),
        numpy.random.randint(-2**31, 2**31, size=n, dtype="int32"),
    ]
    dts = [datatable.DataTable(x, colnames=["C%d" % i])
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

def test_tocsv_int32_datatable(benchmark, src, pivot, capsys):
    benchmark.name = "datatable"
    benchmark.group = "tocsv(int32):%d" % src.nrows
    d = src
    assert d.types == ("int",) * 10
    benchmark(lambda: d.to_csv("out.csv"))
    pivot.save(benchmark)
    # Hack: make `capsys` fixture available to pivot
    pivot.capsys = capsys


def test_tocsv_int32_pandas(benchmark, src, pandas, pivot):
    if src.nrows > 10**6: return
    benchmark.name = "pandas"
    benchmark.group = "tocsv(int32):%d" % src.nrows
    p = src.topandas()
    benchmark(lambda: p.to_csv("out.csv"))
    pivot.save(benchmark)


def test_tocsv_int32_fwrite(benchmark, pivot, srcfile, external_test):
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
    benchmark.group = "tocsv(int32):%d" % n
    benchmark._timer = timer
    benchmark.pedantic(run, rounds=5, iterations=1)
    pivot.save(benchmark)
