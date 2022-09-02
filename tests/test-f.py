#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2019-2020 H2O.ai
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
import datatable as dt
from datatable import f
from tests import assert_equals, noop

@pytest.fixture()
def DT():
    return dt.Frame([
        [2, 7, 0, 0],
        [True, False, False, True],
        [1, 1, 1, 1],
        [0.1, 2, -4, 4.4],
        [None, None, None, None],
        [0, 0, 0, 0],
        ["1", "2", "hello", "world"],
    ],
    names=list("ABCDEFG"),
    stypes=[dt.int32, dt.bool8, dt.int64, dt.float32, dt.int16,
            dt.float64, dt.str32])


#-------------------------------------------------------------------------------
# Plain `f`
#-------------------------------------------------------------------------------

def test_f():
    assert str(f) == "Namespace(0)"
    assert f.__class__ == type(f)
    # Check that missing system atrtibutes raise an AttributeError
    # instead of being converted into a column selector expression
    with pytest.raises(AttributeError):
        f.__name__



#-------------------------------------------------------------------------------
# Stringify
#-------------------------------------------------------------------------------

def test_f_col_selector_unbound():
    # Check that unbounded col-selectors can be stringified. The precise
    # representation may be modified in the future; however f-expressions
    # should not raise exceptions when printed.
    # See issues #1024 and #1241
    assert str(f.a) == "FExpr<f.a>"
    assert str(f.abcdefghijkl) == "FExpr<f.abcdefghijkl>"
    assert str(f.abcdefghijklm) == "FExpr<f.abcdefghijklm>"
    assert str(f[0]) == "FExpr<f[0]>"
    assert str(f[1000]) == "FExpr<f[1000]>"
    assert str(f[-1]) == "FExpr<f[-1]>"
    assert str(f[-999]) == "FExpr<f[-999]>"
    assert str(f[""]) == "FExpr<f['']>"
    assert str(f["0"]) == "FExpr<f['0']>"
    assert str(f["A+B"]) == "FExpr<f['A+B']>"
    assert str(f["_A"]) == "FExpr<f['_A']>"
    assert str(f["_54"]) == "FExpr<f['_54']>"
    assert str(f._3_) == "FExpr<f._3_>"
    assert str(f.a_b_c) == "FExpr<f.a_b_c>"
    assert str(f[" y "]) == "FExpr<f[' y ']>"
    assert str(f["a b c"]) == "FExpr<f['a b c']>"
    assert str(f['"a b c"']) == "FExpr<f['\"a b c\"']>"


def test_f_col_selector_invalid():
    with pytest.raises(TypeError) as e:
        noop(f[2.5])
    assert str(e.value) == ("Column selector should be an integer, string, or "
                            "slice, or list/tuple, not <class 'float'>")
    # Note: at some point we may start supporting all the expressions below:
    with pytest.raises(TypeError):
        noop(f[lambda: 1])


def test_f_col_selector_list_tuple():
    assert str(f[[7, 4]]) == "FExpr<f[[7, 4]]>"
    assert str(f[("A", "B", "C")]) == "FExpr<f[['A', 'B', 'C']]>"


def test_f_expressions():
    assert str(f.C1 < f.C2) == "FExpr<f.C1 < f.C2>"
    assert str(f.C1 > f.C2) == "FExpr<f.C1 > f.C2>"
    assert str(f.C1 <= f.C2) == "FExpr<f.C1 <= f.C2>"
    assert str(f.C1 >= f.C2) == "FExpr<f.C1 >= f.C2>"


def test_f_columnset_str():
    assert str(f[None]) == "FExpr<f[None]>"
    assert str(f[:]) == "FExpr<f[:]>"
    assert str(f[:7]) == "FExpr<f[:7]>"
    assert str(f[::-1]) == "FExpr<f[::-1]>"
    assert str(f['Z':'A']) == "FExpr<f['Z':'A']>"
    assert str(f[bool]) == "FExpr<f[bool]>"
    assert str(f[int]) == "FExpr<f[int]>"
    assert str(f[float]) == "FExpr<f[float]>"
    assert str(f[str]) == "FExpr<f[str]>"
    assert str(f[object]) == "FExpr<f[object]>"
    assert str(f[dt.int32]) == "FExpr<f[stype.int32]>"
    assert str(f[dt.float64]) == "FExpr<f[stype.float64]>"
    assert str(f[dt.ltype.int]) == "FExpr<f[ltype.int]>"
    assert str(f[int, float]) == "FExpr<f[[int, float]]>"
    assert str(f[dt.int32, dt.float64, dt.str32]) == \
        "FExpr<f[[stype.int32, stype.float64, stype.str32]]>"


