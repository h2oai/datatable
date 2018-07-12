#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
# Test cases for various fread parse scenarios. These tests are expected to be
# fairly small and straightforward.
#-------------------------------------------------------------------------------
import datatable as dt
from datatable import ltype, stype
import math
import os
import pytest
import random
import re
import time
from tests import random_string, list_equals



#-------------------------------------------------------------------------------
# Test parsing of various field types
#-------------------------------------------------------------------------------

def test_bool1():
    d0 = dt.fread("L,T,U,D\n"
                  "true,True,TRUE,1\n"
                  "false,False,FALSE,0\n"
                  ",,,\n")
    d0.internal.check()
    assert d0.shape == (3, 4)
    assert d0.ltypes == (ltype.bool,) * 4
    assert d0.topython() == [[True, False, None]] * 4


def test_bool_truncated():
    d0 = dt.fread("A\nTrue\nFalse\nFal")
    d0.internal.check()
    assert d0.ltypes == (ltype.str,)
    assert d0.topython() == [["True", "False", "Fal"]]


def test_incompatible_bools():
    # check that various styles of bools do not mix
    d0 = dt.fread("A,B,C,D\n"
                  "True,TRUE,true,1\n"
                  "False,FALSE,false,0\n"
                  "TRUE,true,True,TRUE\n")
    d0.internal.check()
    assert d0.ltypes == (ltype.str,) * 4
    assert d0.topython() == [["True", "False", "TRUE"],
                             ["TRUE", "FALSE", "true"],
                             ["true", "false", "True"],
                             ["1", "0", "TRUE"]]


def test_float_hex_random():
    rnd = random.random
    arr = [rnd() * 10**(10**rnd()) for i in range(20)]
    inp = "A\n%s\n" % "\n".join(x.hex() for x in arr)
    d0 = dt.fread(text=inp)
    d0.internal.check()
    assert d0.ltypes == (ltype.real, )
    assert d0.topython() == [arr]


def test_float_hex_formats():
    d0 = dt.fread("A\n"
                  "0x1.0p0\n"
                  "-0x1.0p1\n"
                  "0X1.0P3\n"
                  "0x1.4p3\n"
                  "0x1.9p6\n"
                  "NaN\n"
                  "Infinity\n"
                  "-Infinity")
    d0.internal.check()
    assert d0.topython() == [[1, -2, 8, 10, 100, None, math.inf, -math.inf]]


def test_float_hex0():
    d0 = dt.fread("A\n"
                  "0x0.0p+0\n"
                  "0x0p0\n"
                  "-0x0p-0\n")
    assert d0.topython() == [[0.0] * 3]
    assert math.copysign(1.0, d0.topython()[0][2]) == -1.0


def test_float_hex1():
    d0 = dt.fread(text="A\n"
                       "0x0p0\n"
                       "0x1.5p0\n"
                       "0x1.5p-1\n"
                       "0x1.2AAAAAp+22")
    d0.internal.check()
    assert d0.stypes == (stype.float32, )
    assert d0.topython() == [[0, 1.3125, 0.65625, 4893354.5]]


def test_float_hex2():
    d0 = dt.fread("A\n"
                  "0x1.e04b81cad165ap-1\n"
                  "0x1.fb47e352e9a63p-5\n"
                  "0x1.fa0fd778c351ap-1\n"
                  "0x1.7c0a21cf2b982p-7\n"
                  "0x0.FFFFFFFFFFFFFp-1022\n"  # largest subnormal
                  "0x0.0000000000001p-1022\n"  # smallest subnormal
                  "0x1.FFFFFFFFFFFFFp+1023\n"  # largest normal
                  "0x1.0000000000000p-1022")   # smallest normal
    d0.internal.check()
    assert d0.topython()[0] == [0.9380760727005495, 0.06192392729945053,
                                0.9884021124759415, 0.011597887524058551,
                                2.225073858507201e-308, 4.940656458412e-324,
                                1.7976931348623157e308, 2.2250738585072014e-308]


def test_float_hex_invalid():
    fields = ["0x2.0p1",        # does not start with 0x1. or 0x0.
              "0x1.333",        # no exponent
              "0x1.aaaaaaaaaaaaaaaP1",  # too many digits
              "0x1.ABCDEFGp1",  # non-hex digit "G"
              "0x1.0p-1023",    # exponent is too big
              "0x0.1p1"]        # exponent too small for a subnormal number
    d0 = dt.fread("A,B,C,D,E,F\n" + ",".join(fields))
    d0.internal.check()
    assert d0.topython() == [[f] for f in fields]


