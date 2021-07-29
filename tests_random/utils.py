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
import random
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
        assert list_equals(frame1.names, frame2.names)
        assert list_equals(frame1.stypes, frame2.stypes)
        assert list_equals(frame1.to_list(), frame2.to_list())


def traced(fn):
    def wrapper(self, *args, **kwds):
        sargs = ", ".join([repr(arg) for arg in args] +
                          ['%s=%r' % kw for kw in kwds.items()])
        print(f"{self}.{fn.__name__}({sargs})")
        fn(self, *args, **kwds)

    return wrapper



#-------------------------------------------------------------------------------
# Repr helpers
#-------------------------------------------------------------------------------

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
                             width=160, subsequent_indent=" "*(indent+2))
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
    if t is None:
        return "'void'"
    raise ValueError("Unknown type %r" % t)

def repr_types(types):
    assert isinstance(types, (list, tuple))
    return "[" + ", ".join(repr_type(t) for t in types) + "]"



#-------------------------------------------------------------------------------
# Random helpers
#-------------------------------------------------------------------------------

def random_slice(n):
    while True:
        t = random.random()
        s = random.random()
        i0 = int(n * t * 0.2) if t > 0.5 else None
        i1 = int(n * (s * 0.2 + 0.8)) if s > 0.5 else None
        if i0 is not None and i1 is not None and i1 < i0:
            i0, i1 = i1, i0
        step = random.choice([-2, -1, -1, 1, 1, 2, 2, 2, 3, 3, 4, 5, 7] +
                             [None] * 3)
        if step is not None and step < 0:
            i0, i1 = i1, i0
        res = slice(i0, i1, step)
        newn = len(range(*res.indices(n)))
        if newn > 0 or n == 0:
            return res


def random_array(n, positive=False):
    assert n > 0
    newn = max(5, random.randint(n // 2, 3 * n // 2))
    lb = 0 if positive else -n
    ub = n - 1
    return [random.randint(lb, ub) for i in range(newn)]


def random_type():
    return random.choice([bool, int, float, str]*2 + [None])


def random_names(ncols):
    t = random.random()
    if t < 0.8:
        alphabet = "abcdefghijklmnopqrstuvwxyz"
        if t < 0.5:
            alphabet = alphabet * 2 + "0123456789"
        elif t < 0.6:
            # chr(76) is a "`" which doesn't reproduce reliably in
            # error messages
            alphabet += "".join(chr(i) for i in range(32, 127) if i != 96)
        else:
            alphabet = (alphabet * 40 + "0123456789" * 10 +
                        "".join(chr(x) for x in range(192, 448)))
        while True:
            # Ensure uniqueness of the returned names
            names = [random_string(alphabet) for _ in range(ncols)]
            if len(set(names)) == ncols:
                return names
    else:
        c = chr(ord('A') + random.randint(0, 25))
        return ["%s%d" % (c, i) for i in range(ncols)]


def random_string(alphabet="abcdefghijklmnopqrstuvwxyz"):
    n = int(random.expovariate(0.2) + 1.5)
    while True:
        name = "".join(random.choice(alphabet) for _ in range(n))
        if not(name.isdigit() or name.isspace()):
            return name



def random_column(nrows, ttype, missing_fraction, missing_nones=True):
    missing_mask = [False] * nrows
    if ttype == bool:
        data = random_bool_column(nrows)
    elif ttype == int:
        data = random_int_column(nrows)
    elif ttype == float:
        data = random_float_column(nrows)
    elif ttype == str:
        data = random_str_column(nrows)
    elif ttype is None:
        data = [None] * nrows
    else:
        assert False, "Invalid ttype = %s" % ttype
    if missing_fraction:
        for i in range(nrows):
            if random.random() < missing_fraction:
                missing_mask[i] = True

    if missing_nones:
        for i in range(nrows):
            if missing_mask[i]: data[i] = None

    return data, missing_mask


def random_bool_column(nrows):
    t = random.random()  # fraction of 1s
    return [random.random() < t for _ in range(nrows)]


def random_int_column(nrows):
    t = random.random()
    s = t * 10 - int(t * 10)   # 0...1
    q = t * 1000 - int(t * 1000)
    if t < 0.1:
        r0, r1 = -int(10 + s * 20), int(60 + s * 100)
    elif t < 0.3:
        r0, r1 = 0, int(60 + s * 100)
    elif t < 0.6:
        r0, r1 = 0, int(t * 1000000 + 500000)
    elif t < 0.8:
        r1 = int(t * 100000 + 30000)
        r0 = -r1
    elif t < 0.9:
        r1 = (1 << 63) - 1
        r0 = -r1
    else:
        r0, r1 = 0, (1 << 63) - 1
    data = [random.randint(r0, r1) for _ in range(nrows)]
    if q < 0.2:
        fraction = 0.5 + q * 2
        for i in range(nrows):
            if random.random() < fraction:
                data[i] = 0
    return data


def random_float_column(nrows):
    scale = random.expovariate(0.3) + 1
    return [random.random() * scale
            for _ in range(nrows)]


def random_str_column(nrows):
    t = random.random()
    if t < 0.2:
        words = ["cat", "dog", "mouse", "squirrel", "whale",
                 "ox", "ant", "badger", "eagle", "snail"][:int(2 + t * 50)]
        return [random.choice(words) for _ in range(nrows)]
    else:
        return [random_string() for _ in range(nrows)]
