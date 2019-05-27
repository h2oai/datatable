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
import pytest
import datatable.sphinxext.dtframe_directive as dd


def test_comma_separated():
    assert dd.comma_separated(0) == "0"
    assert dd.comma_separated(1) == "1"
    assert dd.comma_separated(3) == "3"
    assert dd.comma_separated(999) == "999"
    assert dd.comma_separated(9999) == "9999"
    assert dd.comma_separated(10000) == "10,000"
    assert dd.comma_separated(123456) == "123,456"
    assert dd.comma_separated(1234567) == "1,234,567"
    assert dd.comma_separated(12345678) == "12,345,678"
    assert dd.comma_separated(123456789) == "123,456,789"
    assert dd.comma_separated(-1) == "-1"
    assert dd.comma_separated(-1000) == "-1000"
    assert dd.comma_separated(-10000) == "-10,000"
    assert dd.comma_separated(-134689711934) == "-134,689,711,934"


def test_check_parens():
    assert dd.check_parens("", "")
    opening = list("([{")
    closing = list(")]}")
    for i, p1 in enumerate(opening):
        p2 = closing[i]
        assert dd.check_parens(p1, p2)
        assert not dd.check_parens(p2, p1)
        assert not dd.check_parens("", p2)
        assert not dd.check_parens(p1, "")
        for p in opening:
            if p != p1:
                assert not dd.check_parens(p, p2)
        for p in closing:
            if p != p2:
                assert not dd.check_parens(p1, p)


def test_parse_csv_line():
    assert dd.parse_csv_line("") == [""]
    assert dd.parse_csv_line("A,") == ["A", ""]
    assert dd.parse_csv_line(",") == ["", ""]
    assert dd.parse_csv_line(",,  ,") == ["", "", "", ""]
    assert dd.parse_csv_line("1,3,4") == ["1", "3", "4"]
    assert dd.parse_csv_line("...") == ["..."]
    assert dd.parse_csv_line("NA") == [None]
    assert dd.parse_csv_line('"NA"') == ["NA"]
    assert dd.parse_csv_line('[23, 48]') == ['[23', '48]']
    assert dd.parse_csv_line('"hello, world!", 12') == ['hello, world!', "12"]
    assert dd.parse_csv_line('  hi  ,  there  \n') == ['hi', 'there']
    assert dd.parse_csv_line('"  hi  ", "  there "') == ["  hi  ", "  there "]
    assert dd.parse_csv_line("A, NA, B, C") == ["A", None, "B", "C"]
    assert dd.parse_csv_line('"ab\\"cd"') == ["ab\"cd"]
    assert dd.parse_csv_line('"ab\\n"') == ["abn"]
    assert dd.parse_csv_line('"ab "" AB"') == ["ab \" AB"]
    assert dd.parse_csv_line('"ab \'\' AB"') == ["ab '' AB"]


def test_parse_csv_line_error():
    def error(s):
        with pytest.raises(ValueError):
            dd.parse_csv_line(s)

    error('"')
    error("'")
    error("'''")
    error('"Hello"there", 12')



def test_parse_shape():
    assert dd.parse_shape(" 0, 0 ") == (0, 0)
    assert dd.parse_shape("2, 34") == (2, 34)
    assert dd.parse_shape("[0,0]") == (0, 0)
    assert dd.parse_shape("  ( 32 , 111 )  \n") == (32, 111)


def test_parse_shape_invalid():
    def error(s):
        with pytest.raises(ValueError):
            dd.parse_shape(s)

    error("1")
    error("")
    error("3, 4, 5")
    error("(23, ]")
    error("(111, 234]")
    error("(111, 234(")
    error("3.4")
    error("3.4, 5")
    error("22, -22")
    error("-12, 0")
    error("3, 7]")
    error("[12, 88")


def test_parse_types():
    assert dd.parse_types("") == []
    assert dd.parse_types("[]") == []
    assert dd.parse_types("int32") == ["int32"]
    assert dd.parse_types("  [\n int64,\tfloat32\n]  ") == ["int64", "float32"]
    assert dd.parse_types("(int32, float64, ...)") == ["int32", "float64", ...]
    assert dd.parse_types("..., float32") == [..., "float32"]
    assert dd.parse_types("(int8 ,bool8 )") == ["int8", "bool8"]
    assert dd.parse_types("bool8, ..., obj64") == ["bool8", ..., "obj64"]
    all_stypes = "bool8, int8, int16, int32, int64, float32, float64, str32, "\
                 "str64, obj64"
    assert dd.parse_types(all_stypes) == all_stypes.split(", ")


def test_parse_types_invalid():
    def error(s):
        with pytest.raises(ValueError):
            dd.parse_types(s)

    error("in32")
    error("int30")
    error("bool")
    error("int")
    error("int8, ...., float32")
    error("int8, ..., int16, ..., int32")
    error("[..., int8, ...]")
    error("[int8[")
    error(")int8(")
    error("[[bool8]]")
    error("float16")
    error("float8")
    error("NA")


def test_parse_names():
    assert dd.parse_names("") == []
    assert dd.parse_names("[C0, C1, C2]") == ["C0", "C1", "C2"]
    assert dd.parse_names("('one','two')") == ["one", "two"]
    assert dd.parse_names("1,2,3") == ["1", "2", "3"]
    assert dd.parse_names("A, B, ..., Z") == ["A", "B", "...", "Z"]
    assert dd.parse_names("NA, NB, NC") == [None, "NB", "NC"]
