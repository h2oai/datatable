#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import pytest
import datatable as dt
from datatable import stype
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
# Unary bitwise NOT (__invert__)
#-------------------------------------------------------------------------------

def inv(t):
    if t is None: return None
    if isinstance(t, bool): return not t
    return ~t


@pytest.mark.parametrize("src", dt_bool | dt_int)
def test_dt_invert(src):
    dt0 = dt.DataTable(src)
    dtr = dt0(select=lambda f: ~f[0])
    assert dtr.internal.check()
    assert dtr.stypes == dt0.stypes
    assert dtr.topython() == [[inv(x) for x in src]]


@pytest.mark.parametrize("src", dt_float)
def test_dt_invert_invalid(src):
    dt0 = dt.DataTable(src)
    with pytest.raises(TypeError) as e:
        dt0(select=lambda f: ~f[0])
    assert str(e.value) == ("Operation ~ not allowed on operands of type %s"
                            % dt0.stypes[0])



#-------------------------------------------------------------------------------
# Unary minus (__neg__)
#-------------------------------------------------------------------------------

def neg(t):
    if t is None: return None
    return -t


@pytest.mark.parametrize("src", dt_int | dt_float)
def test_dt_neg(src):
    dt0 = dt.DataTable(src)
    dtr = dt0(select=lambda f: -f[0])
    assert dtr.internal.check()
    assert dtr.stypes == dt0.stypes
    assert list_equals(dtr.topython()[0], [neg(x) for x in src])


@pytest.mark.parametrize("src", dt_bool)
def test_dt_neg_invalid(src):
    dt0 = dt.DataTable(src)
    with pytest.raises(TypeError) as e:
        dt0(select=lambda f: -f[0])
    assert str(e.value) == ("Operation - not allowed on operands of type %s"
                            % dt0.stypes[0])



#-------------------------------------------------------------------------------
# Unary plus (__pos__)
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("src", dt_int | dt_float)
def test_dt_pos(src):
    dt0 = dt.DataTable(src)
    dtr = dt0(select=lambda f: +f[0])
    assert dtr.internal.check()
    assert dtr.stypes == dt0.stypes
    assert list_equals(dtr.topython()[0], list(src))


@pytest.mark.parametrize("src", dt_bool)
def test_dt_pos_invalid(src):
    dt0 = dt.DataTable(src)
    with pytest.raises(TypeError) as e:
        dt0(select=lambda f: +f[0])
    assert str(e.value) == ("Operation + not allowed on operands of type %s"
                            % dt0.stypes[0])



#-------------------------------------------------------------------------------
# isna()
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("src", dt_bool | dt_int | dt_float | dt_str)
def test_dt_isna(src):
    dt0 = dt.DataTable(src)
    dt1 = dt0(select=lambda f: dt.isna(f[0]), engine="eager")
    dt2 = dt0(select=lambda f: dt.isna(f[0]), engine="llvm")
    assert dt1.internal.check()
    assert dt2.internal.check()
    assert dt1.stypes == dt2.stypes == (stype.bool8,)
    pyans = [x is None for x in src]
    assert dt1.topython()[0] == pyans
    assert dt2.topython()[0] == pyans
