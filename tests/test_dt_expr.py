#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import pytest
import random
import datatable as dt
from datatable import f, stype, ltype
from tests import list_equals


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
          [None, "", " ", "  ", None, "\0"],
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
    assert repr(f.a) == "ColSelectorExpr(f_a)"
    assert str(f.a) == "f_a"
    assert str(f.abcdefghijkl) == "f_abcdefghijkl"
    assert str(f.abcdefghijklm) == "f__" + str(id("abcdefghijklm"))
    assert str(f[0]) == "f_0"
    assert str(f[1000]) == "f_1000"
    assert str(f[-1]) == "f_1_"
    assert str(f[-999]) == "f_999_"
    assert str(f[""]) == "f__" + str(id(""))
    assert str(f["0"]) == "f__" + str(id("0"))
    assert str(f["A+B"]) == "f__" + str(id("A+B"))
    assert str(f["_A"]) == "f__" + str(id("_A"))
    assert str(f["_54"]) == "f__" + str(id("_54"))
    assert str(f._3_) == "f__" + str(id("_3_"))
    assert str(f.a_b_c) == "f__" + str(id("a_b_c"))
    assert str(f[" y "]) == "f__" + str(id(" y "))
    assert str(f["a b c"]) == str(f["a b c"])
    assert str(f["a b c"]) != str(f[" ".join("abc")])


def test_f_col_selector_bound1():
    with f.bind_datatable(dt.Frame()):
        assert str(f.a) == "f_a"
        assert str(f.abcdefghijkl) == "f_abcdefghijkl"
        assert str(f.abcdefghijklm) == "f__" + str(id("abcdefghijklm"))
        assert str(f[0]) == "f_0"
        assert str(f[1]) == "f_1"
        assert str(f[-1]) == "f_1_"
        assert str(f[1000000]) == "f_1000000"
        assert str(f[""]) == "f__" + str(id(""))
        assert str(f["0"]) == "f__" + str(id("0"))
        assert str(f["A/B"]) == "f__" + str(id("A/B"))


def test_f_col_selector_bound2():
    dt0 = dt.Frame([[]] * 3, names=["a", "abcdefg", "abcdefghijklm"])
    with f.bind_datatable(dt0):
        assert str(f.a) == "f_a"
        assert str(f.abcdefg) == "f_abcdefg"
        assert str(f.abcdefghijkl) == "f_abcdefghijkl"
        assert str(f.abcdefghijklm) == "f_2"
        assert str(f[0]) == "f_a"
        assert str(f[1]) == "f_abcdefg"
        assert str(f[2]) == "f_2"  # "f_abcdefgijklm" is too long
        assert str(f[3]) == "f_3"
        assert str(f[-1]) == "f_2"
        assert str(f[-2]) == "f_abcdefg"
        assert str(f[-3]) == "f_a"
        assert str(f[-4]) == "f_4_"
        assert str(f.BBB) == "f_BBB"
        assert str(f[""]) == "f__" + str(id(""))
        assert str(f["0"]) == "f__" + str(id("0"))
        assert str(f["The End."]) == "f__" + str(id("The End."))


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
    assert repr(f.C1 < f.C2) == "RelationalOpExpr((f_C1 < f_C2))"
    assert str(f.C1 < f.C2) == "(f_C1 < f_C2)"



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
    df1 = dt0(select=~f[0], engine="llvm")
    df2 = dt0(select=~f[0], engine="eager")
    df1.internal.check()
    df2.internal.check()
    assert df1.stypes == dt0.stypes
    assert df2.stypes == dt0.stypes
    assert df1.topython() == [[inv(x) for x in src]]
    assert df2.topython() == [[inv(x) for x in src]]


@pytest.mark.parametrize("src", dt_float)
def test_dt_invert_invalid(src):
    dt0 = dt.Frame(src)
    for engine in ["llvm", "eager"]:
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
    assert list_equals(dtr.topython()[0], [neg(x) for x in src])


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
    assert list_equals(dtr.topython()[0], list(src))


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
    dt0 = dt.Frame(src)
    dt1 = dt0(select=lambda f: dt.isna(f[0]), engine="eager")
    dt2 = dt0(select=lambda f: dt.isna(f[0]), engine="llvm")
    dt1.internal.check()
    dt2.internal.check()
    assert dt1.stypes == dt2.stypes == (stype.bool8,)
    pyans = [x is None for x in src]
    assert dt1.topython()[0] == pyans
    assert dt2.topython()[0] == pyans



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
    assert dt1.topython()[0] == pyans


