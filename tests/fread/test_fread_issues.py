#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
# Tests for various tricky / corner cases, especially when reported as an issue
# on GitHub. The test cases are expected to all be reasonably small.
#-------------------------------------------------------------------------------
import datatable as dt
import pytest
import random
import re
import sys
from datatable.internal import frame_integrity_check
from tests import find_file, same_iterables


def test_issue1935():
    # Check that giving wrong command to `cmd=` parameter raises
    # an error instead of returning an empty Frame silently.
    with pytest.raises(ValueError) as e:
        dt.fread(cmd="leeroy jenkins")
    assert ("Shell command returned error code" in str(e.value))
    # This may need adjustments for different OSes
    assert re.search(r"leeroy:(?: command)? not found\s*", str(e.value))



#-------------------------------------------------------------------------------

def test_issue_R1113():
    # Based on #1113 in R, test 1555.1-11
    txt = ("ITER    THETA1    THETA2   MCMC\n"
           "        -11000 -2.50000E+00  2.30000E+00    345678.20255 \n"
           "        -10999 -2.49853E+01  3.79270E+02    -195780.43911\n"
           "        -10998 1.95957E-01  4.16522E+00    7937.13048")
    d0 = dt.fread(txt, verbose=True)
    frame_integrity_check(d0)
    assert d0.names == ("ITER", "THETA1", "THETA2", "MCMC")
    assert d0.ltypes == (dt.ltype.int, dt.ltype.real, dt.ltype.real,
                         dt.ltype.real)
    assert d0.to_list() == [[-11000, -10999, -10998],
                            [-2.5, -24.9853, 0.195957],
                            [2.3, 379.270, 4.16522],
                            [345678.20255, -195780.43911, 7937.13048]]
    # `strip_whitespace` has no effect when `sep == ' '`
    d1 = dt.fread(txt, strip_whitespace=False)
    frame_integrity_check(d1)
    assert d1.names == d0.names
    assert d1.to_list() == d0.to_list()
    # Check that whitespace is not removed from column names either
    d2 = dt.fread(text=" ITER,    THETA1,       THETA2", strip_whitespace=False)
    d3 = dt.fread(text=" ITER  ,  THETA1   ,    THETA2", strip_whitespace=False)
    d4 = dt.fread(text=' ITER  ,  THETA1  ,   "THETA2"', strip_whitespace=False)
    assert d2.names == (" ITER", "    THETA1", "       THETA2")
    assert d3.names == (" ITER  ", "  THETA1   ", "    THETA2")
    assert d4.names == (' ITER  ', '  THETA1  ', '   "THETA2"')



def test_issue_R2106():
    """skip_blank_lines and fill parameters in single-column file."""
    # See also: #R2535
    src = "A\n1\n5\n\n12\n18\n\n"
    src2 = "A\n1\n5\nNA\n12\n18\nNA\n"
    d0 = dt.fread(src)
    d1 = dt.fread(src, skip_blank_lines=True)
    d2 = dt.fread(src, skip_blank_lines=True, fill=True)
    d3 = dt.fread(src2, na_strings=[""])
    d4 = dt.fread(src2, na_strings=["NA"])
    frame_integrity_check(d0)
    frame_integrity_check(d1)
    frame_integrity_check(d2)
    frame_integrity_check(d3)
    frame_integrity_check(d4)
    assert d0.to_list() == [[1, 5, None, 12, 18, None]]
    assert d1.to_list() == [[1, 5, 12, 18]]
    assert d2.to_list() == [[1, 5, 12, 18]]
    assert d3.to_list() == [["1", "5", "NA", "12", "18", "NA"]]
    assert d4.to_list() == [[1, 5, None, 12, 18, None]]


def test_issue_R2196():
    """
    Check that column detection heuristic detects 3 not 4 columns here...
    """
    d0 = dt.fread('1,2,"3,a"\n4,5,"6,b"')
    frame_integrity_check(d0)
    assert d0.shape == (2, 3)
    assert d0.to_list() == [[1, 4], [2, 5], ["3,a", "6,b"]]


