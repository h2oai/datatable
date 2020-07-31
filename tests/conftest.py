#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
    assert sys.version_info >= (3, 5), "Python version 3.5+ is required"
    dt.options.progress.enabled = False


@pytest.fixture(scope="session")
def py36():
    """Skip the test when run under Python 3.5.x"""
    if sys.version_info < (3, 6):
        pytest.skip("Python3.6+ is required")


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
def tempdir():
    dirname = mod_tempfile.mkdtemp()
    yield dirname
    shutil.rmtree(dirname, ignore_errors=True)


# Without this fixture, get_core_tests() does not work properly...
@pytest.fixture()
def testname(request):
    return request.param()