def test_f_columnset_extend():
    assert str(f[:].extend(f.A)) == \
        "Expr:setplus(FExpr<f[:]>, FExpr<f.A>; )"
    assert str(f[int].extend(f[str])) == \
        "Expr:setplus(FExpr<f[int]>, FExpr<f[str]>; )"
    assert str(f.A.extend(f['B','C'])) == \
        "Expr:setplus(FExpr<f.A>, FExpr<f[['B', 'C']]>; )"


def test_f_columnset_remove():
    assert str(f[:].remove(f.A)) == "Expr:setminus(FExpr<f[:]>, FExpr<f.A>; )"
    assert str(f[int].remove(f[0])) == "Expr:setminus(FExpr<f[int]>, FExpr<f[0]>; )"
    assert str(f.A.remove(f['B','C'])) == \
        "Expr:setminus(FExpr<f.A>, FExpr<f[['B', 'C']]>; )"



#-------------------------------------------------------------------------------
# Select individual columns
#-------------------------------------------------------------------------------

def test_f_int(DT):
    assert_equals(DT[:, f[3]], DT[:, 3])
    assert_equals(DT[:, f[-1]], DT[:, 6])
    assert_equals(DT[f[0] > 0, f[-1]], dt.Frame(G=["1", "2"]))

    with pytest.raises(ValueError, match="Column index 10 is invalid for a "
                                         "Frame with 7 columns"):
        assert DT[:, f[10]]


def test_f_str(DT):
    assert_equals(DT[:, "B"], DT[:, 1])
    assert_equals(DT[:, f.B], DT[:, 1])
    assert_equals(DT[:, f["B"]], DT[:, 1])
    for i, name in enumerate(DT.names):
        assert_equals(DT[:, f[name]], DT[:, i])

    with pytest.raises(KeyError) as e:
        noop(DT[:, f["d"]])
    assert ("Column d does not exist in the Frame; "
            "did you mean D, A or B?" == str(e.value))




#-------------------------------------------------------------------------------
# Select columnsets
#-------------------------------------------------------------------------------

def test_f_columnset(DT):
    assert_equals(DT[:, f[:]], DT)
    assert_equals(DT[:, f[::-1]], DT[:, ::-1])
    assert_equals(DT[:, f[:4]], DT[:, :4])
    assert_equals(DT[:, f[3:4]], DT[:, 3:4])
    assert_equals(DT[:, f["B":"E"]], DT[:, 1:5])
    assert_equals(DT[:, f[bool]], DT[:, 1])
    assert_equals(DT[:, f[int]], DT[:, [0, 2, 4]])
    assert_equals(DT[:, f[float]], DT[:, [3, 5]])
    assert_equals(DT[:, f[str]], DT[:, 6])
    assert_equals(DT[:, f[dt.str32]], DT[:, 6])
    assert_equals(DT[:, f[dt.str64]], DT[:, []])
    assert_equals(DT[:, f[None]], DT[:, []])


def test_f_columnset_stypes(DT):
    for st in dt.stype:
        assert_equals(DT[:, f[st]],
                      DT[:, [i for i in range(DT.ncols)
                             if DT.stypes[i] == st]])


def test_f_columnset_ltypes(DT):
    for lt in dt.ltype:
        if lt == dt.ltype.invalid:
            return
        assert_equals(DT[:, f[lt]],
                      DT[:, [i for i in range(DT.ncols)
                             if DT.ltypes[i] == lt]])


def test_columnset_sum(DT):
    assert_equals(DT[:, f[int].extend(f[float])], DT[:, [int, float]])
    assert_equals(DT[:, f[:3].extend(f[-3:])], DT[:, [0, 1, 2, -3, -2, -1]])
    assert_equals( DT[:, f['A','B','C'].extend(f['E','F', 'G'])], DT[:, [0, 1, 2, -3, -2, -1]])
    assert_equals(DT[:, f.A.extend(f.B)], DT[:, ['A', 'B']])
    assert_equals(DT[:, f[:].extend({"extra": f.A + f.C})],
                  dt.cbind(DT, DT[:, {"extra": f.A + f.C}]))


