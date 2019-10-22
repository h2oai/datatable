#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#-------------------------------------------------------------------------------
import datatable as dt
import pytest
import random
import re
from datatable import stype, f
from datatable.internal import frame_integrity_check
from tests import noop, random_string, assert_equals


def test_issue_1912():
    # Check that the expression `A==None` works if A is a string column
    DT = dt.Frame(A=["dfv", None, None, "adfknlkad", None])
    RES = DT[:, f.A == None]
    assert RES.to_list()[0] == [0, 1, 1, 0, 1]



#-------------------------------------------------------------------------------
# split_into_nhot
#-------------------------------------------------------------------------------

def test_split_into_nhot_noarg():
    with pytest.raises(ValueError) as e:
        noop(dt.split_into_nhot())
    assert ("Required parameter `frame` is missing" == str(e.value))


def test_split_into_nhot_none():
    f0 = dt.split_into_nhot(None)
    assert(f0 == None)


def test_split_into_nhot_empty():
    f0 = dt.split_into_nhot(dt.Frame(["", None]))
    assert_equals(f0, dt.Frame())


@pytest.mark.parametrize('sort', [False, True])
def test_split_into_nhot0(sort):
    f0 = dt.Frame(["cat, dog, mouse, peacock, frog",
                   "armadillo, fox, hedgehog",
                   None,
                   "dog, fox, mouse, cat, peacock",
                   "horse, raccoon, cat, frog, dog"])
    f1 = dt.split_into_nhot(f0, sort = sort)
    frame_integrity_check(f1)
    fr = dt.Frame({"cat":       [1, 0, None, 1, 1],
                   "dog":       [1, 0, None, 1, 1],
                   "mouse":     [1, 0, None, 1, 0],
                   "peacock":   [1, 0, None, 1, 0],
                   "frog":      [1, 0, None, 0, 1],
                   "armadillo": [0, 1, None, 0, 0],
                   "fox":       [0, 1, None, 1, 0],
                   "hedgehog":  [0, 1, None, 0, 0],
                   "horse":     [0, 0, None, 0, 1],
                   "raccoon":   [0, 0, None, 0, 1]})
    assert set(f1.names) == set(fr.names)
    if sort :
        assert list(f1.names) == sorted(fr.names)
    fr = fr[:, f1.names]
    assert f1.names == fr.names
    assert f1.stypes == (dt.stype.bool8, ) * f1.ncols
    assert f1.shape == fr.shape
    assert f1.to_list() == fr.to_list()


def test_split_into_nhot1():
    f0 = dt.Frame(["  meow  \n",
                   None,
                   "[ meow]",
                   "['meow' ,purr]",
                   '(\t"meow", \'purr\')',
                   "{purr}"])
    f1 = dt.split_into_nhot(f0)
    frame_integrity_check(f1)
    fr = dt.Frame(meow=[1, None, 1, 1, 1, 0], purr=[0, None, 0, 1, 1, 1])
    assert set(f1.names) == set(fr.names)
    fr = fr[..., f1.names]
    assert f1.shape == fr.shape == (6, 2)
    assert f1.stypes == (dt.stype.bool8, dt.stype.bool8)
    assert f1.to_list() == fr.to_list()


def test_split_into_nhot_sep():
    f0 = dt.Frame(["a|b|c", "b|a", None, "a|c"])
    f1 = dt.split_into_nhot(f0, sep="|")
    assert set(f1.names) == {"a", "b", "c"}
    fr = dt.Frame(a=[1, 1, None, 1], b=[1, 1, None, 0], c=[1, 0, None, 1])
    assert set(f1.names) == set(fr.names)
    assert f1.to_list() == fr[:, f1.names].to_list()


def test_split_into_nhot_quotes():
    f0 = dt.split_into_nhot(dt.Frame(['foo, "bar, baz"']))
    f1 = dt.split_into_nhot(dt.Frame(['foo, "bar, baz']))
    assert set(f0.names) == {"foo", "bar, baz"}
    assert set(f1.names) == {"foo", '"bar', "baz"}


def test_split_into_nhot_bad():
    f0 = dt.Frame([[1.25], ["foo"], ["bar"]])
    with pytest.raises(ValueError) as e:
        dt.split_into_nhot(f0)
    assert ("Function split_into_nhot() may only be applied to a single-column "
            "Frame of type string; got frame with 3 columns" == str(e.value))

    with pytest.raises(TypeError) as e:
        dt.split_into_nhot(f0[:, 0])
    assert ("Function split_into_nhot() may only be applied to a single-column "
            "Frame of type string; received a column of type float64" ==
            str(e.value))

    with pytest.raises(ValueError) as e:
        dt.split_into_nhot(f0[:, 1], sep=",;-")
    assert ("Parameter `sep` in split_into_nhot() must be a single character"
            in str(e.value))


