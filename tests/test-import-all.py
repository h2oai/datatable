#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2019 H2O.ai
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
# This file tests which symbols are exported from the 'datatable'
# module via 'import *'.
# This has to be a separate file, because `import *` is only
# possible at a module level.
#
# In particular, we also test that we DO NOT overwrite built-in
# symbols.
#-------------------------------------------------------------------------------
from datatable import *


def test_dt():
    assert dt
    assert str(type(dt)) == "<class 'module'>"
    assert dt.__name__ == "datatable"


def test_common_symbols():
    assert __version__
    assert options
    assert Frame
    assert fread
    assert f
    assert g
    assert join
    assert by
    assert dt.open != open
    assert dt.TypeError != TypeError
    assert dt.ValueError != ValueError


def test_reducers():
    assert dt.max != max
    assert dt.min != min
    assert dt.sum != sum
    assert mean
    assert median
    assert sd
    assert count
    assert first
    assert last


def test_stypes():
    assert stype
    assert ltype
    assert bool8
    assert int8
    assert int16
    assert int32
    assert int64
    assert float32
    assert float64
    assert str32
    assert str64
    assert obj64


def test_ufuncs():
    assert dt.abs != abs
    assert exp
    assert isna
    assert log
    assert log10


def test_dt_methods():
    assert cbind
    assert rbind
    assert repeat
    assert sort
    assert unique
    assert union
    assert intersect
    assert setdiff
    assert symdiff
    assert split_into_nhot


def test_builtins_all():
    """Check that no builtin variable was exported from datatable."""
    import builtins
    global_vars = [v for v in globals().keys()
                   if not v.startswith('__')]
    builtin_vars = dir(builtins)
    common = set(global_vars) & set(builtin_vars)
    assert not common