def test_float_precision():
    """
    This is a collection of numbers that fread is known to read with up to
    ±1 ulp difference.
    """
    src = [
        0.2396234748320447,  # reads as 0.23962347483204471 (+1 ulp)
        1.378139026373428,   # reads as 1.3781390263734279  (-1 ulp)
        2.543666492383583,   # reads as 2.5436664923835828  (-1 ulp)
        3.656561722844758,   # reads as 3.6565617228447582  (+1 ulp)
        4.018175023192327,   # reads as 4.0181750231923274  (+1 ulp)
        4.04715454502159,    # reads as 4.0471545450215896  (-1 ulp)
        4.413672796373175,   # reads as 4.4136727963731754  (+1 ulp)
        4.909681284908054,   # reads as 4.9096812849080536  (-1 ulp)
        5.66862328123907,    # reads as 5.6686232812390696  (-1 ulp)
        6.712919901967767,   # reads as 6.7129199019677674  (+1 ulp)
        7.304360333549758,   # reads as 7.3043603335497576  (-1 ulp)
        7.58971840634146,    # reads as 7.5897184063414596  (-1 ulp)
        7.964328507593605,   # reads as 7.9643285075936046  (-1 ulp)
        8.65026955243522,    # reads as 8.650269552435219   (-1 ulp)
        10.48519326055973,   # reads as 10.485193260559729  (-1 ulp)
        20.77288912176836,   # reads as 20.772889121768358  (-1 ulp)
    ]
    text = "A\n" + "\n".join(str(x) for x in src)
    d0 = dt.fread(text)
    d0.internal.check()
    assert d0.ltypes == (ltype.real, )
    assert list_equals(d0.topython()[0], src)


def test_float_overflow():
    # Check overflowing float exponents
    fields = ["6e55693457e549ecfce0",
              "1e55555555",
              "-1e+234056",
              "2e-59745"]
    src = "A,B,C,D\n" + ",".join(fields)
    d0 = dt.fread(src)
    d0.internal.check()
    assert d0.shape == (1, len(fields))
    assert d0.ltypes == (ltype.str,) * len(fields)
    assert d0.topython() == [[x] for x in fields]


def test_int():
    d0 = dt.fread("A,B,C\n"
                  "0,0,0\n"
                  "99,999,9999\n"
                  "100,2587,-1000\n"
                  "2147483647,9223372036854775807,99\n"
                  "-2147483647,-9223372036854775807,")
    i64max = 9223372036854775807
    d0.internal.check()
    assert d0.stypes == (stype.int32, stype.int64, stype.int32)
    assert d0.topython() == [[0, 99, 100, 2147483647, -2147483647],
                             [0, 999, 2587, i64max, -i64max],
                             [0, 9999, -1000, 99, None]]


def test_leading0s():
    src = "\n".join("%03d" % i for i in range(1000))
    d0 = dt.fread(src)
    d0.internal.check()
    assert d0.shape == (1000, 1)
    assert d0.topython() == [list(range(1000))]


def test_int_toolong1():
    # check integers that are too long to fit in int64
    src = ["A"] + ["9" * i for i in range(1, 20)]
    d0 = dt.fread("\n".join(src[:-1]))
    d1 = dt.fread("\n".join(src))
    d0.internal.check()
    d1.internal.check()
    assert d0.stypes == (stype.int64, )
    assert d1.stypes == (stype.str32, )
    assert d0.topython() == [[int(x) for x in src[1:-1]]]
    assert d1.topython() == [src[1:]]


def test_int_toolong2():
    d0 = dt.fread("A,B\n"
                  "9223372036854775807,9223372036854775806\n"
                  "9223372036854775808,-9223372036854775808\n")
    d0.internal.check()
    assert d0.ltypes == (ltype.str, ltype.str)
    assert d0.topython() == [["9223372036854775807", "9223372036854775808"],
                             ["9223372036854775806", "-9223372036854775808"]]


def test_int_toolong3():
    # from R issue #2250
    d0 = dt.fread("A,B\n2,384325987234905827340958734572934\n")
    d0.internal.check()
    assert d0.topython() == [[2], ["384325987234905827340958734572934"]]


def test_int_even_longer():
    # Test that integers just above 128 or 256 characters in length parse as
    # strings, not as integers/floats (if the character counter is byte, then
    # it would overflow and fail to recognize that the integer is very long).
    src1 = "1234567890" * 13
    src2 = "25761340981324586079" * 13
    assert len(src1) == 130  # just above 128
    assert len(src2) == 260  # just above 256
    text = "A,B,C,D\n%s,%s,1.%s,%s.99" % (src1, src2, src2, src2)
    d0 = dt.fread(text)
    d0.internal.check()
    assert d0.topython() == [[src1], [src2],
                             [float("1." + src2)],
                             [float(src2)]]


def test_int_with_thousand_sep():
    d0 = dt.fread("A;B;C\n"
                  "5;100;3,378,149\n"
                  "0000;1,234;0001,999\n"
                  "295;500,005;7,134,930\n")
    d0.internal.check()
    assert d0.shape == (3, 3)
    assert d0.names == ("A", "B", "C")
    assert d0.topython() == [[5, 0, 295],
                             [100, 1234, 500005],
                             [3378149, 1999, 7134930]]


def test_int_with_thousand_sep2():
    d0 = dt.fread("A,B,C\n"
                  '3,200,998\n'
                  '"4,785",11,"9,560,293"\n'
                  '17,835,000\n'
                  ',"1,549,048,733,295,668",5354\n')
    d0.internal.check()
    assert d0.stypes == (dt.int32, dt.int64, dt.int32)
    assert d0.topython() == [[3, 4785, 17, None],
                             [200, 11, 835, 1549048733295668],
                             [998, 9560293, 0, 5354]]


