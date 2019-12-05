#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import random
import datatable

# `datatable` doesn't export its `utils`, hence the warning
# noinspection PyUnresolvedReferences
utils_misc = datatable.utils.misc


def test_plural_form():
    """Focus on testing plural form of the word."""
    plural = utils_misc.plural_form
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
    assert plural(-1, "foot", "feet") == "-1 foot"
    assert plural(7, "foot", "feet") == "7 feet"


def test_plural_form2():
    """Focus on testing large numbers."""
    plural = utils_misc.plural_form
    assert plural(1, "cow") == "1 cow"
    assert plural(10, "cow") == "10 cows"
    assert plural(1234, "cow") == "1234 cows"
    assert plural(78789, "cow") == "78789 cows"
    assert plural(99999, "cow") == "99999 cows"
    assert plural(123456, "cow") == "123,456 cows"
    assert plural(1234567, "cow") == "1,234,567 cows"
    assert plural(12002000, "cow") == "12,002,000 cows"
    assert plural(912024001, "cow") == "912,024,001 cows"
    assert plural(9223372036854775807, "cow") == \
        "9,223,372,036,854,775,807 cows"
    assert plural(-1, "cow") == "-1 cow"
    assert plural(-100, "cow") == "-100 cows"
    assert plural(-99999, "cow") == "-99999 cows"
    assert plural(-1234567890, "cow") == "-1,234,567,890 cows"
    assert plural(1000) == "1000"
    assert plural(123345700) == "123,345,700"


def test_clamp():
    clamp = utils_misc.clamp
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



class SliceChecker(object):
    def __init__(self, source):
        self._src = source

    def __getitem__(self, item):
        assert isinstance(item, slice)
        start, count, step = utils_misc.normalize_slice(item, len(self._src))
        res1 = self._src[item]
        res2 = "".join(self._src[start + i * step] for i in range(count))
        return res1 == res2

    def __repr__(self):
        return "Checker(%s)" % self._src


def test_normalize_slice():
    for src in ["", "z", "xy", "Hello", "'tis just a flesh wound!"]:
        check = SliceChecker(src)
        assert check[::]
        for t in (0, 1, 2, 3, 5, 6, 7, 12, 100, -1, -2, -3, -5, -6, -9, -99):
            assert check[t:]
            assert check[t::2]
            assert check[t::3]
            assert check[t::-1]
            assert check[t::-2]
            assert check[t::-3]
            assert check[:t]
            assert check[:t:2]
            assert check[:t:3]
            assert check[:t:-1]
            assert check[:t:-2]
            assert check[:t:-3]
            assert check[t:2 * t]

        start_choices = [-1000, 1000, None] + [None] * 9 + list(range(-10, 10))
        stop_choices = start_choices
        step_choices = [None, None, 1, -1, 2, -2, 3, -3]
        for _ in range(1000):
            start = random.choice(start_choices)
            stop = random.choice(stop_choices)
            step = random.choice(step_choices)
            assert check[slice(start, stop, step)]




class RangeChecker(object):
    def __init__(self, src):
        self._src = src

    def range(self, start, stop, step):
        n = len(self._src)
        rng = range(start, stop, step)
        res = utils_misc.normalize_range(rng, n)
        if res is None:
            array = list(rng)
            assert (not(-n <= array[0] < n and -n <= array[-1] < n) or
                    array[0] >= 0 > array[-1] or
                    array[0] < 0 <= array[-1])
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



def test_normalize_range():
    for src in ["", "z", "xy", "Hello", "'tis just a flesh wound!"]:
        check = RangeChecker(src)
        check.range(0, 0, 1)

        start_choices = [0] * 10 + list(range(-10, 10)) + [-1000, 1000]
        stop_choices = start_choices
        step_choices = [1, -1, 2, -2, 3, -3, 5, 10]
        for _ in range(10000):
            start = random.choice(start_choices)
            stop = random.choice(stop_choices)
            step = random.choice(step_choices)
            check.range(start, stop, step)


def test_humanize_bytes():
    assert utils_misc.humanize_bytes(0) == "0"
    assert utils_misc.humanize_bytes(None) == ""
    assert utils_misc.humanize_bytes(100) == "100B"
    assert utils_misc.humanize_bytes(2 ** 10) == "1KB"
    assert utils_misc.humanize_bytes(10 * 2 ** 10 - 1) == "10.00KB"
    assert utils_misc.humanize_bytes(100 * 2 ** 10 - 1) == "100.0KB"
    assert utils_misc.humanize_bytes(2 ** 20) == "1MB"
    assert utils_misc.humanize_bytes(2 ** 30) == "1GB"
    assert utils_misc.humanize_bytes(2 ** 40) == "1TB"
    try:
        utils_misc.humanize_bytes(-1)
    except AssertionError:
        pass

