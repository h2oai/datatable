#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2020 H2O.ai
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
import sys
import textwrap
from datatable.internal import frame_integrity_check


def assert_equals(frame1, frame2):
    """
    Helper function to assert that 2 frames are equal to each other.
    """
    frame_integrity_check(frame1)
    frame_integrity_check(frame2)
    assert frame1.shape == frame2.shape, (
        "The left frame has shape %r, while the right has shape %r"
        % (frame1.shape, frame2.shape))

    if sys.version_info >= (3, 6):
        assert frame1.names == frame2.names, (
            "The left frame has names %r, while the right has names %r"
            % (frame1.names, frame2.names))
        assert frame1.stypes == frame2.stypes, (
            "The left frame has stypes %r, while the right has stypes %r"
            % (frame1.stypes, frame2.stypes))
        data1 = frame1.to_list()
        data2 = frame2.to_list()
        assert len(data1) == len(data2)  # shape check should ensure this
        for i in range(len(data1)):
            col1 = data1[i]
            col2 = data2[i]
            assert len(col1) == len(col2)
            for j in range(len(col1)):
                val1 = col1[j]
                val2 = col2[j]
                if val1 == val2: continue
                if isinstance(val1, float) and isinstance(val2, float):
                    if math.isclose(val1, val2): continue
                if len(col1) > 16:
                    arr1 = repr(col1[:16])[:-1] + ", ...]"
                    arr2 = repr(col2[:16])[:-1] + ", ...]"
                else:
                    arr1 = repr(col1)
                    arr2 = repr(col2)
                raise AssertionError(
                    "The frames have different data in column %d `%s` at "
                    "index %d: LHS has %r, and RHS has %r\n"
                    "  LHS = %s\n"
                    "  RHS = %s\n"
                    % (i, frame1.names[i], j, val1, val2, arr1, arr2))
    else:
        assert frame1.shape == frame2.shape
        assert same_iterables(frame1.names, frame2.names)
        assert same_iterables(frame1.stypes, frame2.stypes)
        assert same_iterables(frame1.to_list(), frame2.to_list())



def repr_row(row, j):
    assert 0 <= j < len(row)
    if len(row) <= 20 or j <= 12:
        out = str(row[:j])[:-1]
        if j > 0:
            out += ",   " + str(row[j])
        if len(row) > 20:
            out += ",   " + str(row[j+1:20])[1:-1] + ", ...]"
        elif j < len(row) - 1:
            out += ",   " + str(row[j+1:])[1:]
        else:
            out += "]"
    else:
        out = str(row[:10])[:-1] + ", ..., " + str(row[j]) + ", ...]"
    return out



def repr_slice(s):
    if s == slice(None):
        return ":"
    if s.start is None:
        if s.stop is None:
            return "::" +  str(s.step)
        elif s.step is None:
            return ":" + str(s.stop)
        else:
            return ":" + str(s.stop) + ":" + str(s.step)
    else:
        if s.stop is None:
            return str(s.start) + "::" +  str(s.step)
        elif s.step is None:
            return str(s.start) + ":" + str(s.stop)
        else:
            return str(s.start) + ":" + str(s.stop) + ":" + str(s.step)


def repr_data(data, indent):
    out = "["
    for i, arr in enumerate(data):
        if i > 0:
            out += " " * (indent + 1)
        out += "["
        out += textwrap.fill(", ".join(repr(e) for e in arr),
                             width = 80, subsequent_indent=" "*(indent+2))
        out += "]"
        if i != len(data) - 1:
            out += ",\n"
    out += "]"
    return out


def repr_type(t):
    if t is int:
        return "int"
    if t is bool:
        return "bool"
    if t is float:
        return "float"
    if t is str:
        return "str"
    raise ValueError("Unknown type %r" % t)

def repr_types(types):
    assert isinstance(types, (list, tuple))
    return "[" + ", ".join(repr_type(t) for t in types) + "]"