def test_abs_all_stypes():
    from datatable import abs
    src = [[-127, -5, -1, 0, 2, 127],
           [-32767, -299, -7, 32767, 12, -543],
           [-2147483647, -1000, 3, -589, 2147483647, 0],
           [-2**63 + 1, 2**63 - 1, 0, -2**32, 2**32, -793]]
    dt0 = dt.Frame(src, stypes=[dt.int8, dt.int16, dt.int32, dt.int64])
    dt1 = dt0[:, [abs(f[i]) for i in range(4)]]
    dt1.internal.check()
    assert dt1.topython() == [[abs(x) for x in col] for col in src]



#-------------------------------------------------------------------------------
# exp()
#-------------------------------------------------------------------------------

def test_exp():
    from datatable import exp
    import cmath
    assert exp(0) == cmath.exp(0)
    assert exp(1) == cmath.exp(1)
    assert exp(-2.5e12) == cmath.exp(-2.5e12)
    assert exp(12345678) == math.inf
    assert exp(None) is None


@pytest.mark.parametrize("src", dt_int + dt_float)
def test_exp_srcs(src):
    from cmath import exp
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
                pyans.append(float("inf"))
    assert dt1.topython()[0] == pyans


def test_exp_all_stypes():
    from datatable import exp
    import cmath
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
                    l.append(cmath.exp(x))
                except OverflowError:
                    l.append(float("inf"))
        pyans.append(l)
    assert dt1.topython() == pyans



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
    assert list_equals(dt1.topython()[0], pyans)


@pytest.mark.parametrize("stype0", ltype.int.stypes)
def test_cast_int_to_str(stype0):
    dt0 = dt.Frame([None, 0, -3, 189, 77, 14, None, 394831, -52939047130424957],
                   stype=stype0)
    dt1 = dt0[:, [dt.str32(f.C0), dt.str64(f.C0)]]
    dt1.internal.check()
    assert dt1.stypes == (dt.str32, dt.str64)
    assert dt1.shape == (dt0.nrows, 2)
    ans = [None if v is None else str(v)
           for v in dt0.topython()[0]]
    assert dt1.topython()[0] == ans


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
    assert dt1.topython()[0] == [to_str(x) for x in src]


def test_cast_view():
    df0 = dt.Frame({"A": [1, 2, 3]})
    df1 = df0[::-1, :][:, dt.float32(f.A)]
    df1.internal.check()
    assert df1.stypes == (dt.float32,)
    assert df1.topython() == [[3.0, 2.0, 1.0]]



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
    assert df1.topython() == [[src1[i] for i in ans],
                              [src2[i] for i in ans]]


def test_logical_or1():
    src1 = [1, 5, 12, 3, 7, 14]
    src2 = [1, 2] * 3
    ans = [i for i in range(6)
           if src1[i] < 10 or src2[i] == 1]

    df0 = dt.Frame(A=src1, B=src2)
    df1 = df0[(f.A < 10) | (f.B == 1), [f.A, f.B]]
    assert df1.topython() == [[src1[i] for i in ans],
                              [src2[i] for i in ans]]


@pytest.mark.parametrize("seed", [random.getrandbits(63)])
def test_logical_and2(seed):
    random.seed(seed)
    n = 1000
    src1 = [random.choice([True, False, None]) for _ in range(n)]
    src2 = [random.choice([True, False, None]) for _ in range(n)]

    df0 = dt.Frame(A=src1, B=src2)
    df1 = df0[:, f.A & f.B]
    assert df1.topython()[0] == \
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
    assert df1.topython()[0] == \
        [None if (src1[i] is None or src2[i] is None) else
         src1[i] or src2[i]
         for i in range(n)]



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
    assert df1.topython() == [
        [None if src2[i] == 0 else src1[i] // src2[i] for i in range(n)],
        [None if src2[i] == 0 else src1[i] % src2[i] for i in range(n)]
    ]
