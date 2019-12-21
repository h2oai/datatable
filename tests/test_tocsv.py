#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
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
import datatable as dt
import math
import os
import random
import re
import pytest
from datatable import stype
from datatable.internal import frame_integrity_check
from tests import assert_equals


def pyhex(v):
    """
    Normalize Python's "hex" representation by removing trailing zeros. For
    example:

        0x0.0p+0               ->  0x0p+0
        0x1.dcd6500000000p+29  ->  0x1.dcd65p+29
        0x1.0000000000000p+1   ->  0x1p+1
        0x1.4000000000000p+1   ->  0x1.4p+1
    """
    s = v.hex()
    return re.sub(r"\.?0+p", "p", s)

def stringify(x):
    if x is None:
        return ""
    return str(x)



#-------------------------------------------------------------------------------
# Test generic saving
#-------------------------------------------------------------------------------

def test_save_simple():
    d = dt.Frame([[1, 4, 5], [True, False, None], ["foo", None, "bar"]],
                 names=["A", "B", "C"])
    out = d.to_csv()
    assert out == "A,B,C\n1,1,foo\n4,0,\n5,,bar\n"


def test_issue365():
    """
    This is to verify that all data written to CSV can be safely read back,
    and that no rows were accidentally omitted.
    """
    # 999983 is prime; we want to have the number of rows not evenly divisible
    # by the number of threads.
    d0 = dt.Frame({"A": range(999983), "B": [5] * 999983})
    d1 = dt.fread(text=d0.to_csv())
    assert d1.shape == d0.shape
    assert d1.names == d0.names
    assert d1.ltypes == d0.ltypes
    assert d1.to_list() == d0.to_list()


def test_write_spacestrs():
    # Test that fields having spaces at the beginning/end are auto-quoted
    d = dt.Frame([" hello ", "world  ", " !", "!", ""])
    assert d.to_csv() == 'C0\n" hello "\n"world  "\n" !"\n!\n""\n'
    dd = dt.fread(text=d.to_csv(), sep='\n')
    assert d.to_list() == dd.to_list()


def test_write_spacenames():
    d = dt.Frame([[1, 2, 3], [1, 2, 3], [0, 0, 0]],
                 names=["  foo", "bar ", " "])
    assert d.to_csv() == '"  foo","bar "," "\n1,1,0\n2,2,0\n3,3,0\n'
    dd = dt.fread(text=d.to_csv())
    assert d.to_list() == dd.to_list()


@pytest.mark.parametrize("col, scol", [("col", "col"),
                                       ("", 'C0'),
                                       (" ", '" "'),
                                       ('"', '""""')])
def test_issue507(tempfile, col, scol):
    """
    On Unix platform some of these cases were producing random extra character
    at the end of the file (problem with truncate?). This test verifies that the
    problem doesn't reappear in the future.
    """
    d = dt.Frame({col: [-0.006492080633259916]})
    d.to_csv(tempfile)
    exp_text = scol + "\n-0.006492080633259916\n"
    with open(tempfile, "rb") as inp:
        assert inp.read() == exp_text.encode()


def test_view_to_csv():
    df0 = dt.Frame([range(10), range(0, 20, 2)], names=["A", "B"])
    df1 = df0[::2, :]
    txt1 = df1.to_csv()
    df1.materialize()
    txt2 = df1.to_csv()
    assert txt1 == txt2


def test_save_large():
    n = 1000000
    DT = dt.Frame(A=range(n))
    out1 = DT.to_csv()
    out2 = "\n".join(["A"] + [str(n) for n in range(n)] + [''])
    assert out1 == out2



#-------------------------------------------------------------------------------
# Test writing different data types
#-------------------------------------------------------------------------------

def test_save_bool():
    d = dt.Frame([True, False, None, None, False, False, True, False])
    assert d.stypes == (stype.bool8, )
    assert d.to_csv() == "C0\n1\n0\n\n\n0\n0\n1\n0\n"


def test_save_int8():
    src = [1, 0, -5, None, 127, -127, 99, 10, 100, -100, -10]
    res = "\n".join(stringify(x) for x in src)
    d = dt.Frame(src, stype=dt.int8)
    assert d.stypes == (stype.int8, )
    assert d.to_csv() == "C0\n" + res + "\n"


