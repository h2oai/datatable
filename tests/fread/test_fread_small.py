#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
#-------------------------------------------------------------------------------
# Test cases for various fread parse scenarios. These tests are expected to be
# fairly small and straightforward.
#-------------------------------------------------------------------------------
import datatable as dt
from datatable import ltype, stype
import math
import pytest
import random
from tests import random_string



@pytest.mark.parametrize(
    "src", ["", " ", "\n", " \n" * 3, "\t\n  \n\n        \t  ", "\uFEFF",
            "\uFEFF\n", "\uFEFF  \t\n \n\n", "\n\x1A\x1A", "\0", "\0\0\0\0",
            "\n\0", "\uFEFF  \n  \n\t\0\x1A\0\x1A"])
def test_input_empty(tempfile, src):
    with open(tempfile, "w", encoding="utf-8") as o:
        o.write(src)
    d0 = dt.fread(text=src)
    d1 = dt.fread(file=tempfile)
    assert d0.internal.check()
    assert d0.shape == (0, 0)
    assert d1.internal.check()
    assert d1.shape == (0, 0)


def test_input_html():
    with pytest.raises(Exception) as e:
        dt.fread("  \n  <!DOCTYPE html>\n"
                 "<html><body>HA!</body></html>", verbose=True)
    assert "<text> is an HTML file" in str(e)


def test_input_headers_only():
    d0 = dt.fread(text="A,B")
    assert d0.internal.check()
    assert d0.shape == (0, 2)
    d1 = dt.fread(text="AB\n")
    assert d1.internal.check()
    assert d1.shape == (0, 1)


# TODO: also test repl=None, which currently gets deserialized into empty
# strings.
@pytest.mark.parametrize("seed", [random.randint(0, 2**31)])
@pytest.mark.parametrize("repl", ["", "?"])
def test_empty_strings(seed, repl):
    alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
    random.seed(seed)
    ncols = random.randint(3, 10)
    nrows = int(random.expovariate(1 / 200) + 1)
    p = random.uniform(0.1, 0.5)
    src = []
    for i in range(ncols):
        src.append([(random_string(8) if random.random() < p else repl)
                    for j in range(nrows)])
        if all(t == repl for t in src[i]):
            src[i][0] = "!!!"
    colnames = list(alphabet[:ncols].upper())
    d0 = dt.DataTable(src, names=colnames)
    assert d0.names == tuple(colnames)
    assert d0.ltypes == (dt.ltype.str,) * ncols
    text = d0.to_csv()
    d1 = dt.fread(text=text)
    assert d1.internal.check()
    assert d1.names == d0.names
    assert d1.stypes == d0.stypes
    assert d1.topython() == src


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
    assert f.ltypes == (dt.ltype.int, dt.ltype.int, dt.ltype.int, dt.ltype.real)


def test_fread3():
    f = dt.fread("C\n12.345")
    assert f.internal.check()
    assert f.shape == (1, 1)
    assert f.names == ("C", )
    assert f.topython() == [[12.345]]


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
        assert d0.internal.check()
        assert d0.names == names
        assert d0.topython() == [col1, col2, col3]


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


def test_1x1_na():
    d0 = dt.fread("A\n\n")
    assert d0.internal.check()
    assert d0.shape == (1, 1)
    assert d0.names == ("A", )
    assert d0.ltypes == (dt.ltype.bool, )
    assert d0.topython() == [[None]]


def test_last_quoted_field():
    d0 = dt.fread('A,B,C\n1,5,17\n3,9,"1000"')
    assert d0.internal.check()
    assert d0.shape == (2, 3)
    assert d0.ltypes == (dt.ltype.int, dt.ltype.int, dt.ltype.int)
    assert d0.topython() == [[1, 3], [5, 9], [17, 1000]]


def test_numbers_with_quotes1():
    d0 = dt.fread('B,C\n"12"  ,15\n"13"  ,18\n"14"  ,3')
    assert d0.internal.check()
    assert d0.shape == (3, 2)
    assert d0.ltypes == (dt.ltype.int, dt.ltype.int)
    assert d0.names == ("B", "C")
    assert d0.topython() == [[12, 13, 14], [15, 18, 3]]


def test_numbers_with_quotes2():
    d0 = dt.fread('A,B\n83  ,"23948"\n55  ,"20487203497"')
    assert d0.internal.check()
    assert d0.shape == (2, 2)
    assert d0.ltypes == (dt.ltype.int, dt.ltype.int)
    assert d0.names == ("A", "B")
    assert d0.topython() == [[83, 55], [23948, 20487203497]]


def test_unquoting1():
    d0 = dt.fread('"""A"""\n"hello, ""world"""\n""""\n"goodbye"\n')
    assert d0.internal.check()
    assert d0.names == ('"A"',)
    assert d0.topython() == [['hello, "world"', '"', 'goodbye']]


def test_unquoting2():
    d0 = dt.fread("B\nmorning\n'''night'''\n'day'\n'even''ing'\n",
                  quotechar="'")
    assert d0.internal.check()
    assert d0.names == ("B",)
    assert d0.topython() == [["morning", "'night'", "day", "even'ing"]]


def test_unescaping1():
    d0 = dt.fread('"C\\\\D"\n'
                  'AB\\x20CD\\n\n'
                  '"\\"one\\", \\\'two\\\', three"\n'
                  '"\\r\\t\\v\\a\\b\\071\\uABCD"\n')
    assert d0.internal.check()
    assert d0.names == ("C\\D",)
    assert d0.topython() == [["AB CD\n",
                              "\"one\", 'two', three",
                              "\r\t\v\a\b\071\uABCD"]]