def test_issue_R2222():
    d0 = dt.fread("A,B\n999,1\n999,2\n", na_strings=["999", "NA"])
    d1 = dt.fread("A,B\n999,1\n4,2\n", na_strings=["999", "NA"])
    d2 = dt.fread("A,B\n999,5\n999,999\n", na_strings=["999", "NA"])
    d3 = dt.fread("A,B\n999,1\n999,2\n", na_strings=["99", "NA"])
    frame_integrity_check(d0)
    assert d0.to_list() == [[None, None], [1, 2]]
    assert d1.to_list() == [[None, 4], [1, 2]]
    assert d2.to_list() == [[None, None], [5, None]]
    assert d3.to_list() == [[999, 999], [1, 2]]


@pytest.mark.parametrize("v", ["%", "%d", "%s", "%.*s"])
def test_issue_R2287(v):
    """
    If a line where an error occurs contains %d / %s, then they should not be
    interpreted as format characters -- this may result in a crash!
    """
    with pytest.raises(Exception) as e:
        dt.fread("A,B\nfoo,1\nbar" + v)
    assert 'expected 2 but found only 1' in str(e.value)
    assert ('<<bar%s>>' % v) in str(e.value)


def test_issue_R2299():
    """Bad column count on out-of-sample row."""
    # If this crashes on Windows, it's because of %zd/%zu format strings...
    src = ("A,B\n" +
           "1,2\n" * 100 +
           "999\n" +
           "3,4\n" * 5000)
    with pytest.raises(Exception) as e:
        dt.fread(src)
    assert re.search("Too few fields on line 102: expected 2 but found only 1",
                     str(e.value))


def test_issue_R2351(tempfile):
    lines = [b"id%d,%d" % (i, (i * 1000001) % 137) for i in range(100000)]
    text = b"\r".join([b"id,v"] + lines + [b""])
    with open(tempfile, "wb") as o:
        o.write(text)
    d0 = dt.fread(tempfile)
    frame_integrity_check(d0)
    assert d0[:2, :].to_list() == [["id0", "id1"], [0, 38]]
    assert d0[-2:, :].to_list() == [["id99998", "id99999"], [92, 130]]
    with open(tempfile, "ab") as o:
        o.write(b"foo,1000")
    d0 = dt.fread(tempfile)
    frame_integrity_check(d0)
    assert d0[-2:, :].to_list() == [["id99999", "foo"], [130, 1000]]


def test_issue_R2404():
    inp = [["Abc", "def", '"gh,kl"', "mnopqrst"]] * 1000
    inp[111] = ["ain't", "this", "a", "surprise!"]
    txt = "A,B,C,D\n" + "\n".join(",".join(row) for row in inp)
    d0 = dt.fread(txt)
    frame_integrity_check(d0)
    assert d0.names == ("A", "B", "C", "D")
    assert d0.shape == (1000, 4)
    inp[111][2] = '"a"'
    assert d0.to_list() == [[row[0] for row in inp],
                            [row[1] for row in inp],
                            [row[2][1:-1] for row in inp],  # unescape
                            [row[3] for row in inp]]


@pytest.mark.parametrize("sep", [" ", ",", ";"])
def test_issue_R2322(sep):
    """
    Single-column file where column name has spaces in it. This should be
    detected correctly (as opposed to guessing sep = ' ').
    """
    name = sep.join("abcd")
    d0 = dt.fread(name + "\n2\n3\n4\n")
    frame_integrity_check(d0)
    assert d0.shape == (3, 1)
    assert d0.names == (name, )
    assert d0.to_list() == [[2, 3, 4]]


def test_issue_R2464():
    # * Last field of last line contains separator
    # * The file doesn't end with \n
    # * Only subset of columns is requested
    f = dt.fread('A,B,C\n1,2,"a,b"', columns={'A', 'B'})
    frame_integrity_check(f)
    assert f.names == ("A", "B")
    assert f.to_list() == [[1], [2]]


def test_issue_R2535():
    """Test that `skip_blank_lines` takes precedence over parameter `fill`."""
    # See also: #R2106
    src = "a b 2\nc d 3\n\ne f 4\n"
    d0 = dt.fread(src, skip_blank_lines=True, fill=False)
    d1 = dt.fread(src, skip_blank_lines=True, fill=True)
    d2 = dt.fread(src, skip_blank_lines=False, fill=True)
    frame_integrity_check(d0)
    frame_integrity_check(d1)
    frame_integrity_check(d2)
    assert d0.to_list() == [list("ace"), list("bdf"), [2, 3, 4]]
    assert d1.to_list() == [list("ace"), list("bdf"), [2, 3, 4]]
    assert d2.to_list() == [["a", "c", None, "e"],
                            ["b", "d", None, "f"],
                            [2, 3, None, 4]]