def test_columnset_diff(DT):
    assert_equals(DT[:, f[:].remove(f[3])], DT[:, [0, 1, 2, 4, 5, 6]])
    assert_equals(DT[:, f[:].remove(f[2:-2])], DT[:, [0, 1, 5, 6]])
    assert_equals(DT[:, f[:5].remove(f[int])], DT[:, [1, 3]])
    assert_equals(DT[:, f[:].remove(f[100:])], DT)
    assert_equals(DT[:, f[:].remove(f["F":])], DT[:, :"E"])



#-------------------------------------------------------------------------------
# Misc methods
#-------------------------------------------------------------------------------

def test_sum():
    assert str(dt.sum(f.A)) == str(f.A.sum())
    assert str(dt.sum(f[:])) == str(f[:].sum())
    DT = dt.Frame(A=range(1, 10))
    assert_equals(DT[:, f.A.sum()], DT[:, dt.sum(f.A)])


def test_max():
    assert str(dt.max(f.A)) == str(f.A.max())
    assert str(dt.max(f[:])) == str(f[:].max())
    DT = dt.Frame(A=range(1, 10))
    assert_equals(DT[:, f.A.max()], DT[:, dt.max(f.A)])


def test_mean():
    assert str(dt.mean(f.A)) == str(f.A.mean())
    assert str(dt.mean(f[:])) == str(f[:].mean())
    DT = dt.Frame(A=range(1, 10))
    assert_equals(DT[:, f.A.mean()], DT[:, dt.mean(f.A)])


def test_median():
    assert str(dt.median(f.A)) == str(f.A.median())
    assert str(dt.median(f[:])) == str(f[:].median())
    DT = dt.Frame(A=[2, 3, 5, 5, 9, -1, 2.2])
    assert_equals(DT[:, f.A.median()], DT[:, dt.median(f.A)])


def test_min():
    assert str(dt.min(f.A)) == str(f.A.min())
    assert str(dt.min(f[:])) == str(f[:].min())
    DT = dt.Frame(A=[2, 3, 5, 5, 9, -1, 2.2])
    assert_equals(DT[:, f.A.min()], DT[:, dt.min(f.A)])


def test_rowall():
    assert str(dt.rowall(f.A)) == str(f.A.rowall())
    assert str(dt.rowall(f[:])) == str(f[:].rowall())
    DT = dt.Frame({'A': [True, True], 'B': [True, False]})
    assert_equals(DT[:, f[:].rowall()], DT[:, dt.rowall(f[:])])


def test_rowany():
    assert str(dt.rowany(f.A)) == str(f.A.rowany())
    assert str(dt.rowany(f[:])) == str(f[:].rowany())
    DT = dt.Frame({'A': [True, True], 'B': [True, False]})
    assert_equals(DT[:, f[:].rowany()], DT[:, dt.rowany(f[:])])


def test_rowfirst():
    assert str(dt.rowfirst(f.A)) == str(f.A.rowfirst())
    assert str(dt.rowfirst(f[:])) == str(f[:].rowfirst())
    DT = dt.Frame({'A':[1, None, None, None],
                   'B':[None, 3, 4, None],
                   'C':[2, None, 5, None]})

    assert_equals(DT[:, f[:].rowfirst()], DT[:, dt.rowfirst(f[:])])


def test_rowlast():
    assert str(dt.rowlast(f.A)) == str(f.A.rowlast())
    assert str(dt.rowlast(f[:])) == str(f[:].rowlast())
    DT = dt.Frame({'A':[1, None, None, None],
                   'B':[None, 3, 4, None],
                   'C':[2, None, 5, None]})

    assert_equals(DT[:, f[:].rowlast()], DT[:, dt.rowlast(f[:])])


def test_rowmax():
    assert str(dt.rowmax(f.A)) == str(f.A.rowmax())
    assert str(dt.rowmax(f[:])) == str(f[:].rowmax())
    DT = dt.Frame({"C": [2, 5, 30, 20, 10],
                   "D": [10, 8, 20, 20, 1]})

    assert_equals(DT[:, f[:].rowmax()], DT[:, dt.rowmax(f[:])])


