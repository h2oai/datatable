#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
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
import datatable as dt
from datatable import f, stype, ltype
from tests import list_equals, has_llvm


# Sets of tuples containing test columns of each type
dt_bool = [[False, True, False, False, True],
           [True, None, None, True, False]]

dt_int = [[5, -3, 6, 3, 0],
          [None, -1, 0, 26, -3],
          # TODO: currently ~ operation fails on v = 2**31 - 1. Should we
          #       promote the resulting column to int64 in such case?
          [2**31 - 2, -(2**31 - 1), 0, -1, 1]]

dt_float = [[9.5, 0.2, 5.4857301, -3.14159265338979],
            [1.1, 2.3e12, -.5, None, float("inf"), 0.0]]

dt_str = [["foo", "bbar", "baz"],
          [None, "", " ", "  ", None, "\x00"],
          list("qwertyuiiop[]asdfghjkl;'zxcvbnm,./`1234567890-=")]

dt_obj = [[dt, pytest, random, f, dt_int, None],
          ["one", None, 3, 9.99, stype.int32, object, isinstance]]


#-------------------------------------------------------------------------------
# f
#-------------------------------------------------------------------------------

def test_f():
    assert str(f) == "f"
    assert repr(f) == "FrameProxy('f', None)"
    with f.bind_datatable(dt.Frame()):
        assert str(f) == "f"
        assert repr(f) == "FrameProxy('f', <Frame [0 rows x 0 cols]>)"
    assert repr(f) == "FrameProxy('f', None)"


def test_f_col_selector_unbound():
    # Check that unbounded col-selectors can be stringified. The precise
    # representation may be modified in the future; however f-expressions
    # should not raise exceptions when printed.
    # See issues #1024 and #1241
    assert repr(f.a) == "ColSelectorExpr(f.a)"
    assert str(f.a) == "f.a"
    assert str(f.abcdefghijkl) == "f.abcdefghijkl"
    assert str(f.abcdefghijklm) == "f['abcdefghijklm']"
    assert str(f[0]) == "f[0]"
    assert str(f[1000]) == "f[1000]"
    assert str(f[-1]) == "f[-1]"
    assert str(f[-999]) == "f[-999]"
    assert str(f[""]) == "f['']"
    assert str(f["0"]) == "f['0']"
    assert str(f["A+B"]) == "f['A+B']"
    assert str(f["_A"]) == "f['_A']"
    assert str(f["_54"]) == "f['_54']"
    assert str(f._3_) == "f['_3_']"
    assert str(f.a_b_c) == "f['a_b_c']"
    assert str(f[" y "]) == "f[' y ']"
    assert str(f["a b c"]) == "f['a b c']"


def test_f_col_selector_bound1():
    with f.bind_datatable(dt.Frame()):
        assert f.a.safe_name() == "f_a"
        assert f.abcdefghijkl.safe_name() == "f_abcdefghijkl"
        assert f.abcdefghijklm.safe_name() == "f__" + str(id("abcdefghijklm"))
        assert f[0].safe_name() == "f_0"
        assert f[1].safe_name() == "f_1"
        assert f[-1].safe_name() == "f_1_"
        assert f[1000000].safe_name() == "f_1000000"
        assert f[""].safe_name() == "f__" + str(id(""))
        assert f["0"].safe_name() == "f__" + str(id("0"))
        assert f["A/B"].safe_name() == "f__" + str(id("A/B"))


def test_f_col_selector_bound2():
    dt0 = dt.Frame([[]] * 3, names=["a", "abcdefg", "abcdefghijklm"])
    with f.bind_datatable(dt0):
        assert (f.a).safe_name() == "f_a"
        assert (f.abcdefg).safe_name() == "f_abcdefg"
        assert (f.abcdefghijkl).safe_name() == "f_abcdefghijkl"
        assert (f.abcdefghijklm).safe_name() == "f_2"
        assert (f[0]).safe_name() == "f_a"
        assert (f[1]).safe_name() == "f_abcdefg"
        assert (f[2]).safe_name() == "f_2"  # "f_abcdefgijklm" is too long
        assert (f[3]).safe_name() == "f_3"
        assert (f[-1]).safe_name() == "f_2"
        assert (f[-2]).safe_name() == "f_abcdefg"
        assert (f[-3]).safe_name() == "f_a"
        assert (f[-4]).safe_name() == "f_4_"
        assert (f.BBB).safe_name() == "f_BBB"
        assert (f[""]).safe_name() == "f__" + str(id(""))
        assert (f["0"]).safe_name() == "f__" + str(id("0"))
        assert (f["The End."]).safe_name() == "f__" + str(id("The End."))


