#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
#-------------------------------------------------------------------------------
# Tests for various tricky / corner cases, especially when reported as an issue
# on GitHub. The test cases are expected to all be reasonably small.
#-------------------------------------------------------------------------------
import datatable as dt


def test_issue_R1113():
    # Loosely based on #1113 in R
    txt = ("ITER    THETA1    THETA2   MCMC\n"
           "        -11000 -2.50000E+00  2.30000E+00    345678.20255 \n"
           "        -10999 -2.49853E+01  3.79270E+02    -195780.43911\n"
           "        -10998 1.95957E-01  4.16522E+00    7937.13048")
    d0 = dt.fread(txt)
    assert d0.internal.check()
    assert d0.names == ("ITER", "THETA1", "THETA2", "MCMC")
    assert d0.ltypes == (dt.ltype.int, dt.ltype.real, dt.ltype.real,
                         dt.ltype.real)
    assert d0.topython() == [[-11000, -10999, -10998],
                             [-2.5, -24.9853, 0.195957],
                             [2.3, 379.270, 4.16522],
                             [345678.20255, -195780.43911, 7937.13048]]


def test_issue_R2404():
    inp = [["Abc", "def", '"gh,kl"', "mnopqrst"]] * 1000
    inp[111] = ["ain't", "this", "a", "surprise!"]
    txt = "A,B,C,D\n" + "\n".join(",".join(row) for row in inp)
    d0 = dt.fread(txt)
    assert d0.internal.check()
    assert d0.names == ("A", "B", "C", "D")
    assert d0.shape == (1000, 4)
    inp[111][2] = '"a"'
    assert d0.topython() == [[row[0] for row in inp],
                             [row[1] for row in inp],
                             [row[2][1:-1] for row in inp],  # unescape
                             [row[3] for row in inp]]


def test_issue_R2464():
    # * Last field of last line contains separator
    # * The file doesn't end with \n
    # * Only subset of columns is requested
    f = dt.fread('A,B,C\n1,2,"a,b"', columns={'A', 'B'})
    assert f.internal.check()
    assert f.names == ("A", "B")
    assert f.topython() == [[1], [2]]


def test_issue_527():
    """
    Test handling of invalid UTF-8 characters: right now they are decoded
    using Windows-1252 code page (this is better than throwing an exception).
    """
    inp = b"A,B\xFF,C\n1,2,3\xAA\n"
    d0 = dt.fread(text=inp)
    assert d0.internal.check()
    assert d0.names == ("A", "Bÿ", "C")
    assert d0.topython() == [[1], [2], ["3ª"]]


def test_issue_594():
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


def test_issue_606():
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


def test_issue_615():
    d0 = dt.fread("A,B,C,D,E,F,G,H,I\n"
                  "NaNaNa,Infinity-3,nanny,0x1.5p+12@boo,23ba,2.5e-4q,"
                  "Truely,Falsely,1\n")
    assert d0.internal.check()
    assert d0.topython() == [["NaNaNa"], ["Infinity-3"], ["nanny"],
                             ["0x1.5p+12@boo"], ["23ba"], ["2.5e-4q"],
                             ["Truely"], ["Falsely"], [1]]


def test_issue_628():
    """Similar to #594 but read in verbose mode."""
    d0 = dt.fread(b"a,\x80\n11,2\n", verbose=True)
    assert d0.internal.check()
    assert d0.topython() == [[11], [2]]
    # The interpretation of byte \x80 as symbol € is not set in stone: we may
    # alter it in the future, or make it platform-dependent?
    assert d0.names == ("a", "€")


def test_issue_641():
    f = dt.fread("A,B,C\n"
                 "5,,\n"
                 "6,foo\rbar,z\n"
                 "7,bah,")
    assert f.internal.check()
    assert f.names == ("A", "B", "C")
    assert f.ltypes == (dt.ltype.int, dt.ltype.str, dt.ltype.str)
    assert f.topython() == [[5, 6, 7], ["", "foo\rbar", "bah"], ["", "z", ""]]


def test_issue_643():
    d0 = dt.fread("A B\n1 2\n3 4 \n5 6\n6   7   ")
    assert d0.internal.check()
    assert d0.names == ("A", "B")
    assert d0.ltypes == (dt.ltype.int, dt.ltype.int)
    assert d0.topython() == [[1, 3, 5, 6], [2, 4, 6, 7]]


def test_issue_664(capsys):
    f = dt.fread("x\nA B 2\n\ny\n", sep=" ", fill=True, verbose=True,
                 skip_blank_lines=True)
    out, err = capsys.readouterr()
    assert "Too few rows allocated" not in out
    assert f.internal.check()
    assert f.shape == (3, 3)
    assert f.topython() == [["x", "A", "y"],
                            [None, "B", None],
                            [None, 2, None]]
