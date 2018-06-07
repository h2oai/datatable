#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import pytest
import datatable as dt
from datatable import f, stype, ltype
from tests import list_equals


# Sets of tuples containing test columns of each type
dt_bool = {(False, True, False, False, True),
           (True, None, None, True, False)}

dt_int = {(5, -3, 6, 3, 0),
          (None, -1, 0, 26, -3),
          # TODO: currently ~ operation fails on v = 2**31 - 1. Should we
          #       promote the resulting column to int64 in such case?
          (2**31 - 2, -(2**31 - 1), 0, -1, 1)}

dt_float = {(9.5, 0.2, 5.4857301, -3.14159265338979),
            (1.1, 2.3e12, -.5, None, float("inf"), 0.0)}

dt_str = {("foo", "bbar", "baz"),
          (None, "", " ", "  ", None, "\0"),
          tuple("qwertyuiiop[]asdfghjkl;'zxcvbnm,./`1234567890-=")}


#-------------------------------------------------------------------------------
# f
#-------------------------------------------------------------------------------

def test_f():
    # Check that unbounded f-expressions can be stringified (see #1024). The
    # exact representation may be modified in the future; however f-expressions
    # should not raise exceptions when printed.
    x = f.a
    assert repr(x) == "ColSelectorExpr(f_a)"
    assert str(x) == "f_a"
    y = f.C1 < f.C2
    assert repr(y) == "RelationalOpExpr((f_C1 < f_C2))"
    assert str(y) == "(f_C1 < f_C2)"
    z = f[0]
    assert str(z) == "f_0"
    assert repr(z) == "ColSelectorExpr(f_0)"



#-------------------------------------------------------------------------------
# Unary bitwise NOT (__invert__)
#-------------------------------------------------------------------------------

def inv(t):
    if t is None: return None
    if isinstance(t, bool): return not t
    return ~t


@pytest.mark.parametrize("src", dt_bool | dt_int)
def test_dt_invert(src):
    dt0 = dt.Frame(src)
    df1 = dt0(select=~f[0], engine="llvm")
    df2 = dt0(select=~f[0], engine="eager")
    assert df1.internal.check()
    assert df2.internal.check()
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


@pytest.mark.parametrize("src", dt_int | dt_float)
def test_dt_neg(src):
    dt0 = dt.Frame(src)
    dtr = dt0(select=lambda f: -f[0])
    assert dtr.internal.check()
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

@pytest.mark.parametrize("src", dt_int | dt_float)
def test_dt_pos(src):
    dt0 = dt.Frame(src)
    dtr = dt0(select=lambda f: +f[0])
    assert dtr.internal.check()
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

@pytest.mark.parametrize("src", dt_bool | dt_int | dt_float | dt_str)
def test_dt_isna(src):
    dt0 = dt.Frame(src)
    dt1 = dt0(select=lambda f: dt.isna(f[0]), engine="eager")
    dt2 = dt0(select=lambda f: dt.isna(f[0]), engine="llvm")
    assert dt1.internal.check()
    assert dt2.internal.check()
    assert dt1.stypes == dt2.stypes == (stype.bool8,)
    pyans = [x is None for x in src]
    assert dt1.topython()[0] == pyans
    assert dt2.topython()[0] == pyans



#-------------------------------------------------------------------------------
# type-cast
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("src", dt_bool | dt_int | dt_float)
def test_cast_to_float32(src):
    dt0 = dt.Frame(src)
    dt1 = dt0[:, [dt.float32(f[i]) for i in range(dt0.ncols)]]
    assert dt1.internal.check()
    assert dt1.stypes == (dt.float32,) * dt0.ncols
    pyans = [float(x) if x is not None else None for x in src]
    assert list_equals(dt1.topython()[0], pyans)


@pytest.mark.parametrize("stype0", ltype.int.stypes)
def test_cast_int_to_str(stype0):
    dt0 = dt.Frame([None, 0, -3, 189, 77, 14, None, 394831, -52939047130424957],
                   stype=stype0)
    dt1 = dt0[:, [dt.str32(f.C0), dt.str64(f.C0)]]
    assert dt1.internal.check()
    assert dt1.stypes == (dt.str32, dt.str64)
    assert dt1.shape == (dt0.nrows, 2)
    ans = [None if v is None else str(v)
           for v in dt0.topython()[0]]
    assert dt1.topython()[0] == ans


@pytest.mark.parametrize("src", dt_bool | dt_int | dt_float)
def test_cast_to_str(src):
    def to_str(x):
        if x is None: return None
        if isinstance(x, bool): return str(int(x))
        # if isinstance(x, float) and math.isnan(x): return None
        return str(x)

    dt0 = dt.Frame(src)
    dt1 = dt0[:, [dt.str32(f[i]) for i in range(dt0.ncols)]]
    dt2 = dt0[:, [dt.str64(f[i]) for i in range(dt0.ncols)]]
    assert dt1.internal.check()
    assert dt2.internal.check()
    assert dt1.stypes == (dt.str32,) * dt0.ncols
    assert dt2.stypes == (dt.str64,) * dt0.ncols
    assert dt1.topython()[0] == [to_str(x) for x in src]


def test_cast_view():
    df0 = dt.Frame({"A": [1, 2, 3]})
    df1 = df0[::-1, :][:, dt.float32(f.A)]
    assert df1.internal.check()
    assert df1.stypes == (dt.float32,)
    assert df1.topython() == [[3.0, 2.0, 1.0]]