def test_f_col_selector_invalid():
    with pytest.raises(TypeError) as e:
        f[2.5]
    assert str(e.value) == ("Column selector should be an integer or a string, "
                            "not <class 'float'>")
    # Note: at some point we may start supporting all the expressions below:
    with pytest.raises(TypeError):
        f[[7, 4]]
    with pytest.raises(TypeError):
        f[("A", "B", "C")]
    with pytest.raises(TypeError):
        f[:3]


def test_f_expressions():
    assert repr(f.C1 < f.C2) == "RelationalOpExpr((f.C1 < f.C2))"
    assert str(f.C1 < f.C2) == "(f.C1 < f.C2)"



#-------------------------------------------------------------------------------
# Unary bitwise NOT (__invert__)
#-------------------------------------------------------------------------------

def inv(t):
    if t is None: return None
    if isinstance(t, bool): return not t
    return ~t


@pytest.mark.parametrize("src", dt_bool + dt_int)
def test_dt_invert(src):
    dt0 = dt.Frame(src)
    df2 = dt0(select=~f[0], engine="eager")
    df2.internal.check()
    assert df2.stypes == dt0.stypes
    assert df2.to_list() == [[inv(x) for x in src]]
    if has_llvm():
        df1 = dt0(select=~f[0], engine="llvm")
        df1.internal.check()
        assert df1.stypes == dt0.stypes
        assert df1.to_list() == [[inv(x) for x in src]]


@pytest.mark.parametrize("src", dt_float)
def test_dt_invert_invalid(src):
    dt0 = dt.Frame(src)
    for engine in ["llvm", "eager"]:
        if engine == "llvm" and not has_llvm():
            continue
        with pytest.raises(TypeError) as e:
            dt0(select=~f[0], engine=engine)
        assert str(e.value) == ("Operator `~` cannot be applied to a `%s` "
                                "column" % dt0.stypes[0].name)



#-------------------------------------------------------------------------------
# Unary minus (__neg__)
#-------------------------------------------------------------------------------

def neg(t):
    if t is None: return None
    return -t


@pytest.mark.parametrize("src", dt_int + dt_float)
def test_dt_neg(src):
    dt0 = dt.Frame(src)
    dtr = dt0(select=lambda f: -f[0])
    dtr.internal.check()
    assert dtr.stypes == dt0.stypes
    assert list_equals(dtr.to_list()[0], [neg(x) for x in src])


@pytest.mark.parametrize("src", dt_bool)
def test_dt_neg_invalid(src):
    dt0 = dt.Frame(src)
    with pytest.raises(TypeError) as e:
        dt0(select=lambda f: -f[0])
    assert str(e.value) == ("Operator `-` cannot be applied to a `%s` column"
                            % dt0.stypes[0].name)



#-------------------------------------------------------------------------------
# Unary plus (__pos__)
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("src", dt_int + dt_float)
def test_dt_pos(src):
    dt0 = dt.Frame(src)
    dtr = dt0(select=lambda f: +f[0])
    dtr.internal.check()
    assert dtr.stypes == dt0.stypes
    assert list_equals(dtr.to_list()[0], list(src))


@pytest.mark.parametrize("src", dt_bool)
def test_dt_pos_invalid(src):
    dt0 = dt.Frame(src)
    with pytest.raises(TypeError) as e:
        dt0(select=lambda f: +f[0])
    assert str(e.value) == ("Operator `+` cannot be applied to a `%s` column"
                            % dt0.stypes[0].name)



#-------------------------------------------------------------------------------
# isna()
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("src", dt_bool + dt_int + dt_float + dt_str)
def test_dt_isna(src):
    from datatable import isna
    dt0 = dt.Frame(src)
    dt1 = dt0[:, isna(f[0])]
    dt1.internal.check()
    assert dt1.stypes == (stype.bool8,)
    pyans = [x is None for x in src]
    assert dt1.to_list()[0] == pyans
    if has_llvm():
        dt2 = dt0(select=lambda f: dt.isna(f[0]), engine="llvm")
        dt2.internal.check()
        assert dt2.stypes == (stype.bool8,)
        assert dt2.to_list()[0] == pyans


def test_dt_isna2():
    from datatable import isna
    from math import nan
    dt0 = dt.Frame(A=[1, None, 2, 5, None, 3.6, nan, -4.899])
    dt1 = dt0[~isna(f.A), :]
    assert dt1.stypes == dt0.stypes
    assert dt1.names == dt0.names
    assert dt1.to_list() == [[1.0, 2.0, 5.0, 3.6, -4.899]]




#-------------------------------------------------------------------------------
# abs()
#-------------------------------------------------------------------------------

def test_abs():
    from datatable import abs
    assert abs(1) == 1
    assert abs(-5) == 5
    assert abs(-2.5e12) == 2.5e12
    assert abs(None) is None