@pytest.mark.skip(reason="Issue #804")
def test_issue_R2535x():
    # Currently we set fill=True when sep=' ' is detected. However fread should
    # distinguish between the user not passing `fill` parameter at all, or
    # passing explicitly `fill=False` value.
    src = "a b 2\nc d 3\n\ne f 4\n"
    d3 = dt.fread(src, skip_blank_lines=False, fill=False)
    assert d3.to_list() == [list("ac"), list("bd"), [2, 3]]


def test_issue_R2542():
    d0 = dt.fread("A\r1\r\r\r2\r")
    frame_integrity_check(d0)
    assert d0.to_list() == [[1, None, None, 2]]


def test_issue_R2666():
    # Explicitly specified sep should not be ignored
    d0 = dt.fread("1;2;3\n4\n5;6", sep=";", fill=True)
    d1 = dt.fread("1;2;3\n4\n5",   sep=";", fill=True)
    d2 = dt.fread("1;2;3\n;4\n5",  sep=";", fill=True)
    d3 = dt.fread("1;2;3\n4\n;5",  sep=";", fill=True)
    assert d0.to_list() == [[1, 4, 5],    [2, None, 6],    [3, None, None]]
    assert d1.to_list() == [[1, 4, 5],    [2, None, None], [3, None, None]]
    assert d2.to_list() == [[1, None, 5], [2, 4, None],    [3, None, None]]
    assert d3.to_list() == [[1, 4, None], [2, None, 5],    [3, None, None]]



#-------------------------------------------------------------------------------

def test_issue_527():
    """
    Test handling of invalid UTF-8 characters: right now they are decoded
    using Windows-1252 code page (this is better than throwing an exception).
    """
    inp = b"A,B\xFF,C\n1,2,3\xAA\n"
    d0 = dt.fread(text=inp)
    frame_integrity_check(d0)
    assert d0.names == ("A", "Bÿ", "C")
    assert d0.to_list() == [[1], [2], ["3ª"]]


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
    frame_integrity_check(d0)
    assert d0.shape == (1, 2)
    assert d0.names == ("A", bad.decode("windows-1252", "replace"))


def test_issue_606():
    """
    Check that whitespace at the ends of lines is ignored, even if sep=' ' is
    detected. See issue #606
    """
    d0 = dt.fread(text="A\n23     ")
    frame_integrity_check(d0)
    assert d0.names == ("A",)
    assert d0.to_list() == [[23]]
    d1 = dt.fread("A B C \n10 11 12 \n")
    frame_integrity_check(d1)
    assert d1.names == ("A", "B", "C")
    assert d1.to_list() == [[10], [11], [12]]
    d2 = dt.fread("a  b  c\nfoo  bar  baz\noof  bam  \nnah  aye  l8r")
    frame_integrity_check(d2)
    assert d2.names == ("a", "b", "c")
    assert d2.to_list() == [["foo", "oof", "nah"],
                            ["bar", "bam", "aye"],
                            ["baz", None,  "l8r"]]  # should this be ""?


def test_issue_615():
    d0 = dt.fread("A,B,C,D,E,F,G,H,I\n"
                  "NaNaNa,Infinity-3,nanny,0x1.5p+12@boo,23ba,2.5e-4q,"
                  "Truely,Falsely,1\n")
    frame_integrity_check(d0)
    assert d0.to_list() == [["NaNaNa"], ["Infinity-3"], ["nanny"],
                            ["0x1.5p+12@boo"], ["23ba"], ["2.5e-4q"],
                            ["Truely"], ["Falsely"], [1]]


def test_issue_628():
    """Similar to #594 but read in verbose mode."""
    d0 = dt.fread(b"a,\x80\n11,2\n", verbose=True)
    frame_integrity_check(d0)
    assert d0.to_list() == [[11], [2]]
    # The interpretation of byte \x80 as symbol € is not set in stone: we may
    # alter it in the future, or make it platform-dependent?
    assert d0.names == ("a", "€")


def test_issue_641():
    f = dt.fread("A,B,C\n"
                 "5,,\n"
                 "6,foo\rbar,z\n"
                 "7,bah,")
    frame_integrity_check(f)
    assert f.names == ("A", "B", "C")
    assert f.ltypes == (dt.ltype.int, dt.ltype.str, dt.ltype.str)
    assert f.to_list() == [[5, 6, 7], ["", "foo\rbar", "bah"], ["", "z", ""]]


