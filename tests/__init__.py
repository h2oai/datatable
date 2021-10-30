#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2021 H2O.ai
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
import math
import os
import platform
import pytest
import random
from math import isnan
from datatable.lib import core
from datatable.internal import frame_columns_virtual, frame_integrity_check



cpp_test = pytest.mark.skipif(not hasattr(core, "get_test_suites"),
                              reason="C++ tests were not compiled")

skip_on_jenkins = pytest.mark.skipif(os.environ.get("DT_HARNESS") == "Jenkins",
                                     reason="Skipped on Jenkins")



# Try importing _datatable (core lib), so that if that doesn't work we don't
# run any tests (which will all fail anyways). Additionally, we attempt to
# resolve any obfuscated C++ names, for convenience.
try:
    from datatable.lib import core
    assert core
except ImportError as e:
    import re
    mm = re.search(r"dlopen\(.*\): Symbol not found: (\w+)", e.msg)
    if mm:
        try:
            from subprocess import check_output as run
            symbol = mm.group(1)
            decoded = run(["/usr/bin/c++filt", symbol]).decode().strip()
            e = ImportError(re.sub(symbol, "'%s'" % decoded, e.msg))
        except FileNotFoundError:
            # if /usr/bin/c++filt does not exist
            pass
    raise e



def list_equals(a, b, rel_tol = 1e-7, abs_tol = None):
    """
    Helper functions that tests whether two lists `a` and `b` are equal.

    The primary difference from the built-in Python equality operator is that
    this function compares floats up to a relative tolerance of `rel_tol`
    and absolute tolerance of `abs_tol`. It also compares floating NaN as
    equal to itself (in standard Python `nan != nan`).
    The purpose of this function is to compare datatables' python
    representations.
    """

    # The default value of `abs_tol` is set to `rel_tol`
    if abs_tol is None:
        abs_tol = rel_tol

    if a == b:
        return True
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        if isnan(a) and isnan(b):
            return True
        if abs(a - b) < max(rel_tol * max(abs(a), abs(b)), abs_tol):
            return True
    if (isinstance(a, float) and isnan(a) and b is None):
        return True
    if (isinstance(b, float) and isnan(b) and a is None):
        return True
    if isinstance(a, list) and isinstance(b, list):
        return (len(a) == len(b) and
                all(list_equals(a[i], b[i], rel_tol = rel_tol, abs_tol = abs_tol)
                    for i in range(len(a))))
    return False



def assert_equals(frame1, frame2, rel_tol = 1e-7, abs_tol = None):
    """
    Helper function to assert that 2 frames are equal to each other.
    """
    frame_integrity_check(frame1)
    frame_integrity_check(frame2)

    # The default value of `abs_tol` is set to `rel_tol`
    if abs_tol is None:
        abs_tol = rel_tol

    assert frame1.shape == frame2.shape, (
        "The left frame has shape %r, while the right has shape %r"
        % (frame1.shape, frame2.shape))

    assert frame1.names == frame2.names, (
        "The left frame has names %r, while the right has names %r"
        % (frame1.names, frame2.names))
    assert frame1.types == frame2.types, (
        "The left frame has types %r, while the right has types %r"
        % (frame1.types, frame2.types))
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
                if math.isclose(val1, val2, rel_tol = rel_tol, abs_tol = abs_tol): continue
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



def random_string(n):
    """Return random alphanumeric string of `n` characters."""
    alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
    return "".join(random.choice(alphabet) for _ in range(n))


def find_file(*nameparts):
    root_var = "DT_LARGE_TESTS_ROOT"
    d = os.environ.get(root_var, "").strip()
    if not d:
        return pytest.skip("%s is not defined" % root_var)
    filename = os.path.join(d, *nameparts)
    if not os.path.isdir(d):
        pytest.fail("Directory '%s' does not exist" % d)
    elif not os.path.isfile(filename):
        pytest.skip("File %s not found" % filename)
    else:
        return filename


def noop(x):
    """
    Use this function to discard the result of some calculation, e.g:

        with pytest.raises(ValueError):
            noop(DT[1, 2, 3, 4, 5])

    The purpose of this function is to silence the warning about unassigned
    expression result.
    """
    pass


def isview(frame):
    return any(frame_columns_virtual(frame))


def is_ppc64():
    """Helper function to determine ppc64 platform"""
    platform_hardware = [platform.machine(), platform.processor()]
    return platform.system() == "Linux" and "ppc64le" in platform_hardware


def get_core_tests(suite):
    # This must be a function, so that `n` is properly captured within
    def param(n):
        return pytest.param(lambda: n, id=n)

    if hasattr(core, "get_test_suites"):
        return [param(n) for n in core.get_tests_in_suite(suite)]
    else:
        return [pytest.param(lambda: pytest.skip("C++ tests not compiled"))]


def frame_to_xlsx(dt, filename):
    openpyxl = pytest.importorskip("openpyxl")
    wb = openpyxl.Workbook()
    ws = wb.active
    for j, name in enumerate(dt.names):
        _ = ws.cell(row=1, column=j + 1, value=name)
    for i in range(dt.nrows):
        for j in range(dt.ncols):
            _ = ws.cell(row=i + 2, column=j + 1,  value=dt[i, j])
    wb.save(filename=filename)