@pytest.mark.parametrize("src", dt_int + dt_float)
def test_abs_srcs(src):
    dt0 = dt.Frame(src)
    dt1 = dt.abs(dt0)
    dt1.internal.check()
    assert dt0.stypes == dt1.stypes
    pyans = [None if x is None else abs(x) for x in src]
    assert dt1.to_list()[0] == pyans


def test_abs_all_stypes():
    from datatable import abs
    src = [[-127, -5, -1, 0, 2, 127],
           [-32767, -299, -7, 32767, 12, -543],
           [-2147483647, -1000, 3, -589, 2147483647, 0],
           [-2**63 + 1, 2**63 - 1, 0, -2**32, 2**32, -793]]
    dt0 = dt.Frame(src, stypes=[dt.int8, dt.int16, dt.int32, dt.int64])
    dt1 = dt0[:, [abs(f[i]) for i in range(4)]]
    dt1.internal.check()
    assert dt1.to_list() == [[abs(x) for x in col] for col in src]



#-------------------------------------------------------------------------------
# exp()
#-------------------------------------------------------------------------------

def test_exp():
    from datatable import exp
    import math
    assert exp(0) == math.exp(0)
    assert exp(1) == math.exp(1)
    assert exp(-2.5e12) == math.exp(-2.5e12)
    assert exp(12345678) == math.inf
    assert exp(None) is None


@pytest.mark.parametrize("src", dt_int + dt_float)
def test_exp_srcs(src):
    from math import exp, inf
    dt0 = dt.Frame(src)
    dt1 = dt.exp(dt0)
    dt1.internal.check()
    assert all([st == stype.float64 for st in dt1.stypes])
    pyans = []
    for x in src:
        if x is None:
            pyans.append(None)
        else:
            try:
                pyans.append(exp(x))
            except OverflowError:
                pyans.append(inf)
    assert dt1.to_list()[0] == pyans


def test_exp_all_stypes():
    from datatable import exp
    import math
    src = [[-127, -5, -1, 0, 2, 127],
           [-32767, -299, -7, 32767, 12, -543],
           [-2147483647, -1000, 3, -589, 2147483647, 0],
           [-2 ** 63 + 1, 2 ** 63 - 1, 0, -2 ** 32, 2 ** 32, -793]]
    dt0 = dt.Frame(src, stypes=[dt.int8, dt.int16, dt.int32, dt.int64])
    dt1 = dt0[:, [exp(f[i]) for i in range(4)]]
    dt1.internal.check()
    pyans = []
    for col in src:
        l = []
        for x in col:
            if x is None:
                l.append(None)
            else:
                try:
                    l.append(math.exp(x))
                except OverflowError:
                    l.append(math.inf)
        pyans.append(l)
    assert dt1.to_list() == pyans



#-------------------------------------------------------------------------------
# type-cast
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("src", dt_bool + dt_int + dt_float)
def test_cast_to_float32(src):
    dt0 = dt.Frame(src)
    dt1 = dt0[:, [dt.float32(f[i]) for i in range(dt0.ncols)]]
    dt1.internal.check()
    assert dt1.stypes == (dt.float32,) * dt0.ncols
    pyans = [float(x) if x is not None else None for x in src]
    assert list_equals(dt1.to_list()[0], pyans)


@pytest.mark.parametrize("stype0", ltype.int.stypes)
def test_cast_int_to_str(stype0):
    dt0 = dt.Frame([None, 0, -3, 189, 77, 14, None, 394831, -52939047130424957],
                   stype=stype0)
    dt1 = dt0[:, [dt.str32(f.C0), dt.str64(f.C0)]]
    dt1.internal.check()
    assert dt1.stypes == (dt.str32, dt.str64)
    assert dt1.shape == (dt0.nrows, 2)
    ans = [None if v is None else str(v)
           for v in dt0.to_list()[0]]
    assert dt1.to_list()[0] == ans


@pytest.mark.parametrize("src", dt_bool + dt_int + dt_float + dt_obj)
def test_cast_to_str(src):
    def to_str(x):
        if x is None: return None
        if isinstance(x, bool): return str(int(x))
        # if isinstance(x, float) and math.isnan(x): return None
        return str(x)

    dt0 = dt.Frame(src)
    dt1 = dt0[:, [dt.str32(f[i]) for i in range(dt0.ncols)]]
    dt2 = dt0[:, [dt.str64(f[i]) for i in range(dt0.ncols)]]
    dt1.internal.check()
    dt2.internal.check()
    assert dt1.stypes == (dt.str32,) * dt0.ncols
    assert dt2.stypes == (dt.str64,) * dt0.ncols
    assert dt1.to_list()[0] == [to_str(x) for x in src]