def test_int_with_thousand_sep_not_really():
    bad_ints = [",345",
                "1234,567",
                "13,4,488",
                "17,9500,136",
                "2,300,4,800",
                "9,4482",
                "3,800027",
                "723,012,00",
                "900,534,2",
                "967,300,",
                "24,,500"]
    names = tuple("B%d" % i for i in range(len(bad_ints)))
    src = ("\t".join(names) + "\n" +
           "\t".join('"%s"' % x for x in bad_ints) + "\n")
    d0 = dt.fread(src)
    assert d0.names == names
    assert d0.stypes == (dt.str32,) * len(bad_ints)
    assert d0.topython() == [[x] for x in bad_ints]



def test_float_ext_literals1():
    inf = math.inf
    d0 = dt.fread("A\n+Inf\nINF\n-inf\n-Infinity\n1.3e2")
    d0.internal.check()
    assert d0.ltypes == (ltype.real, )
    assert d0.topython() == [[inf, inf, -inf, -inf, 130]]


def test_float_ext_literals2():
    d0 = dt.fread("B\n.2\nnan\nNaN\n-NAN\nqNaN\n+NaN%\n"
                  "sNaN\nNaNQ\nNaNS\n-.999e-1")
    d0.internal.check()
    assert d0.ltypes == (ltype.real, )
    assert d0.topython() == [[0.2, None, None, None, None, None,
                              None, None, None, -0.0999]]


def test_float_ext_literals3():
    d0 = dt.fread("C\n1.0\nNaN3490\n-qNaN9\n+sNaN99999\nNaN000000\nNaN000")
    d0.internal.check()
    assert d0.ltypes == (ltype.real, )
    assert d0.topython() == [[1, None, None, None, None, None]]


def test_float_ext_literals4():
    d0 = dt.fread("D\n1.\n1.#SNAN\n1.#QNAN\n1.#IND\n1.#INF\n")
    d0.internal.check()
    assert d0.ltypes == (ltype.real, )
    assert d0.topython() == [[1, None, None, None, math.inf]]


def test_float_ext_literals5():
    d0 = dt.fread("E\n0e0\n#DIV/0!\n#VALUE!\n#NULL!\n#NAME?\n#NUM!\n"
                  "#REF!\n#N/A\n1e0\n")
    d0.internal.check()
    assert d0.ltypes == (ltype.real, )
    assert d0.topython() == [[0, None, None, None, None, None, None, None, 1]]


def test_float_ext_literals6():
    d0 = dt.fread("F\n1.1\n+1.3333333333333\n5.9e320\n45609E11\n-0890.e-03\n")
    d0.internal.check()
    assert d0.ltypes == (ltype.real, )
    assert d0.topython() == [[1.1, 1.3333333333333, 5.9e320, 45609e11, -0.890]]


def test_float_many_zeros():
    d0 = dt.fread("G\n0.0000000000000000000000000000000000000000000000449548\n")
    d0.internal.check()
    assert d0.ltypes == (ltype.real, )
    assert d0.topython() == [[4.49548e-47]]


def test_invalid_int_numbers():
    d0 = dt.fread('A,B,C\n1,+,4\n2,-,5\n3,-,6\n')
    d0.internal.check()
    assert d0.names == ("A", "B", "C")
    assert d0.topython() == [[1, 2, 3], ["+", "-", "-"], [4, 5, 6]]


def test_invalid_float_numbers():
    d0 = dt.fread("A,B,C,D,E,F\n.,+.,.e,.e+,0e,e-3\n")
    d0.internal.check()
    assert d0.topython() == [["."], ["+."], [".e"], [".e+"], ["0e"], ["e-3"]]



#-------------------------------------------------------------------------------
# Tiny files
#-------------------------------------------------------------------------------

@pytest.mark.parametrize(
    "src", ["", " ", "\n", " \n" * 3, "\t\n  \n\n        \t  ", "\uFEFF",
            "\uFEFF\n", "\uFEFF  \t\n \n\n", "\n\x1A\x1A", "\0", "\0\0\0\0",
            "\n\0", "\uFEFF  \n  \n\t\0\x1A\0\x1A"])
def test_input_empty(tempfile, src):
    with open(tempfile, "w", encoding="utf-8") as o:
        o.write(src)
    d0 = dt.fread(text=src)
    d1 = dt.fread(file=tempfile)
    d0.internal.check()
    assert d0.shape == (0, 0)
    d1.internal.check()
    assert d1.shape == (0, 0)


def test_0x1():
    d0 = dt.fread(text="A")
    d1 = dt.fread("A\n")
    d0.internal.check()
    d1.internal.check()
    assert d0.shape == d1.shape == (0, 1)
    assert d0.names == d1.names == ("A", )
    assert d0.ltypes == d1.ltypes == (ltype.bool, )


def test_0x2(tempfile):
    with open(tempfile, "w") as o:
        o.write("abcd,e")
    d0 = dt.fread(text="abcd,e")
    d1 = dt.fread(tempfile)
    d0.internal.check()
    d1.internal.check()
    assert d0.shape == d1.shape == (0, 2)
    assert d0.names == d1.names == ("abcd", "e")
    assert d0.ltypes == d1.ltypes == (ltype.bool, ltype.bool)