def test_issue_643():
    d0 = dt.fread("A B\n1 2\n3 4 \n5 6\n6   7   ")
    frame_integrity_check(d0)
    assert d0.names == ("A", "B")
    assert d0.ltypes == (dt.ltype.int, dt.ltype.int)
    assert d0.to_list() == [[1, 3, 5, 6], [2, 4, 6, 7]]


def test_issue_664(capsys):
    f = dt.fread("x\nA B 2\n\ny\n", sep=" ", fill=True, verbose=True,
                 skip_blank_lines=True)
    out, err = capsys.readouterr()
    assert not err
    assert "Too few rows allocated" not in out
    assert "we know nrows=3 exactly" in out
    frame_integrity_check(f)
    assert f.shape == (3, 3)
    assert f.to_list() == [["x", "A", "y"],
                           [None, "B", None],
                           [None, 2, None]]


def test_issue_670():
    d0 = dt.fread("A\n1\n\n\n2\n", skip_blank_lines=True)
    frame_integrity_check(d0)
    assert d0.shape == (2, 1)
    assert d0.to_list() == [[1, 2]]


@pytest.mark.parametrize("seed", [random.randint(0, 2**31)])
def test_issue682(seed):
    """
    Make sure that `nextGoodLine()` considers blank line as valid whenever
    the main data loop does.
    """
    random.seed(seed)
    n = 100000
    src = [None] * n
    src[0] = "A"
    for i in range(1, n):
        x = random.randint(0, 300000000)
        if x > 230000000:
            src[i] = ""
        else:
            src[i] = str(x)
    src[-1] = "1"  # Last entry cannot be "", since there is no final newline
    txt = "\n".join(src)
    d0 = dt.fread(txt, verbose=True)
    frame_integrity_check(d0)
    assert d0.ltypes == (dt.ltype.int, )
    assert d0.names == ("A", )
    assert d0.shape == (n - 1, 1)


@pytest.mark.parametrize("seed", [random.randint(0, 2**31)])
def test_issue684(seed):
    """
    The issue was caused by jumping inside a '\\n\\r' newline sequence, when
    the single '\\r' was not considered a newline and was treated as part of
    the data.
    """
    random.seed(seed)
    n = 100000
    src = [str(random.randint(0, 30)) for _ in range(n)]
    src[0] = "A"
    txt = "\n\r".join(src)
    d0 = dt.fread(txt, verbose=True)
    frame_integrity_check(d0)
    assert d0.ltypes == (dt.ltype.int,)
    assert d0.shape == (n - 1, 1)


def test_issue735():
    """
    Integer column turns into a string consisting of 2 comma-separated
    integers on an out-of-sample line...
    """
    lines = ["1,2"] * 2199
    lines[111] = '5,"7,60000"'
    src = "A,B\n" + "\n".join(lines)
    d0 = dt.fread(src)
    frame_integrity_check(d0)


@pytest.mark.parametrize("seed", [random.randint(0, 2**31)])
def test_issue720(seed):
    """
    Fields containing many newlines should still work correctly.
    """
    seed = 124960729
    random.seed(seed)
    n = 100000
    src0 = ["a"] * n
    src1 = ["\n" * int(random.expovariate(0.02)) for _ in range(n)]
    lines = "\n".join('%s,"%s"' % (src0[i], src1[i])
                      for i in range(n))
    src = "A,B\n" + lines
    d0 = dt.fread(src)
    frame_integrity_check(d0)
    assert d0.names == ("A", "B")
    assert d0.ltypes == (dt.ltype.str, dt.ltype.str)
    assert d0.to_list() == [src0, src1]


def test_issuee786():
    df = dt.fread('"A","B"\n', sep="")
    frame_integrity_check(df)
    assert df.shape == (0, 1)
    assert df.names == ('"A","B"',)
    assert df.to_list() == [[]]