def test_save_int16():
    src = [0, 10, 100, 1000, 10000, 32767, -32767, -10, -20, 5, 5125, 3791,
           21, -45, 93, 0, None, 0, None, None, 499, 999, 1001, -8, None, -1]
    res = "\n".join(stringify(x) for x in src)
    d = dt.Frame(src, stype=dt.int16)
    assert d.stypes == (stype.int16, )
    assert d.to_csv() == "C0\n" + res + "\n"


def test_save_int32():
    src = [0, None, 1, 10, 100, 1000, 1000000, 1000000000, 2147483647,
           -2147483647, -25, 111, 215932, -1, None, 34857304, 999999, -1048573,
           123456789, 2, 97, 100100, 99, None, -5]
    res = "\n".join(stringify(x) for x in src)
    d = dt.Frame(src)
    assert d.stypes == (stype.int32, )
    assert d.to_csv() == "C0\n" + res + "\n"


def test_save_int64():
    src = [0, None, 1, 10, 100, 1000, 1000000, 1000000000, 10000000000, -3, 0,
           1000000000000000, 1000000000000000000, 9223372036854775807, 95, 1,
           -9223372036854775807, None, 123456789876543210, -245253356, None,
           328365213640134, 99999999999, 10000000000001, 33, -245, 3]
    res = "\n".join(stringify(x) for x in src)
    d = dt.Frame(src)
    assert d.stypes == (stype.int64, )
    assert d.to_csv() == "C0\n" + res + "\n"


def test_save_double():
    src = ([0.0, -0.0, 1.5, 0.0034876143, 10.3074, 83476101.13487,
            # 7.0785040837745274,
            # 23.898342808714432,
            34981703410983.12, -3.232e-8, -4.241e+67] +
           [10**p for p in range(320) if p != 126])
    d = dt.Frame(src)
    dd = dt.fread(text=d.to_csv())
    assert d.to_list()[0] == dd.to_list()[0]
    # .split() in order to produce better error messages
    assert d.to_csv(hex=True).split("\n") == dd.to_csv(hex=True).split("\n")


def test_save_double2():
    src = [10**i for i in range(-307, 308)]
    res = (["1.0e%02d" % i for i in range(-307, -4)] +
           ["0.0001", "0.001", "0.01", "0.1"] +
           [str(10.0**i) for i in range(15)] +
           ["1.0e+%02d" % i for i in range(15, 308)])
    d = dt.Frame(src)
    assert d.stypes == (stype.float64, )
    assert d.to_csv().split("\n")[1:-1] == res


def test_save_round_doubles():
    src = [1.0, 0.0, -3.0, 123.0, 5e55]
    res = ["1.0", "0.0", "-3.0", "123.0", "5.0e+55"]
    d = dt.Frame(src)
    assert d.stypes == (stype.float64, )
    assert d.to_csv().split("\n")[1:-1] == res


def test_save_hexdouble_subnormal():
    src = [1e-308, 1e-309, 1e-310, 1e-311, 1e-312, 1e-313, 1e-314, 1e-315,
           1e-316, 1e-317, 1e-318, 1e-319, 1e-320, 1e-321, 1e-322, 1e-323,
           0.0, 1e-324, -0.0,
           -1e-323, -1e-300, -2.76e-312, -6.487202e-309]
    d = dt.Frame(src)
    frame_integrity_check(d)
    hexxed = d.to_csv(hex=True).split("\n")[1:-1]  # remove header & last \n
    assert hexxed == [pyhex(v) for v in src]


def test_save_hexdouble_special():
    from math import nan, inf
    src = [nan, inf, -inf, 0]
    d = dt.Frame(src)
    hexxed = d.to_csv(hex=True).split("\n")[1:-1]
    # Note: we serialize nan as an empty string
    assert hexxed == ["", "inf", "-inf", "0x0p+0"]


@pytest.mark.parametrize("seed", [random.randint(0, 2**64 - 1)])
def test_save_hexdouble_random(seed):
    random.seed(seed)
    src = [(1.5 * random.random() - 0.5) * 10**random.randint(-325, 308)
           for _ in range(1000)]
    d = dt.Frame(src)
    hexxed = d.to_csv(hex=True).split("\n")[1:-1]
    assert hexxed == [pyhex(v) for v in src]


