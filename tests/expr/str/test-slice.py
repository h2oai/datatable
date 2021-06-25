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


def test_repr():
    assert str(f.a[:]) == "FExpr<(f.a)[:]>"
    assert str(f.b[3:]) == "FExpr<(f.b)[3:]>"
    assert str(f.c[:4]) == "FExpr<(f.c)[:4]>"
    assert str(f.d[1:4]) == "FExpr<(f.d)[1:4]>"
    assert str(f.e[::-1]) == "FExpr<(f.e)[::-1]>"
    assert str(f.f[-1::-1]) == "FExpr<(f.f)[-1::-1]>"
    assert str(f.g[:4:5]) == "FExpr<(f.g)[:4:5]>"
    assert str(f.h[3:4:5]) == "FExpr<(f.h)[3:4:5]>"
    assert str(f.A[0:1]) == "FExpr<(f.A)[0:1]>"
    assert str((f.A + f.B)[:32]) == "FExpr<(f.A + f.B)[:32]>"
    assert str(f.S[:-3] + f.S[-2:]) == "FExpr<(f.S)[:-3] + (f.S)[-2:]>"
    assert str(f.N[f.a:f.b]) == "FExpr<(f.N)[f.a : f.b]>"
    assert str(f.N[f.a:-1]) == "FExpr<(f.N)[f.a : -1]>"
    assert str(f.N[f.a::4]) == "FExpr<(f.N)[f.a :: 4]>"
    assert str(f.N[::f.s]) == "FExpr<(f.N)[:: f.s]>"
    assert str(f.N[:f.x:f.s]) == "FExpr<(f.N)[: f.x : f.s]>"
    assert str(f.N[f.w:f.x:f.s]) == "FExpr<(f.N)[f.w : f.x : f.s]>"


def test_slice_function():
    from datatable.str import slice
    assert str(slice(f[0], 5, 6)) == "FExpr<(f[0])[5:6]>"
    assert str(slice(f[3], 1, 2, 3)) == "FExpr<(f[3])[1:2:3]>"
    assert str(slice(f.X, start=f.y, stop=f.z)) == "FExpr<(f.X)[f.y : f.z]>"
    assert str(slice(f.X, f.a, None, step=f.b)) == "FExpr<(f.X)[f.a :: f.b]>"


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


@pytest.mark.parametrize('slice',
    [slice(None, None, 2), slice(None, None, 3), slice(2, None, 2),
     slice(1, -1, 3), slice(-5, -1, 2), slice(None, -1, 2)])
def test_slice_large_step(slice):
    src = ["Mercury", "Venus", "Earth", "Mars", "Jupiter", "Saturn", "Uranus",
           "Neptune"]
    DT = dt.Frame(Planet=src)
    RES = DT[:, f.Planet[slice]]
    EXP = dt.Frame(Planet=[s[slice] for s in src])
    assert_equals(RES, EXP)


@pytest.mark.parametrize('slice',
    [slice(None, None, -1), slice(None, None, -2), slice(2, None, -1),
     slice(1, -1, -3), slice(-15, -2, -2), slice(None, -1, -2),
     slice(None, 3, -2)])
def test_slice_negative_step(slice):
    src = ["Juneteenth", "Independence Day", "Christmas", "Halloween",
           "Labor Day", "Memorial Day", "Native American Day", "MLK Day"]
    DT = dt.Frame(Planet=src)
    RES = DT[:, f.Planet[slice]]
    EXP = dt.Frame(Planet=[s[slice] for s in src])
    assert_equals(RES, EXP)


@pytest.mark.parametrize('seed', [random.getrandbits(32) for _ in range(20)])
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
    c = None if random.random() < 0.6 else \
        random.randint(2, 5) if random.random() < 0.5 else \
        random.randint(-4, -1)
    DT = dt.Frame(src)
    RES = DT[:, f[0][a:b:c]]
    EXP = dt.Frame([s[a:b:c] for s in src])
    assert_equals(RES, EXP)


def test_slice_expression():
    DT = dt.Frame(A=["Monday", "Tuesday", "Wednesday", "Thursday", "Friday"],
                  s=[0, 3, 2, 1, -4],
                  e=[5, 4, 0, -1, None])
    RES = DT[:, (f.A)[f.s:f.e]]
    EXP = dt.Frame(A=["Monda", "s", "", "hursda", "iday"])
    assert_equals(RES, EXP)


def test_slice_zero_step():
    # Check that zero step in a slice produces an NA value
    DT = dt.Frame(A=["abcdefghij", "ABCDEFGHIJ", "---"], B=[2, 1, 0])
    RES = DT[:, f.A[::f.B]]
    EXP = dt.Frame(A=["acegi", "ABCDEFGHIJ", None])
    assert_equals(RES, EXP)