def test_rowmin():
    assert str(dt.rowmin(f.A)) == str(f.A.rowmin())
    assert str(dt.rowmin(f[:])) == str(f[:].rowmin())
    DT = dt.Frame({"C": [2, 5, 30, 20, 10],
                   "D": [10, 8, 20, 20, 1]})

    assert_equals(DT[:, f[:].rowmin()], DT[:, dt.rowmin(f[:])])


def test_rowsum():
    assert str(dt.rowsum(f.A)) == str(f.A.rowsum())
    assert str(dt.rowsum(f[:])) == str(f[:].rowsum())
    DT = dt.Frame({"C": [2, 5, 30, 20, 10],
                   "D": [10, 8, 20, 20, 1]})

    assert_equals(DT[:, f[:].rowsum()], DT[:, dt.rowsum(f[:])])


def test_rowmean():
    assert str(dt.rowmean(f.A)) == str(f.A.rowmean())
    assert str(dt.rowmean(f[:])) == str(f[:].rowmean())
    DT = dt.Frame({"C": [2, 5, 30, 20, 10],
                   "D": [10, 8, 20, 20, 1]})

    assert_equals(DT[:, f[:].rowmean()], DT[:, dt.rowmean(f[:])])


def test_rowcount():
    assert str(dt.rowcount(f.A)) == str(f.A.rowcount())
    assert str(dt.rowcount(f[:])) == str(f[:].rowcount())
    DT = dt.Frame({"C": [2, 5, 30, 20, 10],
                   "D": [10, 8, 20, 20, 1]})

    assert_equals(DT[:, f[:].rowcount()], DT[:, dt.rowcount(f[:])])


def test_rowsd():
    assert str(dt.rowsd(f.A)) == str(f.A.rowsd())
    assert str(dt.rowsd(f[:])) == str(f[:].rowsd())
    DT = dt.Frame({"C": [2, 5, 30, 20, 10],
                   "D": [10, 8, 20, 20, 1]})

    assert_equals(DT[:, f[:].rowsd()], DT[:, dt.rowsd(f[:])])


def test_sd():
    assert str(dt.sd(f.A)) == str(f.A.sd())
    assert str(dt.sd(f[:])) == str(f[:].sd())
    DT = dt.Frame({"C": [2, 5, 30, 20, 10],
                   "D": [10, 8, 20, 20, 1]})

    assert_equals(DT[:, f[:].sd()], DT[:, dt.sd(f[:])])


def test_shift():
    assert str(dt.shift(f.A)) == str(f.A.shift())
    assert str(dt.shift(f[:])) == str(f[:].shift())
    DT = dt.Frame({"C": [2, 5, 30, 20, 10],
                   "D": [10, 8, 20, 20, 1]})

    assert_equals(DT[:, f[:].shift()], DT[:, dt.shift(f[:])])


def test_shift_n():
    DT = dt.Frame(a=range(10))
    assert_equals(DT[:, [f.a.shift(n=3), f.a.shift(-1)]],
                  DT[:, [dt.shift(f.a, 3), dt.shift(f.a, -1)]])


def test_last():
    assert str(dt.last(f.A)) == str(f.A.last())
    assert str(dt.last(f[:])) == str(f[:].last())
    DT = dt.Frame({"C": [2, 5, 30, 20, 10],
                   "D": [10, 8, 20, 20, 1]})

    assert_equals(DT[:, f[:].last()], DT[:, dt.last(f[:])])


def test_count():
    assert str(dt.count(f.A)) == str(f.A.count())
    assert str(dt.count(f[:])) == str(f[:].count())
    DT = dt.Frame({'A': ['1', '1', '2', '1', '2'],
                   'B': [None, '2', '3', '4', '5'],
                   'C': [1, 2, 1, 1, 2]})

    assert_equals(DT[:, f.A.count()], DT[:, dt.count(f.A)])
    assert_equals(DT[:, f[:].count()], DT[:, dt.count(f[:])])