def test_save_hexfloat_sample():
    # Manually check these values against Java's generated strings
    src = {
        0: "0x0p+0",
        1: "0x1p+0",
        3: "0x1.8p+1",
        -1: "-0x1p+0",
        -23: "-0x1.7p+4",
        0.001: "0x1.0624dep-10",
        0.0001: "0x1.a36e2ep-14",
        1e-10: "0x1.b7cdfep-34",
        1e+10: "0x1.2a05f2p+33",
        -1000: "-0x1.f4p+9",
        3.1415927: "0x1.921fb6p+1",
        273.9134: "0x1.11e9d4p+8",
        300.00002: "0x1.2c0002p+8",
        99999999: "0x1.7d784p+26",
        123456789: "0x1.d6f346p+26",
        987654321: "0x1.d6f346p+29",  # ...
        5.55e-40: "0x0.0c163ap-126",
        -2.01e-42: "-0x0.000b34p-126",
        1.0e-45: "0x0.000002p-126",
        1.4e-45: "0x0.000002p-126",  # min subnormal
        1.17549435e-38: "0x1p-126",  # min normal
        8.700001e37: "0x1.05ce5ep+126",
        3.4028235e38: "0x1.fffffep+127",  # max
        3.4028236e38: "inf",
        -3e328: "-inf",
    }
    DT = dt.Frame(A=list(src.keys()), stype=stype.float32)
    assert DT.stypes == (stype.float32, )
    hexxed = DT.to_csv(hex=True).split("\n")[1:-1]
    assert hexxed == list(src.values())


def test_save_float_sample():
    src = {
        0: "0.0",
        10: "10.0",
        -100: "-100.0",
        -1000: "-1000.0",
        100000: "100000.0",
        1000000: "1000000.0",
        1000000.2: "1000000.2",
        -10000000: "-10000000.0",
        100000000: "1.0e+08",
        0.1: "0.1",
        0.01: "0.01",
        0.001: "0.001",
        0.0001: "0.0001",
        0.00001: "1.0e-05",
        99.0: "99.0",
        12345678: "12345678.0",
        1e-23: "1.0e-23",
        1e+28: "1.0e+28",
        1e+29: "1.0e+29",
        3.1415927: "3.1415927",
        1.13404e+28: "1.13404e+28",
        1.134043e+28: "1.134043e+28",
    }
    DT = dt.Frame(A=list(src.keys()), stype=stype.float32)
    assert DT.stypes == (stype.float32, )
    decs = DT.to_csv().split("\n")[1:-1]
    assert decs == list(src.values())


def test_save_strings():
    src1 = ["foo", "bar", 'bazz"', 'tri"cky', 'tri""ckier', "simple\nnewline",
            r"A backslash!\n", r"Anoth\\er", "weird\rnewline", "\n\n\n",
            "\x01\x02\x03\x04\x05\x06\x07", '"""""', None]
    src2 = ["", "empty", "None", None, "   with whitespace ", "with,commas",
            "\twith tabs\t", '"oh-no', "single'quote", "'squoted'",
            "\x00bwahaha!", "?", "here be dragons"]
    d = dt.Frame([src1, src2], names=["A", "B"])
    assert d.stypes == (stype.str32, stype.str32)
    frame_integrity_check(d)
    assert d.to_csv().split("\n") == [
        'A,B',
        'foo,""',
        'bar,empty',
        '"bazz""",None',
        '"tri""cky",',
        '"tri""""ckier","   with whitespace "',
        '"simple', 'newline","with,commas"',
        'A backslash!\\n,"\twith tabs\t"',
        'Anoth\\\\er,"""oh-no"',
        '"weird\rnewline","single\'quote"',
        '"', '', '', '","\'squoted\'"',
        '"\x01\x02\x03\x04\x05\x06\x07","\x00bwahaha!"',
        '"""""""""""",?',
        ',here be dragons',
        '']


