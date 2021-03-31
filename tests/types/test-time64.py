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
import datetime
import pytest
import random
from datatable import dt, f
from tests import assert_equals



def test_time64_name():
    assert repr(dt.Type.time64) == "Type.time64"
    assert dt.Type.time64.name == "time64"


def test_time64_type_from_basic():
    assert dt.Type("time") == dt.Type.time64
    assert dt.Type("time64") == dt.Type.time64
    assert dt.Type(datetime.datetime) == dt.Type.time64


def test_time64_type_from_numpy(np):
    assert dt.Type(np.dtype("datetime64[s]")) == dt.Type.time64
    assert dt.Type(np.dtype("datetime64[ms]")) == dt.Type.time64
    assert dt.Type(np.dtype("datetime64[us]")) == dt.Type.time64
    assert dt.Type(np.dtype("datetime64[ns]")) == dt.Type.time64


def test_time64_type_from_pyarrow(pa):
    # pyarrow.date64: ms since unix epoch
    assert dt.Type(pa.date64()) == dt.Type.time64
