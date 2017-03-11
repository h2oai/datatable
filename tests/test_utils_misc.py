#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import pytest
import random

from tests import datatable


def test_plural_form():
    plural = datatable.utils.misc.plural_form
    assert plural(0, "egg") == "0 eggs"
    assert plural(1, "egg") == "1 egg"
    assert plural(2, "egg") == "2 eggs"
    assert plural(-1, "egg") == "-1 egg"
    assert plural(-2, "ham") == "-2 hams"
    assert plural(10, "stock") == "10 stocks"
    assert plural(29, "fine") == "29 fines"
    assert plural(29, "kiwi") == "29 kiwis"
    assert plural(102, "box") == "102 boxes"
    assert plural(7, "try") == "7 tries"
    assert plural(8, "elf") == "8 elves"
    assert plural(3, "buff") == "3 buffs"
    assert plural(9, "life") == "9 lives"
    assert plural(1021, "loss") == "1021 losses"
    assert plural(4, "mesh") == "4 meshes"
    assert plural(5, "branch") == "5 branches"
    assert plural(1, "foot", "feet") == "1 foot"
    assert plural(7, "foot", "feet") == "7 feet"



def test_clamp():
    clamp = datatable.utils.misc.clamp
    assert clamp(0, 1, 10) == 1
    assert clamp(1, 1, 10) == 1
    assert clamp(2, 1, 10) == 2
    assert clamp(10, 1, 10) == 10
    assert clamp(15, 1, 10) == 10
    assert clamp(-1, -5, -3) == -3
    assert clamp(-2, -5, -3) == -3
    assert clamp(-3, -5, -3) == -3
    assert clamp(-4, -5, -3) == -4
    assert clamp(-5, -5, -3) == -5
    assert clamp(-6, -5, -3) == -5
    assert clamp(0.3, 0, 1) == 0.3



def test_normalize_slice():
    norm = datatable.utils.misc.normalize_slice

    class Checker(object):
        def __init__(self, src):
            self._src = src

        def __getitem__(self, item):
            assert isinstance(item, slice)
            start, count, step = norm(item, len(self._src))
            res1 = self._src[item]
            res2 = "".join(self._src[start + i * step] for i in range(count))
            assert res1 == res2

        def __repr__(self):
            return "Checker(%s)" % self._src


    for src in ["", "z", "xy", "Hello", "'tis just a flesh wound!"]:
        check = Checker(src)
        check[::]
        for t in (0, 1, 2, 3, 5, 6, 7, 12, 100, -1, -2, -3, -5, -6, -9, -99):
            check[t:]
            check[t::2]
            check[t::3]
            check[t::-1]
            check[t::-2]
            check[t::-3]
            check[:t]
            check[:t:2]
            check[:t:3]
            check[:t:-1]
            check[:t:-2]
            check[:t:-3]
            check[t:2 * t]

        start_choices = [None] * 10 + list(range(-10, 10)) + [-1000, 1000]
        stop_choices = start_choices
        step_choices = [None, None, 1, -1, 2, -2, 3, -3]
        for _ in range(1000):
            start = random.choice(start_choices)
            stop = random.choice(stop_choices)
            step = random.choice(step_choices)
            check[slice(start, stop, step)]



def test_normalize_range():
    norm = datatable.utils.misc.normalize_range

    class Checker(object):
        def __init__(self, src):
            self._src = src

        def range(self, start, stop, step):
            n = len(self._src)
            rng = range(start, stop, step)
            res = norm(rng, n)
            if res is None:
                array = list(rng)
                assert (array[0] < -n or array[0] >= n or array[-1] < -n or
                        array[-1] >= n or array[0] >= 0 and array[-1] < 0 or
                        array[0] < 0 and array[-1] >= 0)
            else:
                nstart, ncount, nstep = res
                assert ncount >= 0 and nstart >= 0 and nstep == step
                if ncount > 0:
                    assert nstart < n
                    assert 0 <= nstart + (ncount - 1) * nstep < n
                res1 = "".join(self._src[i] for i in rng)
                res2 = "".join(self._src[nstart + i * nstep]
                               for i in range(ncount))
                assert res1 == res2

        def __repr__(self):
            return "Checker(%s)" % self._src

    for src in ["", "z", "xy", "Hello", "'tis just a flesh wound!"]:
        check = Checker(src)
        check.range(0, 0, 1)

        start_choices = [0] * 10 + list(range(-10, 10)) + [-1000, 1000]
        stop_choices = start_choices
        step_choices = [1, -1, 2, -2, 3, -3, 5, 10]
        for _ in range(10000):
            start = random.choice(start_choices)
            stop = random.choice(stop_choices)
            step = random.choice(step_choices)
            check.range(start, stop, step)