def test_first():
    assert str(dt.first(f.A)) == str(f.A.first())
    assert str(dt.first(f[:])) == str(f[:].first())
    DT = dt.Frame({'A': ['1', '1', '2', '1', '2'],
                   'B': [None, '2', '3', '4', '5'],
                   'C': [1, 2, 1, 1, 2]})

    assert_equals(DT[:, f.A.first()], DT[:, dt.first(f.A)])
    assert_equals(DT[:, f[:].first()], DT[:, dt.first(f[:])])


def test_as_type():
    assert str(dt.as_type(f.A, int)) == str(f.A.as_type(int))
    assert str(dt.as_type(f[:], int)) == str(f[:].as_type(int))
    DT = dt.Frame({'A': ['1.0', '1.0', '2.0', '1.0', '2'],
                   'B': [None, '2', '3', '4', '5'],
                   'C': [1, 2, 1, 1, 2]})

    assert_equals(DT[:, f.A.as_type(int)], DT[:, dt.as_type(f.A, int)])
    assert_equals(DT[:, f[:].as_type(float)], DT[:, dt.as_type(f[:], float)])


def test_prod():
    assert str(dt.prod(f.A)) == str(f.A.prod())
    assert str(dt.prod(f[:])) == str(f[:].prod())
    DT = dt.Frame(A=range(1, 10))
    assert_equals(DT[:, f.A.prod()], DT[:, dt.prod(f.A)])


def test_nunique():
    assert str(dt.nunique(f.A)) == str(f.A.nunique())
    assert str(dt.nunique(f[:])) == str(f[:].nunique())
    DT = dt.Frame(A=range(1, 5))
    assert_equals(DT[:, f.A.nunique()], DT[:, dt.nunique(f.A)])


def test_countna():
    assert str(dt.countna(f.A)) == str(f.A.countna())
    assert str(dt.countna(f[:])) == str(f[:].countna())
    DT = dt.Frame(A = [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    assert_equals(DT[:, f.A.countna()], DT[:, dt.countna(f.A)])


def test_cumsum():
    assert str(dt.cumsum(f.A)) == str(f.A.cumsum())
    assert str(dt.cumsum(f[:])) == str(f[:].cumsum())
    DT = dt.Frame(A = [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    assert_equals(DT[:, f.A.cumsum()], DT[:, dt.cumsum(f.A)])


def test_cummax():
    assert str(dt.cummax(f.A)) == str(f.A.cummax())
    assert str(dt.cummax(f[:])) == str(f[:].cummax())
    DT = dt.Frame(A = [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    assert_equals(DT[:, f.A.cummax()], DT[:, dt.cummax(f.A)])

def test_cummin():
    assert str(dt.cummin(f.A)) == str(f.A.cummin())
    assert str(dt.cummin(f[:])) == str(f[:].cummin())
    DT = dt.Frame(A = [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    assert_equals(DT[:, f.A.cummin()], DT[:, dt.cummin(f.A)])

def test_cumprod():
    assert str(dt.cumprod(f.A)) == str(f.A.cumprod())
    assert str(dt.cumprod(f[:])) == str(f[:].cumprod())
    DT = dt.Frame(A = [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    assert_equals(DT[:, f.A.cumprod()], DT[:, dt.cumprod(f.A)])


def test_fillna():
    assert str(dt.fillna(f.A, reverse=False)) == str(f.A.fillna(reverse=False))
    assert str(dt.fillna(f.A, reverse=True)) == str(f.A.fillna(reverse=True))
    assert str(dt.fillna(f[:], True)) == str(f[:].fillna(True))
    DT = dt.Frame(A = [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    assert_equals(DT[:, f.A.fillna(reverse=True)], DT[:, dt.fillna(f.A, True)])
    assert_equals(DT[:, f.A.fillna(reverse=False)], DT[:, dt.fillna(f.A, False)])


def test_nth():
    assert str(dt.nth(f.A, nth=0)) == str(f.A.nth(nth=0))
    assert str(dt.nth(f.A, nth=1)) == str(f.A.nth(nth=1))
    assert str(dt.nth(f[:], -1)) == str(f[:].nth(-1))
    DT = dt.Frame(A = [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    assert_equals(DT[:, f.A.nth(nth=1)], DT[:, dt.nth(f.A, 1)])
    assert_equals(DT[:, f.A.nth(nth=0)], DT[:, dt.nth(f.A, 0)])