@pytest.mark.parametrize("seed, st", [(random.getrandbits(32), stype.str32),
                                      (random.getrandbits(32), stype.str64)])
def test_split_into_nhot_long(seed, st):
    random.seed(seed)
    n = int(random.expovariate(0.0001) + 100)
    col1 = [random.getrandbits(1) for _ in range(n)]
    col2 = [random.getrandbits(1) for _ in range(n)]
    col3 = [random.getrandbits(1) for _ in range(n)]
    col4 = [0] * n

    data = [",".join(["liberty"] * col1[i] +
                     ["equality"] * col2[i] +
                     ["justice"] * col3[i]) for i in range(n)]

    # Introduce 1% of None's, making sure we preserve data at freedom_index
    na_indices = random.sample(range(n), n // 100)
    freedom_index = random.randint(0, n - 1)
    for i in na_indices:
        if i == freedom_index: continue
        col1[i] = None
        col2[i] = None
        col3[i] = None
        col4[i] = None
        data[i] = None
    col4[freedom_index] = 1
    data[freedom_index] += ", freedom"

    f0 = dt.Frame(data, stype=st)
    assert f0.stypes == (st,)
    assert f0.shape == (n, 1)
    f1 = dt.split_into_nhot(f0)
    assert f1.shape == (n, 4)
    fr = dt.Frame(liberty=col1, equality=col2, justice=col3, freedom=col4)
    assert set(f1.names) == set(fr.names)
    f1 = f1[..., fr.names]
    assert f1.to_list() == fr.to_list()


def test_split_into_nhot_view():
    f0 = dt.Frame(A=["cat,dog,mouse", "mouse", None, "dog, cat"])
    f1 = dt.split_into_nhot(f0[::-1, :])
    f2 = dt.split_into_nhot(f0[3, :])
    assert set(f1.names) == {"cat", "dog", "mouse"}
    assert f1[:, ["cat", "dog", "mouse"]].to_list() == \
           [[1, None, 0, 1], [1, None, 0, 1], [0, None, 1, 1]]
    assert set(f2.names) == {"cat", "dog"}
    assert f2[:, ["cat", "dog"]].to_list() == [[1], [1]]



#-------------------------------------------------------------------------------
# re_match()
#-------------------------------------------------------------------------------

regexp_test = pytest.mark.skipif(not dt.internal.regex_supported(),
                                 reason="Regex not supported")

@regexp_test
def test_re_match():
    f0 = dt.Frame(A=["abc", "abd", "cab", "acc", None, "aaa"])
    f1 = f0[:, f.A.re_match("ab.")]
    assert f1.stypes == (dt.stype.bool8,)
    assert f1.to_list() == [[True, True, False, False, None, False]]


@regexp_test
def test_re_match2():
    # re_match() matches the entire string, not just the beginning...
    f0 = dt.Frame(A=["a", "ab", "abc", "aaaa"])
    f1 = f0[:, f.A.re_match("a.?")]
    assert f1.stypes == (dt.stype.bool8,)
    assert f1.to_list() == [[True, True, False, False]]


@regexp_test
def test_re_match_ignore_groups():
    # Groups within the regular expression ought to be ignored
    f0 = dt.Frame(list("abcdibaldfn"))
    f1 = f0[f[0].re_match("([a-c]+)"), :]
    assert f1.to_list() == [["a", "b", "c", "b", "a"]]


@regexp_test
def test_re_match_bad_regex1():
    with pytest.raises(ValueError):
        noop(dt.Frame(["abc"])[f.A.re_match("(."), :])
    # assert ("Invalid regular expression: it contained mismatched ( and )"
    #         in str(e.value))


@regexp_test
def test_re_match_bad_regex2():
    with pytest.raises(ValueError):
        noop(dt.Frame(["abc"])[f.A.re_match("\\j"), :])
    # assert ("Invalid regular expression: it contained an invalid escaped "
    #         "character, or a trailing escape"
    #         in str(e.value))


@regexp_test
def test_re_match_bad_regex3():
    with pytest.raises(ValueError):
        noop(dt.Frame(["abc"])[f.A.re_match("???"), :])
    # assert ("Invalid regular expression: One of *?+{ was not preceded by a "
    #         "valid regular expression"
    #         in str(e.value))


@regexp_test
@pytest.mark.parametrize("seed", [random.getrandbits(32) for _ in range(5)])
def test_re_match_random(seed):
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
    frame = dt.Frame(A=src)
    frame_res = frame[:, f.A.re_match(random_rx)]
    assert frame_res.shape == (n, 1)

    res = [bool(re.fullmatch(random_rx, s)) for s in src]
    dtres = frame_res.to_list()[0]
    assert res == dtres
