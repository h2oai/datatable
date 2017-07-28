#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import datatable


def test_i4i_small():
    d0 = datatable.DataTable([17, 2, 96, 245, 847569, 34, -45, None, 1])
    assert d0.stypes == ("i4i", )
    d1 = d0(sort=0)
    assert d1.stypes == d0.stypes
    assert d1.internal.isview
    assert d1.internal.check()
    assert d1.topython() == [[None, -45, 1, 2, 17, 34, 96, 245, 847569]]


def test_i4i_small_stable():
    d0 = datatable.DataTable([
        [5, 3, 5, None, 1000000, None, 3, None],
        [1, 5, 10, 20, 50, 100, 200, 500]
    ], colnames=["A", "B"])
    d1 = d0(sort="A")
    assert d1.internal.check()
    assert d1.topython() == [
        [None, None, None, 3, 3, 5, 5, 1000000],
        [20, 100, 500, 5, 200, 1, 10, 50],
    ]


def test_i4i_large():
    N = 1000003  # should be prime
    src = [((n + 1) * 2000003) % N for n in range(N)]
    d0 = datatable.DataTable(src)
    assert d0.stypes == ("i4i", )
    d1 = d0(sort=0)
    assert d1.topython() == [list(range(N))]


def test_i4i_large_stable():
    N = 60000
    src = [None, 100, 100000] * (N // 3)
    d0 = datatable.DataTable({"A": src, "B": list(range(N))})
    assert d0.stypes == ("i4i", "i4i")
    d1 = d0(sort="A", select="B")
    assert d1.topython() == [list(range(0, N, 3)) +
                             list(range(1, N, 3)) +
                             list(range(2, N, 3))]
