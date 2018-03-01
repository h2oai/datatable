#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
# This file is used by `pytest` to define common fixtures shared across all
# tests.
#-------------------------------------------------------------------------------
import os
import sys
import pytest
import shutil
import tempfile as mod_tempfile


@pytest.fixture(autouse=True, scope="session")
def setup():
    """This fixture will be run once only."""
    assert sys.version_info >= (3, 5), "Python version 3.5+ is required"


@pytest.fixture(scope="session")
def py36():
    """Skip the test when run under Python 3.5.x"""
    if sys.version_info < (3, 6):
        pytest.skip("Python3.6+ is required")


@pytest.fixture(scope="session")
def pandas():
    """
    This fixture returns pandas module, or if unavailable marks test as skipped.
    """
    try:
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
    os.unlink(fname)


@pytest.fixture(scope="function")
def tempdir():
    dirname = mod_tempfile.mkdtemp()
    yield dirname
    try:
        shutil.rmtree(dirname)
    except:
        pass
