#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
# Tests related to datatable's ability to detect various parsing settings, such
# as sep, ncols, quote rule, presence of headers, etc.
#-------------------------------------------------------------------------------
import datatable as dt
from datatable.internal import frame_integrity_check



#-------------------------------------------------------------------------------
# Detect headers
#-------------------------------------------------------------------------------

def test_fread_headers_less1():
    # Number of headers is less than the number of columns
    f0 = dt.fread("A\n10,5,foo,1\n11,-3,bar,2\n")
    frame_integrity_check(f0)
    assert f0.names == ("A", "C0", "C1", "C2")
    assert f0.to_list() == [[10, 11], [5, -3], ["foo", "bar"], [1, 2]]


def test_fread_headers_less2():
    f0 = dt.fread("A,B\n10,5,foo,1\n11,-3,bar,2\n")
    frame_integrity_check(f0)
    assert f0.names == ("A", "B", "C0", "C1")
    assert f0.to_list() == [[10, 11], [5, -3], ["foo", "bar"], [1, 2]]


def test_fread_headers_against_mu():
    # See issue #656
    # First row contains "A" in first column, while in the rest of the file
    # first column is empty. "Empty" type should be considered equivalent to
    # string (or any other) for the purpose of headers detection.
    f0 = dt.fread("A,100,2\n,300,4\n,500,6\n")
    frame_integrity_check(f0)
    assert f0.names == ("C0", "C1", "C2")
    assert f0.to_list() == [["A", "", ""], [100, 300, 500], [2, 4, 6]]


def test_fread_headers_with_blanks():
    f0 = dt.fread("""
        02-FEB-2009,09:55:04:962,PE,36,500,44,200,,2865
        02-FEB-2009,09:55:04:987,PE,108.75,200,111,50,,2865
        02-FEB-2009,09:55:04:939,CE,31.1,3000,36.55,200,,2865
        """)
    frame_integrity_check(f0)
    assert f0.names == ("C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8")
    assert f0[:, -2:].to_list() == [[None] * 3, [2865] * 3]



#-------------------------------------------------------------------------------
# Detect separator
#-------------------------------------------------------------------------------

def test_detect_sep1():
    """Test that comma is detected as sep, not space."""
    f0 = dt.fread("""
        Date Time,Open,High,Low,Close,Volume
        2007/01/01 22:51:00,5683,5683,5673,5673,64
        2007/01/01 22:52:00,5675,5676,5674,5674,17
        2007/01/01 22:53:00,5674,5674,5673,5674,42
        """)
    frame_integrity_check(f0)
    assert f0.shape == (3, 6)
    assert f0.names == ("Date Time", "Open", "High", "Low", "Close", "Volume")
    assert f0[:, "Open"].to_list() == [[5683, 5675, 5674]]
