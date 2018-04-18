#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
# Tests related to datatable's ability to detect various parsing settings, such
# as sep, ncols, quote rule, presence of headers, etc.
#-------------------------------------------------------------------------------
import pytest
import datatable as dt



#-------------------------------------------------------------------------------
# Detect headers
#-------------------------------------------------------------------------------

def test_fread_headers_less1():
    # Number of headers is less than the number of columns
    f0 = dt.fread("A\n10,5,foo,1\n11,-3,bar,2\n")
    assert f0.internal.check()
    assert f0.names == ("A", "C0", "C1", "C2")
    assert f0.topython() == [[10, 11], [5, -3], ["foo", "bar"], [1, 2]]


def test_fread_headers_less2():
    f0 = dt.fread("A,B\n10,5,foo,1\n11,-3,bar,2\n")
    assert f0.internal.check()
    assert f0.names == ("A", "B", "C0", "C1")
    assert f0.topython() == [[10, 11], [5, -3], ["foo", "bar"], [1, 2]]


def test_fread_headers_against_mu():
    # See issue #656
    # First row contains "A" in first column, while in the rest of the file
    # first column is empty. "Empty" type should be considered equivalent to
    # string (or any other) for the purpose of headers detection.
    f0 = dt.fread("A,100,2\n,300,4\n,500,6\n")
    assert f0.internal.check()
    assert f0.names == ("C0", "C1", "C2")
    assert f0.topython() == [["A", "", ""], [100, 300, 500], [2, 4, 6]]
