#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
#-------------------------------------------------------------------------------
# Tests for various tricky / corner cases, especially when reported as an issue
# on GitHub. The test cases are expected to all be reasonably small.
#-------------------------------------------------------------------------------
import datatable as dt
from datatable import ltype, stype
import math
import pytest
import random



def test_select_some_columns():
    # * Last field of last line contains separator
    # * The file doesn't end with \n
    # * Only subset of columns is requested
    f = dt.fread('A,B,C\n1,2,"a,b"', columns={'A', 'B'})
    assert f.internal.check()
    assert f.names == ("A", "B")
    assert f.topython() == [[1], [2]]


def test_fread_issue527():
    """
    Test handling of invalid UTF-8 characters: right now they are decoded
    using Windows-1252 code page (this is better than throwing an exception).
    """
    inp = b"A,B\xFF,C\n1,2,3\xAA\n"
    d0 = dt.fread(text=inp)
    assert d0.internal.check()
    assert d0.names == ("A", "Bÿ", "C")
    assert d0.topython() == [[1], [2], ["3ª"]]


def test_fread_issue594():
    """
    Test handling of characters that are both invalid UTF-8 and invalid
    Win-1252, when they appear as a column header.
    """
    bad = (b'\x7f'
           b'\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f'
           b'\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f'
           b'\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf'
           b'\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf'
           b'\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf'
           b'\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf'
           b'\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef'
           b'\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff')
    inp = (b'A,"' + bad + b'"\n2,foo\n')
    d0 = dt.fread(text=inp)
    assert d0.internal.check()
    assert d0.shape == (1, 2)
    assert d0.names == ("A", bad.decode("windows-1252", "replace"))


def test_fread_issue615():
    d0 = dt.fread("A,B,C,D,E,F,G,H,I\n"
                  "NaNaNa,Infinity-3,nanny,0x1.5p+12@boo,23ba,2.5e-4q,"
                  "Truely,Falsely,1\n")
    assert d0.internal.check()
    assert d0.topython() == [["NaNaNa"], ["Infinity-3"], ["nanny"],
                             ["0x1.5p+12@boo"], ["23ba"], ["2.5e-4q"],
                             ["Truely"], ["Falsely"], [1]]


def test_fread_issue628():
    """Similar to #594 but read in verbose mode."""
    d0 = dt.fread(b"a,\x80\n11,2\n", verbose=True)
    assert d0.internal.check()
    assert d0.topython() == [[11], [2]]
    # The interpretation of byte \x80 as symbol € is not set in stone: we may
    # alter it in the future, or make it platform-dependent?
    assert d0.names == ("a", "€")



def test_fread_CtrlZ():
    """Check that Ctrl+Z characters at the end of the file are removed"""
    src = b"A,B,C\n-1,2,3\x1A\x1A"
    d0 = dt.fread(text=src)
    assert d0.internal.check()
    assert d0.ltypes == (dt.ltype.int, dt.ltype.int, dt.ltype.int)
    assert d0.topython() == [[-1], [2], [3]]


def test_fread_NUL():
    """Check that NUL characters at the end of the file are removed"""
    d0 = dt.fread(text=b"A,B\n2.3,5\0\0\0\0\0\0\0\0\0\0")
    assert d0.internal.check()
    assert d0.ltypes == (dt.ltype.real, dt.ltype.int)
    assert d0.topython() == [[2.3], [5]]


def test_fread_1col_a():
    """Check that it is possible to read 1-column file witn NAs."""
    # We also check that trailing newlines are not discarded in this case
    # (otherwise round-trip with write_csv wouldn't work).
    d0 = dt.fread(text="A\n1\n2\n\n4\n\n5\n\n")
    assert d0.internal.check()
    assert d0.names == ("A",)
    assert d0.topython() == [[1, 2, None, 4, None, 5, None]]


def test_fread_1col_b():
    d1 = dt.fread("QUOTE\n"
                  "If you think\n"
                  "you can do it,\n\n"
                  "you can.\n\n", sep="\n")
    assert d1.internal.check()
    assert d1.names == ("QUOTE",)
    assert d1.topython() == [["If you think", "you can do it,", "",
                              "you can.", ""]]


@pytest.mark.parametrize("eol", ["\n", "\r", "\n\r", "\r\n", "\r\r\n"])
def test_fread_1col_c(eol):
    data = ["A", "100", "200", "", "400", "", "600"]
    d0 = dt.fread(eol.join(data))
    assert d0.internal.check()
    assert d0.names == ("A", )
    assert d0.topython() == [[100, 200, None, 400, None, 600]]


def test_fread_line_endings():
    entries = ["A", "", "1", "2", "3"]
    for eol in ["\n", "\r", "\n\r", "\r\n", "\r\r\n"]:
        text = eol.join(entries)
        d0 = dt.fread(text=text)
        assert d0.internal.check()
        assert d0.names == ("A",)
        assert d0.topython() == [[None, 1, 2, 3]]


def test_fread_with_whitespace():
    """
    Check that whitespace at the ends of lines is ignored, even if sep=' ' is
    detected. See issue #606
    """
    d0 = dt.fread(text="A\n23     ")
    assert d0.internal.check()
    assert d0.names == ("A",)
    assert d0.topython() == [[23]]
    d1 = dt.fread("A B C \n10 11 12 \n")
    assert d1.internal.check()
    assert d1.names == ("A", "B", "C")
    assert d1.topython() == [[10], [11], [12]]
    d2 = dt.fread("a  b  c\nfoo  bar  baz\noof  bam  \nnah  aye  l8r")
    assert d2.internal.check()
    assert d2.names == ("a", "b", "c")
    assert d2.topython() == [["foo", "oof", "nah"],
                             ["bar", "bam", "aye"],
                             ["baz", None,  "l8r"]]  # should this be ""?


def test_fread_hex():
    rnd = random.random
    arr = [rnd() * 10**(10**rnd()) for i in range(20)]
    inp = "A\n%s\n" % "\n".join(x.hex() for x in arr)
    d0 = dt.fread(text=inp)
    assert d0.internal.check()
    assert d0.ltypes == (ltype.real, )
    assert d0.topython() == [arr]


def test_fread_hex0():
    d0 = dt.fread(text="A\n0x0.0p+0\n0x0p0\n-0x0p-0\n")
    assert d0.topython() == [[0.0] * 3]
    assert math.copysign(1.0, d0.topython()[0][2]) == -1.0


def test_fread_float():
    inp = "A\n0x0p0\n0x1.5p0\n0x1.5p-1\n0x1.2AAAAAp+22"
    d0 = dt.fread(text=inp)
    assert d0.internal.check()
    assert d0.stypes == (stype.float32, )
    assert d0.topython() == [[0, 1.3125, 0.65625, 4893354.5]]
