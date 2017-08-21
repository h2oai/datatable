#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
#-------------------------------------------------------------------------------
# This file is used by `pytest` to define common fixtures shared across all
# benchmarks.
#-------------------------------------------------------------------------------
import datatable
import pytest
import subprocess
import re
import py

plural = datatable.utils.misc.plural_form


#-------------------------------------------------------------------------------

class ManualTimer(object):
    def __init__(self):
        self._elapsed_time = 0

    def __call__(self):
        t = self._elapsed_time
        self._elapsed_time = 0
        return t

    def set(self, t):
        self._elapsed_time += t


@pytest.fixture()
def external_test():
    def test(cmd):
        timer = ManualTimer()

        def run():
            out = subprocess.check_output(cmd).decode()
            mm = re.search(r"Time taken: (\d*(?:\.\d*)?(?:[eE][-+]?\d+)?)", out)
            if mm:
                timer.set(float(mm.group(1)))
            else:
                raise RuntimeError(out)

        return run, timer

    return test


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
    try:
        with pb.capsys.disabled():
            pb.display()
    except Exception:
        # Maybe `pb.capsys` was not set; or its use results in an error -- try
        # to display the summary table without the capsys.
        pb.display()


#-------------------------------------------------------------------------------

@pytest.fixture(scope="session")
def pandas():
    """
    This fixture returns pandas module, or if unavailable marks test as skipped.
    """
    try:
        import pandas as pd
        return pd
    except ImportError:
        pytest.skip("Pandas module is required for this test")


@pytest.fixture(scope="session")
def numpy():
    """
    This fixture returns numpy module, or if unavailable marks test as skipped.
    """
    try:
        import numpy as np
        return np
    except ImportError:
        pytest.skip("Numpy module is required for this test")
