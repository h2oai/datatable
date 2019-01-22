#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import os
import pytest
import random
import sys
from math import isnan

# Try importing _datatable (core lib), so that if that doesn't work we don't
# run any tests (which will all fail anyways). Additionally, we attempt to
# resolve any obfuscated C++ names, for convenience.
try:
    from datatable.lib import core as c
    assert c
except ImportError as e:
    import re
    import subprocess
    mm = re.search(r"dlopen\(.*\): Symbol not found: (\w+)", e.msg)
    try:
        symbol = mm.group(1)
        decoded = subprocess.check_output(["c++filt", symbol]).decode().strip()
        e = ImportError(re.sub(symbol, "'%s'" % decoded, e.msg))
    except:
        # mm is None, or subprocess fail to find c++filt, etc.
        pass
    raise e



def same_iterables(a, b):
    """
    Convenience function for testing datatables created from dictionaries.

    On Python3.6+ it simply checks whether `a == b`. However on Python3.5, it
    checks whether `a` and `b` have same elements but potentially in a different
    order.

    The reason for this helper function is the difference between semantics of
    a dictionary in Python3.6 versus older versions: in Python3.6 dictionaries
    preserved the order of their keys, whereas in previous Python versions they
    didn't. Thus, if one creates say a datatable

        dt = datatable.Frame({"A": 1, "B": "foo", "C": 3.5})

    then in Python3.6 it will be guaranteed to have columns ("A", "B", "C") --
    in this order, whereas in Python3.5 or older the order may be arbitrary.
    Thus, this function is designed to help with testing of such a datatable:

        assert same_iterables(dt.names, ("A", "B", "C"))
        assert same_iterables(dt.types, ("int", "string", "float"))

    Then in Python3.6 lhs and rhs will be tested with strict equality, whereas
    in older Python versions the test will be weaker, taking into account the
    non-deterministic nature of the dictionary that created the datatable.
    """
    if type(a) != type(b) or len(a) != len(b):
        return False
    if sys.version_info >= (3, 6):
        return list_equals(a, b)
    else:
        js = set(range(len(a)))
        for i in range(len(a)):
            found = False
            for j in js:
                if list_equals(a[i], b[j]):
                    found = True
                    js.remove(j)
                    break
            if not found:
                return False
        return True



def list_equals(a, b):
    """
    Helper functions that tests whether two lists `a` and `b` are equal.

    The primary difference from the built-in Python equality operator is that
    this function compares floats up to a relative tolerance of 1e-6, and it
    also compares floating NaN as equal to itself (in standard Python
    `nan != nan`).
    The purpose of this function is to compare datatables' python
    representations.
    """
    if a == b: return True
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        if isnan(a) and isnan(b): return True
        if abs(a - b) < 1e-6 * (abs(a) + abs(b) + 1): return True
    if isinstance(a, list) and isinstance(b, list):
        return (len(a) == len(b) and
                all(list_equals(a[i], b[i]) for i in range(len(a))))
    return False



def assert_equals(frame1, frame2):
    """
    Helper function to assert that 2 frames are equal to each other.
    """
    frame1.internal.check()
    frame2.internal.check()
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
        assert frame1.to_list() == frame2.to_list()
    else:
        assert frame1.shape == frame2.shape
        assert same_iterables(frame1.names, frame2.names)
        assert same_iterables(frame1.stypes, frame2.stypes)
        assert same_iterables(frame1.to_list(), frame2.to_list())



def random_string(n):
    """Return random alphanumeric string of `n` characters."""
    alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
    return "".join(random.choice(alphabet) for _ in range(n))


def find_file(*nameparts):
    root_var = "DT_LARGE_TESTS_ROOT"
    d = os.environ.get(root_var, "").strip()
    filename = os.path.join(d, *nameparts)
    if not d:
        pytest.skip("%s is not defined" % root_var)
    elif not os.path.isdir(d):
        pytest.fail("Directory '%s' does not exist" % d)
    elif not os.path.isfile(filename):
        pytest.skip("File %s not found" % filename)
    else:
        return filename


def has_llvm():
    return False


def noop(x):
    """
    Use this function to discard the result of some calculation, e.g:

        with pytest.raises(ValueError):
            noop(DT[1, 2, 3, 4, 5])

    The purpose of this function is to silence the warning about unassigned
    expression result.
    """
    pass
