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
import py

plural = datatable.utils.misc.plural_form

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


class ManualTimer(object):
    def __init__(self):
        self._elapsed_time = 0

    def __call__(self):
        t = self._elapsed_time
        self._elapsed_time = 0
        return t

    def set(self, t):
        self._elapsed_time += t

#-------------------------------------------------------------------------------

class PivotBenchmark(object):
    def __init__(self):
        self.tbl = {}

    def save(self, benchmark):
        time = benchmark.stats.stats.as_dict()["mean"]
        name = benchmark.name
        group, nstr = benchmark.group.split(":")
        n = int(nstr)
        if group not in self.tbl:
            self.tbl[group] = {}
        if name not in self.tbl[group]:
            self.tbl[group][name] = {}
        self.tbl[group][name][n] = time

    def display(self):
        tw = py.io.TerminalWriter()
        for grp, table in self.tbl.items():
            ns = set()
            for row in table.values():
                ns |= set(row.keys())
            ns = sorted(ns)

            best = [1e9] * len(ns)
            worst = [-1e9] * len(ns)
            for name, row in table.items():
                for i, n in enumerate(ns):
                    v = row.get(n)
                    if v is not None:
                        best[i] = min(best[i], v)
                        worst[i] = max(worst[i], v)

            colnames = ["n=" + plural(n) for n in ns]
            colwidths = [15] + [max(len(name), 14) for name in colnames]
            tblwidth = sum(colwidths) + len(ns) * 2
            halfwidth = (tblwidth - 2 - len(grp)) // 2
            print("\n\n")
            # write header
            tw.line("-" * halfwidth + " " + grp + " " +
                    "-" * (tblwidth - 2 - len(grp) - halfwidth), yellow=True)
            tw.write("%-*s" % (colwidths[0], "(time in ms)"))
            for i in range(len(ns)):
                tw.write("  %*s" % (colwidths[i + 1], colnames[i]))
            tw.line()
            tw.line("-" * tblwidth, yellow=True)
            # write table body
            for name in sorted(table.keys(),
                               key=lambda n: table[n].get(ns[-1], 1e10)):
                tw.write("%-15s" % name)
                for i, n in enumerate(ns):
                    v = table[name].get(n)
                    f = self.format(v, colwidths[i + 1], best[i], worst[i])
                    tw.write("  " + f, bold=True, green=(best[i] == v))
                tw.line()
            tw.line("-" * tblwidth, yellow=True)

    def format(self, value, width, minval, maxval):
        if value is None:
            return " " * (width + 2)
        x = value * 1e3  # cast to ms
        minx = minval * 1e3
        maxx = maxval * 1e3
        rel = x / minx
        if rel < 10:
            ratio = "(%.2f)" % rel
        elif rel < 100:
            ratio = "(%.1f)" % rel
        elif rel < 1000:
            ratio = "(%d.)" % int(rel + 0.5)
        else:
            ratio = "(>999)"
        if minx < 0.1 or minx * maxx < 100:
            return "%*.3f %s" % (width - 7, x, ratio)
        elif minx < 1 or minx * maxx < 10000:
            return "%*.2f %s" % (width - 7, x, ratio)
        elif minx < 10 or minx * maxx < 1000000:
            return "%*.1f %s" % (width - 7, x, ratio)
        else:
            return "%*.0f %s" % (width - 7, x, ratio)


@pytest.fixture(scope="module")
def pivot():
    pb = PivotBenchmark()
    yield pb
    with pb.capsys.disabled():
        pb.display()


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


def test_rdatatable(benchmark, pivot, srcfile):
    filename, n = srcfile
    iters = max(int(1000000 / n), 1)
    timer = ManualTimer()
    script = ("require(data.table);"
              "DT=fread('%s',header=T,na.strings='--');"
              "stopifnot(class(DT$C1) == 'logical');"
              "stopifnot(nrow(DT) == %d);"
              "bb=%d;"
              "tt=system.time(for(b in 1:bb){DC=copy(DT);setkey(DC,C1)});"
              "te=as.numeric(tt['elapsed'])/bb;"
              "cat(paste('Time taken:', te, '\\\\n'));"
              % (filename, n, iters))

    def run():
        out = subprocess.check_output(["R", "-e", script]).decode()
        mm = re.search(r"Time taken: (\d*(?:\.\d*)?(?:[eE][-+]?\d+)?)", out)
        if mm:
            timer.set(float(mm.group(1)))
        else:
            raise RuntimeError(out)

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
