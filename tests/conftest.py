#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2023 H2O.ai
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
# This file is used by `pytest` to define common fixtures shared across all
# tests.
#-------------------------------------------------------------------------------
import datatable as dt
import os
import pytest
import shutil
import sys
import platform
import tempfile as mod_tempfile
import warnings


@pytest.fixture(autouse=True, scope="session")
def setup():
    """This fixture will be run once only."""
    assert sys.version_info >= (3, 8), "Python version 3.8+ is required"
    dt.options.progress.enabled = False


def is_ppc64():
    """Helper function to determine ppc64 platform"""
    platform_hardware = [platform.machine(), platform.processor()]
    return platform.system() == "Linux" and "ppc64le" in platform_hardware


@pytest.fixture(scope="session")
def noppc64():
    """ Skip the test if running in PowerPC64 """
    if is_ppc64():
        pytest.skip("Disabled on PowerPC64 platform")


@pytest.fixture(scope="session")
def release_only():
    """Run this test for release only"""
    if not os.environ.get("DT_RELEASE"):
        pytest.skip("Enabled for release only")


@pytest.fixture(scope="session")
def win_only():
    """Skip this test when running not on Windows"""
    if platform.system() != "Windows":
        pytest.skip("Enabled on Windows only")


@pytest.fixture(scope="session")
def nowin():
    """Skip this test when running on Windows"""
    if platform.system() == "Windows":
        pytest.skip("Disabled on Windows")


@pytest.fixture(scope="session")
def tol():
    """
    This fixture returns a tolerance to compare floats on a particular platform.
    The reason we need this fixture are some platforms that don't have a proper
    long double type, resulting in a loss of precision when fread converts
    double literals into double numbers.
    """
    platform_tols = {"Windows": 1e-15, "PowerPC64": 1e-16}
    platform_system = "PowerPC64" if is_ppc64() else platform.system()

    return platform_tols.get(platform_system, 0)


@pytest.fixture(scope="session")
def nocov():
    """Skip this test when running in the 'coverage' mode"""
    if "DTCOVERAGE" in os.environ:
        pytest.skip("Disabled under COVERAGE mode")


@pytest.fixture(scope="session")
def pandas():
    """
    This fixture returns pandas module, or if unavailable marks test as skipped.
    """
    try:
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            import pandas as pd
            pd.__major_version__ = int(pd.__version__.split(".")[0])
            return pd
    except ImportError:
        pytest.skip("Pandas module is required for this test")


@pytest.fixture(scope="session")
def numpy():
    """
    This fixture returns numpy module, or if unavailable marks test as skipped.
    """
    try:
        import numpy as np
        return np
    except ImportError:
        pytest.skip("Numpy module is required for this test")


@pytest.fixture(scope="session")
def pyarrow():
    """
    This fixture returns pyarrow module, or if unavailable marks test as skipped.
    """
    try:
        import pyarrow as pa
        return pa
    except ImportError:
        pytest.skip("Pyarrow module is required for this test")


@pytest.fixture(scope="session")
def h2o():
    """
    This fixture returns h2o module, or if unavailable marks test as skipped.
    """
    try:
        import h2o
        v = h2o.__version__
        if (v == "SUBST_PROJECT_VERSION" or
                tuple(int(x) for x in v.split(".")) >= (3, 14)):
            h2o.init()
            return h2o
        else:
            pytest.skip("h2o version 3.14+ is required, you have %s" % v)
    except ImportError:
        pytest.skip("h2o module is required for this test")


@pytest.fixture(scope="function")
def tempfile():
    fd, fname = mod_tempfile.mkstemp()
    os.close(fd)
    yield fname
    if os.path.exists(fname):
        os.unlink(fname)


@pytest.fixture(scope="function")
def tempfile_jay():
    fd, fname = mod_tempfile.mkstemp(suffix=".jay")
    os.close(fd)
    yield fname
    if os.path.exists(fname):
        os.unlink(fname)


@pytest.fixture(scope="function")
def tempfile_csv():
    fd, fname = mod_tempfile.mkstemp(suffix=".csv")
    os.close(fd)
    yield fname
    if os.path.exists(fname):
        os.unlink(fname)


@pytest.fixture(scope="function")
def tempdir():
    dirname = mod_tempfile.mkdtemp()
    yield dirname
    shutil.rmtree(dirname, ignore_errors=True)


# Without this fixture, get_core_tests() does not work properly...
@pytest.fixture()
def testname(request):
    return request.param()


pd = pandas
np = numpy
pa = pyarrow