def test_1x2_empty():
    d0 = dt.fread(text=",")
    d0.internal.check()
    assert d0.shape == (1, 2)  # should this be 0x2 ?
    assert d0.names == ("C0", "C1")


def test_headers_line():
    # (similar to previous 2 tests)
    d0 = dt.fread(text="A,B")
    d0.internal.check()
    assert d0.shape == (0, 2)
    d1 = dt.fread(text="AB\n")
    d1.internal.check()
    assert d1.shape == (0, 1)


def test_headers_with_embedded_newline():
    # The newline in the column name will be mangled into a '.'
    d0 = dt.fread('A,B,"C\nD",E')
    assert d0.shape == (0, 4)
    assert d0.names == ("A", "B", "C.D", "E")


def test_fread_single_number1():
    f = dt.fread("C\n12.345")
    f.internal.check()
    assert f.shape == (1, 1)
    assert f.names == ("C", )
    assert f.topython() == [[12.345]]


def test_fread_single_number2():
    f = dt.fread("345.12\n")
    f.internal.check()
    assert f.shape == (1, 1)
    assert f.topython() == [[345.12]]


def test_fread_two_numbers():
    f = dt.fread("12.34\n56.78")
    f.internal.check()
    assert f.shape == (2, 1)
    assert f.topython() == [[12.34, 56.78]]


def test_1x1_na():
    d0 = dt.fread("A\n\n")
    d0.internal.check()
    assert d0.shape == (1, 1)
    assert d0.names == ("A", )
    assert d0.ltypes == (ltype.bool, )
    assert d0.topython() == [[None]]


def test_0x2_na():
    # for 2+ columns empty lines do not mean NA
    d0 = dt.fread("A,B\r\n\r\n")
    d0.internal.check()
    assert d0.shape == (0, 2)
    assert d0.names == ("A", "B")
    assert d0.ltypes == (ltype.bool, ltype.bool)
    assert d0.topython() == [[], []]


def test_0x3_with_whitespace():
    # see issue #673
    d0 = dt.fread("A,B,C\n  ")
    d0.internal.check()
    assert d0.shape == (0, 3)
    assert d0.names == ("A", "B", "C")


def test_1line_not_header():
    d0 = dt.fread(text="C1,C2,3")
    d0.internal.check()
    assert d0.shape == (1, 3)
    assert d0.ltypes == (ltype.str, ltype.str, ltype.int)
    assert d0.topython() == [["C1"], ["C2"], [3]]


def test_input_htmlfile():
    with pytest.raises(Exception) as e:
        dt.fread("  \n  <!DOCTYPE html>\n"
                 "<html><body>HA!</body></html>", verbose=True)
    assert "<text> is an HTML file" in str(e)



#-------------------------------------------------------------------------------
# Small files
#-------------------------------------------------------------------------------

def test_fread1():
    f = dt.fread("hello\n"
                 "1.1\n"
                 "200000\n"
                 "100.3")
    assert f.shape == (3, 1)
    assert f.names == ("hello", )
    assert f.topython() == [[1.1, 200000.0, 100.3]]


def test_fread2():
    f = dt.fread("""
        A,B,C,D
        1,2,3,4
        10,0,,7.2
        ,1,12,3.3333333333
        """)
    assert f.shape == (3, 4)
    assert f.names == ("A", "B", "C", "D")
    assert f.ltypes == (ltype.int, ltype.int, ltype.int, ltype.real)


def test_runaway_quote():
    d0 = dt.fread('"A,B,C\n1,2,3\n4,5,6')
    assert d0.shape == (2, 3)
    assert d0.names == ('"A', 'B', 'C')
    assert d0.topython() == [[1, 4], [2, 5], [3, 6]]


def test_space_separated_numbers():
    # Roughly based on http://www.stats.ox.ac.uk/pub/datasets/csb/ch11b.dat
    src = "\n".join("%03d %03d %04d %4.2f %d"
                    % (i + 1,
                       307 + (i > 87),
                       (900 + i * 10) % 1700,
                       36 + (i * 19034881 % 53 - 25) / 100,
                       int(i > 60))
                    for i in range(100))
    d0 = dt.fread(src)
    d0.internal.check()
    assert d0.shape == (100, 5)
    assert d0.ltypes == (ltype.int, ltype.int, ltype.int, ltype.real,
                         ltype.bool)


def test_utf16(tempfile):
    names = ("alpha", "beta", "gamma")
    col1 = [1.5, 12.999, -4e-6, 2.718281828]
    col2 = ["я", "не", "нездужаю", "нівроку"]
    col3 = [444, 5, -7, 0]
    srctxt = ("\uFEFF" +  # BOM
              ",".join(names) + "\n" +
              "\n".join('%r,"%s",%d' % row for row in zip(col1, col2, col3)))
    for encoding in ["utf-16le", "utf-16be"]:
        with open(tempfile, "wb") as o:
            srcbytes = srctxt.encode(encoding)
            assert srcbytes[0] + srcbytes[1] == 0xFE + 0xFF
            o.write(srcbytes)
        d0 = dt.fread(tempfile)
        d0.internal.check()
        assert d0.names == names
        assert d0.topython() == [col1, col2, col3]


