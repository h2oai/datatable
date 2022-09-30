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
# Tests related to datatable's ability to detect various parsing settings, such
# as sep, ncols, quote rule, presence of headers, etc.
#-------------------------------------------------------------------------------
import datatable as dt
from datatable.internal import frame_integrity_check
from tests import assert_equals



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


def test_fread_headers_first_colname_missing():
    DT = dt.fread(text=",first_name,last_name\n0,John,Doe\n1,Jane,Doe\n2,J,D")
    DT_ref = dt.Frame(C0=[0, 1, 2],
                      first_name = ["John", "Jane", "J"],
                      last_name = ["Doe", "Doe", "D"])
    assert_equals(DT, DT_ref)



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




#-------------------------------------------------------------------------------
# Detect newline
#-------------------------------------------------------------------------------

def test_detect_newlines1():
    DT = dt.fread("A,B\n1,2\n3,4\n")
    assert_equals(DT, dt.Frame(A=[1, 3], B=[2, 4]))


def test_detect_newlines2():
    DT = dt.fread("A,B\r1,2\r3,4")
    assert_equals(DT, dt.Frame(A=[1, 3], B=[2, 4]))


def test_detect_newline3():
    DT = dt.fread("A,B\rC\rD\n3,4")
    assert_equals(DT, dt.Frame([[3], [4]], names=["A", "B.C.D"]))


def test_detect_newlines4():
    # We wait to see at least 10 \r's before concluding that it must be
    # a valid line separator
    DT1 = dt.fread("B\r1\r2\r3\r4\r5\r6\r7\r8\r9\n255")
    assert_equals(DT1, dt.Frame({"B.1.2.3.4.5.6.7.8.9": [255]}))
    DT2 = dt.fread("B\r1\r2\r3\r4\r5\r6\r7\r8\r9\r0\n255")
    assert_equals(DT2, dt.Frame(B=[1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 255]))


def test_detect_newlines5():
    DT = dt.fread('A,"B\nC"\r5,7')
    assert_equals(DT, dt.Frame({"A": [5], "B.C": [7]}))


def test_detect_newlines6():
    DT1 = dt.fread('A,"B\n""C"\r5,7')
    assert_equals(DT1, dt.Frame({"A": [5], "B.\"C": [7]}))
    DT2 = dt.fread('A,"B\n\\"C"\r5,7')
    assert_equals(DT2, dt.Frame({"A": [5], "B.\"C": [7]}))


def test_detect_newlines7():
    # No newlines found
    DT = dt.fread(text="A,B")
    assert_equals(DT, dt.Frame(A=[], B=[]))


def test_detect_newline8():
    DT = dt.fread("a,b,c\r1,4,foo\r7,99,\"wha\ntt?\"\r4,-1,nvm")
    assert_equals(DT, dt.Frame(a=[1, 7, 4],
                               b=[4, 99, -1],
                               c=["foo", "wha\ntt?", "nvm"]))



#-------------------------------------------------------------------------------
# Detect quote rule
#-------------------------------------------------------------------------------

def test_quotes_unescaped():
    DT = dt.fread("""
        Size    Width   Height  Area
        "       "       "       sq.in.
        32"     27.9"   15.7"   438
        40"     34.9"   19.6"   684
        43"     37.5"   21.1"   791
        50"     43.6"   24.5"   1068
        55"     47.9"   27.0"   1293
        60"     52.3"   29.4"   1538"""
    )
    assert DT.shape == (7, 4)
    assert DT.names == ("Size", "Width", "Height", "Area")
    assert DT.to_tuples()[0] == ('"', '"', '"', 'sq.in.')