def test_issue939(capsys):
    df = dt.fread("""
             1,9ff4ed3a-6b00-4130-9aca-2ed897305fd1,1
             2,ac1e1ca3-5ca8-438a-85a4-8175ed5bb7ec,1
             3,6870f256-e145-4d75-adb0-99ccb77d5d3a,0
             4,d8da52c1-d145-4dff-b3d1-127c6eb75d40,1
             5,,0
             6,2e1d193f-d1da-4664-8a2b-ffdfe0aa7be3,0
    1000010407,89e68530-422e-43ba-bd00-aa3d8f2cfcaa,1
    1000024046,4055a53b-411f-46f0-9d2e-cf03bc95c080,0
    1000054511,49d14d8e-5c42-439d-b4a8-995e25b1602f,0
    1000065922,4e31b8aa-4aa9-4e8b-be8f-5cc6323235b4,0
    1000066478,2e1d193f-d1da-4664-8a2b-ffdfe0aa7be3,0
    1000067268,25ce1456-546d-4e35-bddc-d571b26581ea,0
     100007536,d8da52c1-d145-4dff-b3d1-127c6eb75d40,1
    1000079839,6870f256-e145-4d75-adb0-99ccb77d5d3a,0
      10000913,ac1e1ca3-5ca8-438a-85a4-8175ed5bb7ec,0
    1000104538,9ff4ed3a-6b00-4130-9aca-2ed897305fd1,1
             7,00000000-0000-0000-0000-000000000000,0
             9,FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF,1

    """, verbose=True)
    frame_integrity_check(df)
    assert df.names == ("C0", "C1", "C2")
    assert df.shape == (18, 3)
    assert df.stypes == (dt.stype.int32, dt.stype.str32, dt.stype.bool8)
    out, err = capsys.readouterr()
    assert not err
    assert "`header` determined to be False" in out
    assert "Sampled 18 rows" in out
    assert "Type codes (jump 000): isb" in out
    assert "columns need to be re-read" not in out
    assert "column needs to be re-read" not in out


def test_issue998():
    src = find_file("h2o-3", "bigdata", "laptop", "higgs_head_2M.csv")
    # The file is 1.46GB in size. I could not find a smaller file that exhibits
    # this problem... The issue only appeared in single-threaded mode, so we
    # have to read this file slowly. On my laptop, this test runs in about 8s.
    f0 = dt.fread(src, nthreads=1, fill=True, na_strings=["-999"])
    assert f0.shape == (2000000, 29)
    assert f0.names == tuple("C%d" % i for i in range(f0.ncols))
    assert f0.stypes == (dt.stype.float64,) * f0.ncols
    assert same_iterables(
        f0.sum().to_list(),
        [[1058818.0], [1981919.6107614636], [701.7858121241807],
         [-195.48500674014213], [1996390.3476011853], [-1759.5364254778178],
         [1980743.446578741], [-1108.7512905876065], [1712.947751407064],
         [2003064.4534490108], [1985100.3810670376], [1190.8404791812281],
         [384.00605312064], [1998592.0739881992], [1984490.1900614202],
         [2033.9754767678387], [-1028.0810855487362], [2001341.0813384056],
         [1971311.3271338642], [-943.92552991907], [-1079.3848229270661],
         [1996588.295421958], [2068619.2163415626], [2049516.5437491536],
         [2100795.4839400873], [2019540.6562294513], [1946283.046177674],
         [2066298.020782411], [1919714.12131235]])


def test_issue1030():
    # The file contains unterminated quote character, which had resulted in
    # a crash in verbose mode (due to incorrect use of `snprintf`).
    lines = ["6,7,8,9,3,4,5\n"] * 100000
    lines[0] = "A,B,C,D,E,F\n"
    lines[3333] = '3,"45,99,-3,7,0\n'
    src = "".join(lines)
    with pytest.raises(RuntimeError):
        dt.fread(src, verbose=True)


def test_issue1233():
    # The problem with this example is because the type hierarchy is not
    # strictly linear, we end up type-bumping the column more than once,
    # which means the re-read has to happen more than once too...
    d0 = dt.fread("NaN\n2\n")
    assert d0.to_list() == [[None, 2.0]]


def test_issue1803():
    ff = find_file("h2o-3", "smalldata", "airlines", "allyears2k.zip")
    d0 = dt.fread(ff, max_nrows=10000)
    frame_integrity_check(d0)
    assert d0.nrows == 10000
    assert d0[-1, :].to_tuples() == [(1992, 1, 6, 1, 752, 750, 838, 846, 'US',
                                      53, None, 46, 56, None, -8, 2, 'CMH',
                                      'IND', 182, None, None, False, None,
                                      False, None, None, None, None, None,
                                      'NO', 'YES')]
