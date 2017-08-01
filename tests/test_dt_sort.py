#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import datatable
import pytest


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
    # p1, p2 should both be prime, with p1 < p2. Here we rely on the fact that
    # a multiplicative group of integers module n is cyclic with order n-1 when
    # n is prime.
    p1 = 1000003
    p2 = 2000003
    src = [((n + 1) * p2) % p1 for n in range(p1)]
    d0 = datatable.DataTable(src)
    assert d0.stypes == ("i4i", )
    d1 = d0(sort=0)
    assert d1.topython() == [list(range(p1))]


@pytest.mark.parametrize("n", [30, 300, 3000, 30000, 120000])
def test_i4i_large_stable(n):
    src = [None, 100, 100000] * (n // 3)
    d0 = datatable.DataTable({"A": src, "B": list(range(n))})
    assert d0.stypes[0] == "i4i"
    d1 = d0(sort="A", select="B")
    assert d1.topython() == [list(range(0, n, 3)) +
                             list(range(1, n, 3)) +
                             list(range(2, n, 3))]
