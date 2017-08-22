#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
#-------------------------------------------------------------------------------
# Sort booleans
#-------------------------------------------------------------------------------
import datatable
import pytest
import os
import tempfile


@pytest.fixture(params=[10**i for i in range(3, 9)], scope="module")
def src(numpy, request):
    n = request.param
    data = numpy.random.randn(n) > 0
    mask = numpy.random.randn(n) > 1
    assert data.dtype == numpy.dtype("bool")
    assert mask.dtype == numpy.dtype("bool")
    return numpy.ma.array(data, mask=mask)

@pytest.fixture(scope="module")
def srcfile(pandas, src):
    fd, name = tempfile.mkstemp(suffix=".csv",
                                prefix="dtbench%d_" % len(src))
    with os.fdopen(fd, "wt") as f:
        pdf = pandas.DataFrame(src, columns=["C1"])
        pdf.to_csv(f, index=False, na_rep="--")
    yield (name, len(src))
    os.remove(name)


#-------------------------------------------------------------------------------

def test_datatable(benchmark, pivot, srcfile, capsys):
    filename, n = srcfile
    d = datatable.fread(filename, header=True, na_strings=["--"])
    assert d.types == ("bool",)
    assert d.shape == (n, 1)
    benchmark.name = "datatable"
    benchmark.group = "sort(bool):%d" % n
    benchmark(lambda: d.sort(0))
    pivot.save(benchmark)
    # Hack: make `capsys` fixture available to pivot
    pivot.capsys = capsys


def test_numpy(benchmark, pivot, src, numpy):
    a = src
    assert a.dtype == numpy.dtype("bool")
    benchmark.name = "numpy"
    benchmark.group = "sort(bool):%d" % len(src)
    benchmark(lambda: numpy.sort(a))
    pivot.save(benchmark)


def test_pandas(benchmark, pivot, src, pandas):
    p = pandas.DataFrame(src)
    assert p.shape == (len(src), 1)
    benchmark.name = "pandas"
    benchmark.group = "sort(bool):%d" % len(src)
    if len(src) > 10**6:
        benchmark._min_rounds = 1
    benchmark(lambda: p.sort_values(0))
    pivot.save(benchmark)


def test_rdatatable(benchmark, pivot, srcfile, external_test):
    filename, n = srcfile
    iters = max(int(1000000 / n), 1)
    run, timer = external_test([
        "R", "-e",
        "require(data.table);"
        "DT=fread('%s',header=T,na.strings='--');"
        "stopifnot(class(DT$C1) == 'logical');"
        "stopifnot(nrow(DT) == %d);"
        "bb=%d;"
        "tt=system.time(for(b in 1:bb){DC=copy(DT);setkey(DC,C1)});"
        "te=as.numeric(tt['elapsed'])/bb;"
        "cat(paste('Time taken:', te, '\\\\n'));"
        % (filename, n, iters)
    ])

    benchmark.name = "R data.table"
    benchmark.group = "sort(bool):%d" % n
    benchmark._timer = timer
    benchmark.pedantic(run, rounds=5, iterations=1)
    pivot.save(benchmark)



# def test_h2o(benchmark, pivot, srcfile, h2o):
#     filename, n = srcfile
#     if n > 10**7:
#         # H2O throws an Out-of-Memory error
#         return
#     h = h2o.import_file(filename, header=1, na_strings=["--"])
#     assert h.shape == (n, 1)
#     assert h.types == {"C1": "enum"}
#     if n > 10**6:
#         benchmark._min_rounds = 1
#     benchmark.name = "h2o"
#     benchmark.group = "sort(bool):%d" % n
#     benchmark(lambda: h.sort(0).refresh())
#     pivot.save(benchmark)
