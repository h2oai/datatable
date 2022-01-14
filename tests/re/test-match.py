#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2021 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------
import pytest
import random
import re
from datatable import dt, f
from datatable.re import match
from tests import random_string, assert_equals


def test_match_repr():
    assert str(match(f.A, "abc")) == r"FExpr<re.match(f.A, r'abc', icase=False)>"
    assert str(match(f.A, r"\d+", icase=True)) == r"FExpr<re.match(f.A, r'\d+', icase=True)>"


def test_match_simple():
    DT = dt.Frame(A=["abc", "abd", "cab", "acc", None, "aaa"])
    DT1 = DT[:, match(f.A, "ab.")]
    assert_equals(DT1, dt.Frame(A=[True, True, False, False, None, False]))


def test_match_entire_string():
    # match() matches the entire string, not just the beginning
    DT = dt.Frame(A=["a", "ab", "abc", "aaaa"])
    DT1 = DT[:, match(f.A, "a.?")]
    assert_equals(DT1, dt.Frame(A=[True, True, False, False]))


def test_match_ignore_groups():
    # Groups within the regular expression ought to be ignored
    DT = dt.Frame(list("abcdibaldfn"))
    DT1 = DT[match(f[0], "([a-c]+)"), :]
    assert_equals(DT1, dt.Frame(["a", "b", "c", "b", "a"]))


def test_match_case_insensitive():
    DT = dt.Frame(A=["This is an Apple", "banana", "apPle", "Which apple?"])
    DT1 = DT[:, match(f.A, ".*apPle.*")]
    DT2 = DT[:, match(f.A, ".*apPle.*", icase=True)]
    assert_equals(DT1, dt.Frame(A=[False, False, True, False]))
    assert_equals(DT2, dt.Frame(A=[True, False, True, True]))


def test_match_bad_regex1():
    DT = dt.Frame(A=["abc"])
    with pytest.raises(ValueError):
        assert DT[match(f.A, "(."), :]


def test_match_bad_regex2():
    DT = dt.Frame(A=["abc"])
    with pytest.raises(ValueError):
        assert DT[match(f.A, "\\"), :]


def test_match_bad_regex3():
    DT = dt.Frame(A=["abc"])
    with pytest.raises(ValueError):
        assert DT[match(f.A, "???"), :]


def test_match_bad_icase():
    DT = dt.Frame(A=["abc"])
    with pytest.raises(TypeError):
        assert DT[match(f.A, "a", icase=1), :]


@pytest.mark.parametrize("seed", [random.getrandbits(32) for _ in range(5)])
def test_match_random(seed):
    random.seed(seed)
    n = int(random.expovariate(0.001) + 100)
    k = random.randint(2, 12)
    random_re = ""
    random_len = 0
    while random_len < k:
        t = random.random()
        if t < 0.4:
            random_re += "."
        elif t < 0.6:
            random_re += random.choice("abcdefgh")
        elif t < 0.8:
            random_re += ".*"
            random_len += 3
        else:
            random_re += "\\w"
        random_len += 1
    random_rx = re.compile(random_re)

    src = [random_string(k) for _ in range(n)]
    DT = dt.Frame(A=src)
    res = DT[:, match(f.A, random_rx)]
    assert_equals(res,
                  dt.Frame(A=[bool(re.fullmatch(random_rx, s)) for s in src]))


def test_re_method():
    # Remove this test in version 1.1
    DT = dt.Frame(A=["abc", "abd", "cab", "acc", None, "aaa"])
    with pytest.warns(FutureWarning):
        RES = DT[:, f.A.re_match("ab.")]
    assert_equals(RES, dt.Frame(A=[True, True, False, False, None, False]))
