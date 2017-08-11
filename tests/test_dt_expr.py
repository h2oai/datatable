#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import pytest
import datatable as dt
from tests import list_equals


# Sets of tuples containing test columns of each type
dt_bool = {(False, True, False, False, True),
           (True, None, None, True, False)}

dt_int = {(5, -3, 6, 3, 0),
          (None, -1, 0, 26, -3),
          # TODO: currently ~ operation fails on v = 2**31 - 1. Should we
          #       promote the resulting column to i8i in such case?
          (2**31 - 2, -(2**31 - 1), 0, -1, 1)}

dt_float = {(9.5, 0.2, 5.4857301, -3.14159265338979),
            (1.1, 2.3e12, -.5, None, float("inf"), 0.0)}



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

@pytest.mark.parametrize("src", dt_bool | dt_int | dt_float)
def test_dt_isna(src):
    dt0 = dt.DataTable(src)
    dtr = dt0(select=lambda f: dt.isna(f[0]))
    assert dtr.internal.check()
    assert dtr.stypes == ("i1b",)
    assert dtr.topython()[0] == [x is None for x in src]