def test_fread_CtrlZ():
    """Check that Ctrl+Z characters at the end of the file are removed"""
    src = b"A,B,C\n-1,2,3\x1A\x1A"
    d0 = dt.fread(text=src)
    d0.internal.check()
    assert d0.ltypes == (dt.ltype.int, dt.ltype.int, dt.ltype.int)
    assert d0.topython() == [[-1], [2], [3]]


def test_fread_NUL():
    """Check that NUL characters at the end of the file are removed"""
    d0 = dt.fread(text=b"A,B\n2.3,5\0\0\0\0\0\0\0\0\0\0")
    d0.internal.check()
    assert d0.ltypes == (dt.ltype.real, dt.ltype.int)
    assert d0.topython() == [[2.3], [5]]


def test_fread_1col_a():
    """Check that it is possible to read 1-column file witn NAs."""
    # We also check that trailing newlines are not discarded in this case
    # (otherwise round-trip with write_csv wouldn't work).
    d0 = dt.fread(text="A\n"
                       "1\n"
                       "2\n"
                       "\n"
                       "4\n"
                       "\n"
                       "5\n"
                       "\n")
    d0.internal.check()
    assert d0.names == ("A",)
    assert d0.topython() == [[1, 2, None, 4, None, 5, None]]


def test_fread_1col_b():
    d1 = dt.fread("QUOTE\n"
                  "If you think\n"
                  "you can do it,\n\n"
                  "you can.\n\n", sep="\n")
    d1.internal.check()
    assert d1.names == ("QUOTE",)
    assert d1.topython() == [["If you think", "you can do it,", "",
                              "you can.", ""]]


@pytest.mark.parametrize("eol", ["\n", "\r", "\n\r", "\r\n", "\r\r\n"])
def test_fread_1col_c(eol):
    data = ["A", "100", "200", "", "400", "", "600"]
    d0 = dt.fread(eol.join(data))
    d0.internal.check()
    assert d0.names == ("A", )
    assert d0.topython() == [[100, 200, None, 400, None, 600]]


@pytest.mark.parametrize("eol", ["\n", "\r", "\n\r", "\r\n", "\r\r\n"])
def test_different_line_endings(eol):
    entries = ["A,B", ",koo", "1,vi", "2,mu", "3,yay"]
    text = eol.join(entries)
    d0 = dt.fread(text=text)
    d0.internal.check()
    assert d0.names == ("A", "B")
    assert d0.topython() == [[None, 1, 2, 3], ["koo", "vi", "mu", "yay"]]


def test_fread_quoting():
    d0 = dt.fread("A,B,C,D\n1,3,ghu,5\n2,4,zifuh,667")
    d0.internal.check()
    assert d0.names == ("A", "B", "C", "D")
    assert d0.topython() == [[1, 2], [3, 4], ["ghu", "zifuh"], [5, 667]]
    d1 = dt.fread('A,B,C,D\n1,3,pq[q,5\n2,4,"dflb",6')
    d1.internal.check()
    assert d1.names == ("A", "B", "C", "D")
    assert d1.topython() == [[1, 2], [3, 4], ["pq[q", "dflb"], [5, 6]]
    d2 = dt.fread('A,B,C,D\n1,3,nib,5\n2,4,"la,la,la,la",6')
    d2.internal.check()
    assert d2.names == ("A", "B", "C", "D")
    assert d2.topython() == [[1, 2], [3, 4], ["nib", "la,la,la,la"], [5, 6]]
    d3 = dt.fread('A,B,C,D\n1,3,foo,5\n2,4,"ba,\\"r,",6')
    d3.internal.check()
    assert d3.names == ("A", "B", "C", "D")
    assert d3.topython() == [[1, 2], [3, 4], ["foo", "ba,\"r,"], [5, 6]]


def test_fread_default_colnames():
    """Check that columns with missing headers get assigned proper names."""
    f0 = dt.fread('A,B,,D\n1,3,foo,5\n2,4,bar,6\n')
    f0.internal.check()
    assert f0.shape == (2, 4)
    assert f0.names == ("A", "B", "C0", "D")
    f1 = dt.fread('0,2,,4\n1,3,foo,5\n2,4,bar,6\n')
    f1.internal.check()
    assert f1.shape == (3, 4)
    assert f1.names == ("C0", "C1", "C2", "C3")
    f2 = dt.fread('A,B,C\nD,E,F\n', header=True)
    f2.internal.check()
    assert f2.names == ("A", "B", "C")
    f3 = dt.fread('A,B,\nD,E,F\n', header=True)
    f3.internal.check()
    assert f3.names == ("A", "B", "C0")



def test_fread_na_field():
    d0 = dt.fread("A,B,C\n1,3,\n2,4,\n")
    d0.internal.check()
    assert d0.names == ("A", "B", "C")
    assert d0.stypes == (stype.int32, stype.int32, stype.bool8)
    assert d0.topython() == [[1, 2], [3, 4], [None, None]]


def test_last_quoted_field():
    d0 = dt.fread('A,B,C\n'
                  '1,5,17\n'
                  '3,9,"1000"')
    d0.internal.check()
    assert d0.shape == (2, 3)
    assert d0.ltypes == (dt.ltype.int, dt.ltype.int, dt.ltype.int)
    assert d0.topython() == [[1, 3], [5, 9], [17, 1000]]


