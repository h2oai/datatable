#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2021 H2O.ai
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
from datatable import dt, f, FExpr
from tests import assert_equals


@pytest.mark.parametrize('ttype',
    [dt.bool8, dt.int8, dt.int16, dt.int32, dt.int64, dt.float32, dt.float64,
     dt.Type.date32, dt.Type.time64, dt.obj64])
def test_slice_wrong_types(ttype):
    msg = "Slice expression can only be applied to a column of string type"
    with pytest.raises(TypeError, match=msg):
        DT = dt.Frame(A=[None], type=ttype)
        DT[:, f.A[:1]]


@pytest.mark.parametrize('ttype',
    [dt.bool8, dt.float32, dt.float64, dt.str32, dt.str64,
     dt.Type.date32, dt.Type.time64, dt.obj64])
def test_slice_wrong_types2(ttype):
    DT = dt.Frame(A=[None], B=["Hello"], types={"A": ttype})
    msg = 'Non-integer expressions cannot be used inside a slice'
    with pytest.raises(TypeError, match=msg):
        DT[:, f.B[f.A:]]
    with pytest.raises(TypeError, match=msg):
        DT[:, f.B[:f.A]]
    with pytest.raises(TypeError, match=msg):
        DT[:, f.B[::f.A]]


@pytest.mark.parametrize('slice',
    [slice(None, 5), slice(3, None), slice(None, None, 1), slice(-1, None),
     slice(2, 7), slice(3, -1), slice(5, -8), slice(-8, 5), slice(None, -2)])
def test_slice_normal(slice):
    src = ["Hydrogen", "Helium", "Lithium", "Berillium", "Boron", "Carbon",
           None, "Praseodymium"]
    DT = dt.Frame(A=src)
    RES = DT[:, f.A[slice]]
    EXP = dt.Frame(A=[None if s is None else s[slice] for s in src])
    assert_equals(RES, EXP)


@pytest.mark.parametrize('seed', [random.getrandbits(32) for _ in range(10)])
def test_slice_unicode_random(seed):
    random.seed(seed)
    src = [
        "По улиці вітер віє",
        "Та сніг замітає.",
        "По улиці попідтинню",
        "Вдова шкандибає",
        "Під дзвіницю, сердешная,",
        "Руки простягати",
        "До тих самих, до багатих,",
        "Що сина в солдати",
        "Позаторік заголили.",
        "А думала жити...",
        "Хоч на старість у невістки",
        "В добрі одпочити.",
        "Не довелось. Виблагала",
        "Тую копійчину...",
        "Та пречистій поставила",
        "Свічечку за сина.",
        "",
        "møøse",
        "𝔘𝔫𝔦𝔠𝔬𝔡𝔢",
        "J̲o̲s̲é̲",
        "🚑🐧💚💥✅",
        "[Ⅰ] 💥, [Ⅱ] 🐕, [Ⅲ] 🦊, [Ⅳ] 🐽"
    ]
    a = None if random.random() < 0.2 else \
        random.randint(-3, 10)
    b = None if random.random() < 0.5 else \
        random.randint(-8, 0) if random.random() < 0.6 else \
        random.randint(15, 25)
    DT = dt.Frame(src)
    RES = DT[:, f[0][a:b]]
    EXP = dt.Frame([s[a:b] for s in src])
    assert_equals(RES, EXP)


def test_slice_expression():
    DT = dt.Frame(A=["Monday", "Tuesday", "Wednesday", "Thursday", "Friday"],
                  s=[0, 3, 2, 1, -4],
                  e=[5, 4, 0, -1, None])
    RES = DT[:, (f.A)[f.s:f.e]]
    EXP = dt.Frame(A=["Monda", "s", "", "hursda", "iday"])
    assert_equals(RES, EXP)