def test_save_str64():
    src = [["foo", "baar", "ba", None],
           ["idjfbn", "q", None, "bvqpoeqnperoin;dj"]]
    d = dt.Frame(src, stypes=(stype.str32, stype.str64), names=["F", "G"])
    frame_integrity_check(d)
    assert d.stypes == (stype.str32, stype.str64)
    assert d.to_csv() == (
        "F,G\n"
        "foo,idjfbn\n"
        "baar,q\n"
        "ba,\n"
        ",bvqpoeqnperoin;dj\n")


def test_issue_1278():
    f0 = dt.Frame([[True, False] * 10] * 1000)
    a = f0.to_csv()
    # Header + \n + 2 byte per value (0/1 + sep)
    assert len(a) == len(",".join(f0.names)) + 1 + f0.ncols * f0.nrows * 2


def test_save_empty_frame():
    """
    Check that empty frame of sizes (0 x 0), (0 x k), or (n x 0) can be
    written safely without any crashes... See issue #1565
    """
    DT = dt.Frame()
    assert DT.to_csv() == ''
    DT.nrows = 5
    assert DT.to_csv() == ''
    DT[:, "A"] = None
    assert DT.to_csv() == "A" + "\n" * 6
    DT.nrows = 0
    assert DT.to_csv() == "A\n"


def test_save_empty_frame_to_file(tempfile):
    # Check that saving an empty frame to a file will succeed. There was
    # a problem once with `fallocate()` not being able to execute on
    # 0-size file.
    dt.Frame().to_csv(tempfile)
    assert os.path.isfile(tempfile)
    assert os.stat(tempfile).st_size == 0


def test_issue1615():
    """
    Check that to_csv() works correctly with a Frame that has different
    rowindices on different columns.
    """
    DT = dt.Frame(A=range(1000))[::2, :]
    DT.cbind(dt.Frame(B=range(500)))
    RES = dt.fread(DT.to_csv())
    assert_equals(RES, DT)


def test_write_joined_frame():
    # The joined frame will have a rowindex with some rows missing (-1).
    # Check that such frame can be written correctly. See issue #1919.
    DT1 = dt.Frame(A=range(5), B=list('ABCDE'))
    DT1.key = "A"
    DT2 = dt.Frame(A=[3, 7, 11, -2, 0, 1])
    DT = DT2[:, :, dt.join(DT1)]
    out = DT.to_csv()
    assert out == 'A,B\n3,D\n7,\n11,\n-2,\n0,A\n1,B\n'


def test_issue1921():
    n = 1921
    DTA = dt.Frame(A=range(n))
    DTB = dt.repeat(dt.Frame(B=["hey"], stype=dt.str64), n)
    DT = dt.cbind(DTA, DTB)
    out = DT.to_csv()
    assert out == "\n".join(["A,B"] + ["%d,hey" % i for i in range(n)] + [""])


def test_issue2253():
    X = dt.Frame(A=[10, 20])
    X["B"] = dt.f.A > 15
    assert X.to_csv() == "A,B\n" + "10,0\n" + "20,1\n"




#-------------------------------------------------------------------------------
# Test options
#-------------------------------------------------------------------------------

def test_strategy(capsys, tempfile):
    """Check that the _strategy parameter is respected."""
    DT = dt.Frame(A=[5, 6, 10, 12], B=["one", "two", "tree", "for"])
    DT.to_csv(tempfile, _strategy="mmap", verbose=True)
    out, err = capsys.readouterr()
    assert out
    assert err == ""
    DT.to_csv(tempfile, _strategy="write", verbose=True)
    out, err = capsys.readouterr()
    assert out
    assert err == ""