def test_numbers_with_quotes1():
    d0 = dt.fread('B,C\n'
                  '"12"  ,15\n'
                  '"13"  ,18\n'
                  '"14"  ,3')
    d0.internal.check()
    assert d0.shape == (3, 2)
    assert d0.ltypes == (dt.ltype.int, dt.ltype.int)
    assert d0.names == ("B", "C")
    assert d0.topython() == [[12, 13, 14], [15, 18, 3]]


def test_numbers_with_quotes2():
    d0 = dt.fread('A,B\n'
                  '83  ,"23948"\n'
                  '55  ,"20487203497"')
    d0.internal.check()
    assert d0.shape == (2, 2)
    assert d0.ltypes == (dt.ltype.int, dt.ltype.int)
    assert d0.names == ("A", "B")
    assert d0.topython() == [[83, 55], [23948, 20487203497]]


def test_unquoting1():
    d0 = dt.fread('"""A"""\n'
                  '"hello, ""world"""\n'
                  '""""\n'
                  '"goodbye"\n')
    d0.internal.check()
    assert d0.names == ('"A"',)
    assert d0.topython() == [['hello, "world"', '"', 'goodbye']]


def test_unquoting2():
    d0 = dt.fread("B\n"
                  "morning\n"
                  "'''night'''\n"
                  "'day'\n"
                  "'even''ing'\n",
                  quotechar="'")
    d0.internal.check()
    assert d0.names == ("B",)
    assert d0.topython() == [["morning", "'night'", "day", "even'ing"]]


def test_unescaping1():
    d0 = dt.fread('"C\\\\D"\n'
                  'AB\\x20CD\\n\n'
                  '"\\"one\\", \\\'two\\\', three"\n'
                  '"\\r\\t\\v\\a\\b\\071\\uABCD"\n')
    d0.internal.check()
    assert d0.names == ("C\\D",)
    assert d0.topython() == [["AB CD\n",
                              "\"one\", 'two', three",
                              "\r\t\v\a\b\071\uABCD"]]


def test_whitespace_nas():
    d0 = dt.fread('A,   B,    C\n'
                  '17,  34, 2.3\n'
                  '3.,  NA,   1\n'
                  'NA ,  2, NA \n'
                  '0,0.1,0')
    d0.internal.check()
    assert d0.names == ("A", "B", "C")
    assert d0.ltypes == (dt.ltype.real,) * 3
    assert d0.topython() == [[17, 3, None, 0],
                             [34, None, 2, 0.1],
                             [2.3, 1, None, 0]]


def test_quoted_na_strings():
    # Check that na strings are recognized regardless from whether they
    # were quoted or not. See issue #1014
    d0 = dt.fread('A,   B,   C\n'
                  'foo, bar, caw\n'
                  'nan, inf, "inf"\n', na_strings=["nan", "inf"])
    d0.internal.check()
    assert d0.names == ("A", "B", "C")
    assert d0.ltypes == (dt.ltype.str,) * 3
    assert d0.topython() == [["foo", None], ["bar", None], ["caw", None]]


def test_clashing_column_names():
    # there should be no warning; and first column should be C2
    d0 = dt.fread("""C2\n1,2,3,4,5,6,7\n""")
    d0.internal.check()
    assert d0.shape == (1, 7)
    assert d0.names == ("C2", "C3", "C4", "C5", "C6", "C7", "C8")


def test_clashing_column_names2():
    # there should be no warnings; and the second column should retain its name
    d0 = dt.fread("""
        ,C0,,,
        1,2,3,4,5
        6,7,8,9,0
        """)
    d0.internal.check()
    assert d0.shape == (2, 5)
    assert d0.names == ("C1", "C0", "C2", "C3", "C4")


def test_nuls1():
    d0 = dt.fread("A,B\0\n1,2\n")
    d0.internal.check()
    # Special characters are replaced in column names
    assert d0.names == ("A", "B.")
    assert d0.shape == (1, 2)
    assert d0.topython() == [[1], [2]]


def test_nuls2():
    d0 = dt.fread("A,B\nfoo,ba\0r\nalpha,beta\0\ngamma\0,delta\n")
    d0.internal.check()
    assert d0.names == ("A", "B")
    assert d0.shape == (3, 2)
    assert d0.topython() == [["foo", "alpha", "gamma\0"],
                             ["ba\0r", "beta\0", "delta"]]