def test_cast_view():
    df0 = dt.Frame({"A": [1, 2, 3]})
    df1 = df0[::-1, :][:, dt.float32(f.A)]
    df1.internal.check()
    assert df1.stypes == (dt.float32,)
    assert df1.to_list() == [[3.0, 2.0, 1.0]]



#-------------------------------------------------------------------------------
# logical ops
#-------------------------------------------------------------------------------

def test_logical_and1():
    src1 = [1, 5, 12, 3, 7, 14]
    src2 = [1, 2] * 3
    ans = [i for i in range(6)
           if src1[i] < 10 and src2[i] == 1]

    df0 = dt.Frame(A=src1, B=src2)
    df1 = df0[(f.A < 10) & (f.B == 1), [f.A, f.B]]
    assert df1.to_list() == [[src1[i] for i in ans],
                             [src2[i] for i in ans]]


def test_logical_or1():
    src1 = [1, 5, 12, 3, 7, 14]
    src2 = [1, 2] * 3
    ans = [i for i in range(6)
           if src1[i] < 10 or src2[i] == 1]

    df0 = dt.Frame(A=src1, B=src2)
    df1 = df0[(f.A < 10) | (f.B == 1), [f.A, f.B]]
    assert df1.to_list() == [[src1[i] for i in ans],
                             [src2[i] for i in ans]]


@pytest.mark.parametrize("seed", [random.getrandbits(63)])
def test_logical_and2(seed):
    random.seed(seed)
    n = 1000
    src1 = [random.choice([True, False, None]) for _ in range(n)]
    src2 = [random.choice([True, False, None]) for _ in range(n)]

    df0 = dt.Frame(A=src1, B=src2)
    df1 = df0[:, f.A & f.B]
    assert df1.to_list()[0] == \
        [None if (src1[i] is None or src2[i] is None) else
         src1[i] and src2[i]
         for i in range(n)]


@pytest.mark.parametrize("seed", [random.getrandbits(63)])
def test_logical_or2(seed):
    random.seed(seed)
    n = 1000
    src1 = [random.choice([True, False, None]) for _ in range(n)]
    src2 = [random.choice([True, False, None]) for _ in range(n)]

    df0 = dt.Frame(A=src1, B=src2)
    df1 = df0[:, f.A | f.B]
    assert df1.to_list()[0] == \
        [None if (src1[i] is None or src2[i] is None) else
         src1[i] or src2[i]
         for i in range(n)]



#-------------------------------------------------------------------------------
# Equality operators
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("st1, st2", [(dt.str32, dt.str32),
                                      (dt.str32, dt.str64),
                                      (dt.str64, dt.str32),
                                      (dt.str64, dt.str64)])
def test_equal_strings(st1, st2):
    df0 = dt.Frame([["foo", "gowebrt", None, "afdw", "req",  None],
                    ["foo", "goqer",   None, "afdw", "req?", ""]],
                   names=["A", "B"], stypes=[st1, st2])
    assert df0.stypes == (st1, st2)
    df1 = df0[:, f.A == f.B]
    assert df1.stypes == (dt.bool8,)
    assert df1.to_list() == [[True, False, True, True, False, False]]
    df2 = df0[:, f.A != f.B]
    assert df2.stypes == (dt.bool8,)
    assert df2.to_list() == [[False, True, False, False, True, True]]




#-------------------------------------------------------------------------------
# Division
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("seed", [random.getrandbits(63)])
def test_div_mod(seed):
    random.seed(seed)
    n = 1000
    src1 = [random.randint(-100, 100) for _ in range(n)]
    src2 = [random.randint(-10, 10) for _ in range(n)]

    df0 = dt.Frame(x=src1, y=src2)
    df1 = df0[:, [f.x // f.y, f.x % f.y]]
    assert df1.to_list() == [
        [None if src2[i] == 0 else src1[i] // src2[i] for i in range(n)],
        [None if src2[i] == 0 else src1[i] % src2[i] for i in range(n)]
    ]



#-------------------------------------------------------------------------------
# Misc
#-------------------------------------------------------------------------------

def test_expr_reuse():
    # Check that an expression can later be used in another selector
    # In particular, we first apply `expr` to `df0` (where A is the second
    # column). If `expr` were to memorize that "A" maps to the second column,
    # it would produce an error when applied to the second Frame which has
    # only 1 column. See #1366.
    expr = f.A < 1
    df0 = dt.Frame([range(5), range(5)], names=["B", "A"])
    df1 = df0[:, {"A": expr}]
    df1.internal.check()
    assert df1.names == ("A", )
    assert df1.stypes == (stype.bool8, )
    assert df1.to_list() == [[True, False, False, False, False]]
    df2 = df1[:, expr]
    df2.internal.check()
    assert df2.to_list() == [[False, True, True, True, True]]