def test_quoting():
    import csv
    DT = dt.Frame(A=range(5),
                  B=[True, False, None, None, True],
                  C=[0.77, -3.14, math.inf, math.nan, 200.001],
                  D=["once", "up'on", "  a time  ", None, ", THE END"])
    DT = DT[:, list("ABCD")]  # Fix column order in Python 3.5

    answer0 = ("A,B,C,D\n"
               "0,1,0.77,once\n"
               "1,0,-3.14,\"up'on\"\n"
               "2,,inf,\"  a time  \"\n"
               "3,,,\n"
               "4,1,200.001,\", THE END\"\n")
    for q in [csv.QUOTE_MINIMAL, "minimal", "MINIMAL"]:
        out = DT.to_csv(quoting=q)
        assert out == answer0

    answer1 = ('"A","B","C","D"\n'
               '"0","1","0.77","once"\n'
               '"1","0","-3.14","up\'on"\n'
               '"2",,"inf","  a time  "\n'
               '"3",,,\n'
               '"4","1","200.001",", THE END"\n')
    for q in [csv.QUOTE_ALL, "all", "ALL"]:
        out = DT.to_csv(quoting=q)
        assert out == answer1

    answer2 = ('"A","B","C","D"\n'
               '0,1,0.77,"once"\n'
               '1,0,-3.14,"up\'on"\n'
               '2,,inf,"  a time  "\n'
               '3,,,\n'
               '4,1,200.001,", THE END"\n')
    for q in [csv.QUOTE_NONNUMERIC, "nonnumeric", "NONNUMERIC"]:
        out = DT.to_csv(quoting=q)
        assert out == answer2

    answer3 = ('A,B,C,D\n'
               '0,1,0.77,once\n'
               '1,0,-3.14,up\'on\n'
               '2,,inf,  a time  \n'
               '3,,,\n'
               '4,1,200.001,, THE END\n')
    for q in [csv.QUOTE_NONE, "none", "NONE"]:
        out = DT.to_csv(quoting=q)
        assert out == answer3


def test_quoting_invalid():
    DT = dt.Frame(A=range(5))

    with pytest.raises(TypeError) as e:
        DT.to_csv(quoting=1.7)
    assert ("Argument `quoting` in Frame.to_csv() should be an integer"
            in str(e.value))

    for q in [-1, 4, 99, "ANY"]:
        with pytest.raises(ValueError) as e:
            DT.to_csv(quoting=q)
        assert ("Invalid value of the `quoting` parameter in Frame.to_csv()"
                in str(e.value))


def test_compress1():
    DT = dt.Frame(A=range(5))
    out = DT.to_csv(compression="gzip")
    expected = (b'\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff'
                b's\xe4\x02\x00\xa5\x85nH\x02\x00\x00\x00'
                b'\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff'
                b'3\xe02\xe42\xe22\xe62\xe1\x02\x00'
                b'\n\x1a\xb4D\n\x00\x00\x00')
    assert isinstance(out, bytes)
    assert len(out) == 52
    # The last byte in the 10-bytes header of each Gzip section is the
    # OS code, which will be different on different platforms. We
    # replace those bytes with \xFF so that the output can be safely
    # compared against the "expected" sample.
    arr = bytearray(out)
    arr[9] = 0xFF
    arr[31] = 0xFF
    out = bytes(arr)
    assert out == expected


def test_compress2(tempfile):
    tempfile += ".gz"
    DT = dt.Frame(A=range(1000), B=["one", "five", "seven", "t"]*250)
    out = DT.to_csv()  # uncompressed
    try:
        DT.to_csv(tempfile, compression="infer")
        assert os.path.isfile(tempfile)
        assert os.stat(tempfile).st_size < len(out)
        IN = dt.fread(tempfile)
        assert_equals(DT, IN)
    finally:
        os.unlink(tempfile)


def test_compress_invalid():
    DT = dt.Frame()
    with pytest.raises(TypeError) as e:
        DT.to_csv(compression=0)
    assert ("Argument `compression` in Frame.to_csv() should be a string, "
            "instead got <class 'int'>" in str(e.value))
    with pytest.raises(ValueError) as e:
        DT.to_csv(compression="rar")
    assert ("Unsupported compression method 'rar' in Frame.to_csv()"
            == str(e.value))



#-------------------------------------------------------------------------------
# Parameter header=
#-------------------------------------------------------------------------------

def test_header():
    DT = dt.Frame(A=[5])
    assert DT.to_csv() == "A\n5\n"
    assert DT.to_csv(header=True) == "A\n5\n"
    assert DT.to_csv(header=False) == "5\n"


def test_header2():
    DT = dt.Frame(A=range(3), B=[5]*3, ZZZ=['yes', 'no', None])
    DT = DT[:, ["A", "B", "ZZZ"]]  # for py3.5
    assert DT.to_csv(header=False) == ("0,5,yes\n"
                                       "1,5,no\n"
                                       "2,5,\n")