def test_nuls3():
    lines = ["%02d,%d,%d\0" % (i, i % 3, 20 - i) for i in range(10)]
    src = "\n".join(["a,b,c"] + lines + [""])
    d0 = dt.fread(src, verbose=True)
    d0.internal.check()
    assert d0.shape == (10, 3)
    assert d0.names == ("a", "b", "c")
    assert d0.topython() == [[0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
                             [0, 1, 2, 0, 1, 2, 0, 1, 2, 0],
                             ["%d\0" % i for i in range(20, 10, -1)]]


def test_headers_with_na():
    # Conceivably, this file can also be recognized as `header=False`, owing
    # to the fact that there is an NA value in the first line.
    d = dt.fread("A,B,NA\n"
                 "1,2,3\n")
    d.internal.check()
    assert d.names == ("A", "B", "C0")
    assert d.topython() == [[1], [2], [3]]



#-------------------------------------------------------------------------------
# Medium-size files (generated)
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("seed", [random.randint(0, 2**31)])
@pytest.mark.parametrize("repl", ["", "?"])
def test_empty_strings(seed, repl):
    # TODO: also test repl=None, which currently gets deserialized into empty
    # strings.
    alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
    random.seed(seed)
    ncols = random.randint(3, 10)
    nrows = int(random.expovariate(1 / 200) + 1)
    p = random.uniform(0.1, 0.5)
    src = []
    for i in range(ncols):
        src.append([(random_string(8) if random.random() < p else repl)
                    for j in range(nrows)])
        if src[i] == [repl] * nrows:
            src[i][0] = "!!!"
    colnames = list(alphabet[:ncols].upper())
    d0 = dt.Frame(src, names=colnames)
    assert d0.names == tuple(colnames)
    assert d0.ltypes == (ltype.str,) * ncols
    text = d0.to_csv()
    d1 = dt.fread(text)
    d1.internal.check()
    assert d1.names == d0.names
    assert d1.stypes == d0.stypes
    assert d1.topython() == src


@pytest.mark.parametrize("seed", [random.randint(0, 2**31)])
def test_random_data(seed, tempfile):
    # Based on R tests 880-884
    random.seed(seed)
    n = 110
    src = [
        [random.randint(1, 1000) for _ in range(n)],    # A
        [random.randint(-500, 500) for _ in range(n)],  # B
        [random.gauss(0, 10) for _ in range(n)],        # C
        [random.choice(["abc", "zzz", "ldfvb", "s", "nn"]) for _ in range(n)],
        [random.gauss(0, 10) for _ in range(n)],        # E
        [random.randint(1, 1000) for _ in range(n)],    # F
    ]
    src[1][1] = ""
    src[2][2] = ""
    src[3][3] = ""
    src[3][5] = ""
    src[4][2] = math.inf
    src[4][3] = -math.inf
    src[4][4] = math.nan
    with open(tempfile, "w") as out:
        out.write("A,B,C,D,E,F\n")
        for i in range(n):
            row = ",".join(str(src[j][i]) for j in range(6))
            out.write(row + '\n')
    d1 = dt.fread(tempfile)
    d1.internal.check()
    assert d1.names == tuple("ABCDEF")
    src[1][1] = None
    src[2][2] = None
    src[3][3] = ""    # FIXME
    src[4][4] = None  # .topython() below converts `nan`s into None
    res = d1.topython()
    assert res[0] == src[0]
    assert res[1] == src[1]
    assert list_equals(res[2], src[2])
    assert res[3] == src[3]
    assert list_equals(res[4], src[4])
    assert res[5] == src[5]


def test_int64s_and_typebumps(capsys):
    # Based on R tests 897-899
    n = 2222
    names = ("i32", "i64", "i64-2", "f64-1", "f64-2",
             "s32", "s32-2", "s32-3", "s32-4")
    src = [list(range(n)),  # i32
           [2**40 * 7 * i for i in range(n)],  # i64
           list(range(n)),  # i64-2
           list(range(n)),  # f64-1
           [2**40 * 11 * i for i in range(n)],  # f64-2
           ["f" + "o" * (i % 5 + 1) for i in range(n)],  # s32
           list(range(n)),  # s32-2
           [str(j * 100) for j in range(n)],  # s32-3
           [str(i / 100) for i in range(n)]  # s32-4
           ]
    src[2][111] = 4568034971487098  # i64-2s
    src[3][111] += 0.11  # f64-1
    src[4][111] = 11111111111111.11  #  f64-2
    src[6][111] = "111e"  # s32-2
    src[7][111] = "1111111111111111111111"  # s32-3
    src[8][111] = "1.23e"
    text = ",".join(names) + "\n" + \
           "\n".join(",".join(str(src[j][i]) for j in range(len(src)))
                     for i in range(n))
    f0 = dt.fread(text, verbose=True)
    out, err = capsys.readouterr()
    assert f0.names == names
    assert f0.shape == (n, len(names))
    assert f0.stypes == (stype.int32, stype.int64, stype.int64,
                         stype.float64, stype.float64,
                         stype.str32, stype.str32, stype.str32, stype.str32)
    assert "6 columns need to be re-read" in out
    assert "Column 3 (i64-2) bumped from Int32 to Int64" in out
    assert "Column 9 (s32-4) bumped from Float64 to Str32 due to " \
           "<<1.23e>> on row 111" in out
    f1 = dt.fread(text, verbose=True, columns=f0.stypes)
    out, err = capsys.readouterr()
    assert f1.stypes == f0.stypes
    assert not re.search("columns? needs? to be re-?read", out)
    assert "bumped from" not in out



def test_almost_nodata(capsys):
    # Test datasets where the field is almost always empty
    n = 5111
    src = [["2017", "", "foo"] for i in range(n)]
    src[109][1] = "gotcha"
    m = [""] * n
    m[109] = "gotcha"
    text = "A,B,C\n" + "\n".join(",".join(row) for row in src)
    d0 = dt.fread(text, verbose=True)
    out, err = capsys.readouterr()
    d0.internal.check()
    assert d0.shape == (n, 3)
    assert d0.ltypes == (ltype.int, ltype.str, ltype.str)
    assert d0.topython() == [[2017] * n, m, ["foo"] * n]
    print(out)
    assert ("Column 2 (B) bumped from Unknown to Str32 "
            "due to <<gotcha>> on row 109" in out)



def test_under_allocation(capsys):
    # Test scenarion when fread underestimates the number of rows in a file
    # and consequently allocates too few rows.
    l1 = ["1234567,45789123"] * 200
    l2 = ["7,3"] * 2000
    lines = (l1 + l2) * 100 + l1
    n = len(lines)
    src = "A,B\n" + "\n".join(lines)
    d0 = dt.fread(src, verbose=True)
    out, err = capsys.readouterr()
    mm = re.search("Initial alloc = (\d+) rows", out)
    assert mm
    assert int(mm.group(1)) < n, "Under-allocation didn't happen"
    assert "Too few rows allocated" in out
    d0.internal.check()
    assert d0.shape == (n, 2)
    assert d0.ltypes == (ltype.int, ltype.int)
    assert d0.names == ("A", "B")
    sum1 = 1234567 * 200 * 101 + 7 * 2000 * 100
    sum2 = 45789123 * 200 * 101 + 3 * 2000 * 100
    assert d0.sum().topython() == [[sum1], [sum2]]


@pytest.mark.parametrize("mul", [16, 128, 256, 512, 1024, 2048])
@pytest.mark.parametrize("eol", [True, False])
def test_round_filesize(tempfile, mul, eol):
    """
    Check that nothing bad happens if input file has size which is a multiple
    of memory page size. See R#2194.
    """
    text = b"1234,5678,9012,3456,7890,abcd,4\x0A" * mul
    data = [[1234] * mul, [5678] * mul, [9012] * mul,
            [3456] * mul, [7890] * mul, ["abcd"] * mul, [4] * mul]
    if not eol:
        # No new line at the end of file
        text = text[:-1] + b"0"
        data[-1][-1] = 40
    with open(tempfile, "wb") as o:
        o.write(text)
    filesize = os.stat(tempfile).st_size
    assert filesize == mul * 32
    d0 = dt.fread(tempfile)
    d0.internal.check()
    assert d0.shape == (mul, 7)
    assert d0.topython() == data


def test_maxnrows_on_large_dataset():
    src = "A,B,C\n" + "\n".join("%06d,x,1" % i for i in range(2000000))
    t0 = time.time()
    d0 = dt.fread(src, max_nrows=5, verbose=True)
    t0 = time.time() - t0
    d0.internal.check()
    assert d0.shape == (5, 3)
    assert d0.topython() == [[0, 1, 2, 3, 4], ["x"] * 5, [True] * 5]
    t1 = time.time()
    d1 = dt.fread(src)
    t1 = time.time() - t1
    assert d1.shape == (2000000, 3)
    assert t0 < t1 / 2, ("Reading with max_nrows=5 should be faster than "
                         "reading the whole dataset")


def test_typebumps(capsys):
    lines = ["1,2,3,4"] * 2111
    lines[105] = "Fals,3.5,boo,\"1,000\""
    src = "A,B,C,D\n" + "\n".join(lines)
    d0 = dt.fread(src, verbose=True)
    d0.internal.check()
    out, err = capsys.readouterr()
    assert ("4 columns need to be re-read because their types have changed"
            in out)
    assert ("Column 1 (A) bumped from Bool8/numeric to Str32 due to <<Fals>> "
            "on row 105" in out)
    assert ("Column 2 (B) bumped from Int32 to Float64 due to <<3.5>> on "
            "row 105" in out)
    assert ("Column 3 (C) bumped from Int32 to Str32 due to <<boo>> on "
            "row 105" in out)
    assert ("Column 4 (D) bumped from Int32 to Str32 due to <<\"1,000\">> on "
            "row 105" in out)


def test_too_few_rows():
    lines = ["1,2,3"] * 2500
    lines[111] = "a"
    src = "\n".join(lines)
    with pytest.raises(RuntimeError) as e:
        dt.fread(src, verbose=True)
    assert ("Too few fields on line 112: expected 3 but found only 1"
            in str(e.value))


def test_too_many_rows():
    lines = ["1,2,3"] * 2500
    lines[111] = "0,0,0,0,0"
    src = "\n".join(lines)
    with pytest.raises(RuntimeError) as e:
        dt.fread(src, verbose=True)
    assert ("Too many fields on line 112: expected 3 but more are present"
            in str(e.value))


def test_qr_bump_out_of_sample(capsys):
    lines = ["123,abcd\n"] * 3000
    lines[137] = '-1,"This" is not funny\n'
    src = "".join(lines)
    d0 = dt.fread(src, verbose=True)
    out, err = capsys.readouterr()
    assert "sep = ','" in out
    assert "Quote rule = 0" in out
    assert d0.shape == (3000, 2)
    pysrc = [[123] * 3000, ["abcd"] * 3000]
    pysrc[0][137] = -1
    pysrc[1][137] = '"This" is not funny'
    assert d0.topython() == pysrc