def test_header_valid():
    DT = dt.Frame(names=["Q"])
    assert DT.to_csv(header=False) == ''
    assert DT.to_csv(header=True) == 'Q\n'
    assert DT.to_csv(header=None) == 'Q\n'
    assert DT.to_csv(header=...) == 'Q\n'


def test_header_invalid():
    msg = r"Argument `header` in Frame\.to_csv\(\) should be a boolean"
    DT = dt.Frame()
    with pytest.raises(TypeError, match=msg):
        DT.to_csv(header=1)
    with pytest.raises(TypeError, match=msg):
        DT.to_csv(header="yes")
    with pytest.raises(TypeError, match=msg):
        DT.to_csv(header=[])



#-------------------------------------------------------------------------------
# Parameter append=
#-------------------------------------------------------------------------------

def readfile(file):
    with open(file, "r") as tmp:
        return tmp.read()


def test_append_to_new_file(tempfile):
    os.unlink(tempfile)
    DT = dt.Frame(A=[5])
    DT.to_csv(tempfile, append=True)
    assert readfile(tempfile) == "A\n5\n"
    # Write again: the old content should remain in the file
    DT[0, 0] = 3
    DT.to_csv(tempfile, append=True)
    assert readfile(tempfile) == "A\n5\n3\n"


def test_append_to_existing_file(tempfile):
    with open(tempfile, "w") as tmp:
        tmp.write("# text/csv\n")
    DT = dt.Frame(XYZ=range(5))
    DT.to_csv(tempfile, append=True)
    assert readfile(tempfile) == "# text/csv\n0\n1\n2\n3\n4\n"


def test_append_no_file_given():
    DT = dt.Frame(A=[5])
    with pytest.raises(ValueError, match="`append` parameter is set to True, "
                                         "but the output file is not specifi"):
        DT.to_csv(append=True)


def test_append_valid(tempfile):
    with open(tempfile, "w") as tmp:
        tmp.write("@")
    DT = dt.Frame(A=[7])
    DT.to_csv(tempfile, append=True)
    assert readfile(tempfile) == "@7\n"  # auto-detects header=False
    DT.to_csv(tempfile, append=False)
    assert readfile(tempfile) == "A\n7\n"
    DT.to_csv(tempfile, append=None)     # None is same as False
    assert readfile(tempfile) == "A\n7\n"


def test_append_invalid(tempfile):
    os.unlink(tempfile)
    msg = r"Argument `append` in Frame\.to_csv\(\) should be a boolean"
    DT = dt.Frame()
    with pytest.raises(TypeError, match=msg):
        DT.to_csv('tmp', append=1)
    with pytest.raises(TypeError, match=msg):
        DT.to_csv('tmp', append="yes")
    with pytest.raises(TypeError, match=msg):
        DT.to_csv('tmp', append=[])
    assert not os.path.exists(tempfile)


def test_append_with_headers(tempfile):
    DT = dt.Frame(A=[7])
    DT.to_csv(tempfile)
    assert readfile(tempfile) == "A\n7\n"
    DT.to_csv(tempfile, append=True)
    assert readfile(tempfile) == "A\n7\n7\n"
    DT.to_csv(tempfile, append=True, header=True)
    assert readfile(tempfile) == "A\n7\n7\nA\n7\n"
    DT.to_csv(tempfile, append=True, header=False)
    assert readfile(tempfile) == "A\n7\n7\nA\n7\n7\n"


def test_append_strategy(tempfile):
    DT = dt.Frame(D=[3])
    DT.to_csv(tempfile)
    DT.to_csv(tempfile, append=True, _strategy="write")
    assert readfile(tempfile) == "D\n3\n3\n"
    DT.to_csv(tempfile, append=True, _strategy="mmap")
    assert readfile(tempfile) == "D\n3\n3\n3\n"


def test_append_large(tempfile):
    word = "supercalifragilisticexpialidocious"
    n = 10000
    DT = dt.Frame([word] * n, names=["..."])
    DT.to_csv(tempfile)
    DT.to_csv(tempfile, append=True, _strategy="write")
    assert readfile(tempfile) == "...\n" + (word + "\n") * (2*n)
    DT.to_csv(tempfile, append=True, _strategy="mmap")
    assert readfile(tempfile) == "...\n" + (word + "\n") * (3*n)
