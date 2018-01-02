#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import datatable as dt
import random
import re
import pytest
from datatable import ltype, stype

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
    d = dt.DataTable([[1, 4, 5], [True, False, None], ["foo", None, "bar"]],
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
    d0 = dt.DataTable({"A": list(range(999983)), "B": [5] * 999983})
    d1 = dt.fread(text=d0.to_csv())
    assert d1.shape == d0.shape
    assert d1.names == d0.names
    assert d1.ltypes == d0.ltypes
    assert d1.topython() == d0.topython()


def test_write_spacestrs():
    # Test that fields having spaces at the beginning/end are auto-quoted
    d = dt.DataTable([" hello ", "world  ", " !", "!", ""])
    assert d.to_csv() == 'C1\n" hello "\n"world  "\n" !"\n!\n""\n'
    dd = dt.fread(text=d.to_csv(), sep='\n')
    assert d.topython() == dd.topython()


def test_write_spacenames():
    d = dt.DataTable({"  foo": [1, 2, 3], "bar ": [1, 2, 3], "": [0, 0, 0]})
    assert d.to_csv() == '"  foo","bar ",""\n1,1,0\n2,2,0\n3,3,0\n'
    dd = dt.fread(text=d.to_csv())
    assert d.topython() == dd.topython()


def test_strategy(capsys, tempfile):
    """Check that the _strategy parameter is respected."""
    d = dt.DataTable({"A": [5, 6, 10, 12], "B": ["one", "two", "tree", "for"]})
    d.to_csv(tempfile, _strategy="mmap", verbose=True)
    out, err = capsys.readouterr()
    assert err == ""
    # assert ("Creating and memory-mapping destination file " + tempfile) in out
    d.to_csv(tempfile, _strategy="write", verbose=True)
    out, err = capsys.readouterr()
    assert err == ""
    # assert ("Creating an empty destination file " + tempfile) in out


@pytest.mark.parametrize("col, scol", [("col", "col"),
                                       ("", '""'),
                                       (" ", '" "'),
                                       ('"', '""""')])
def test_issue507(tempfile, col, scol):
    """
    On Unix platform some of these cases were producing random extra character
    at the end of the file (problem with truncate?). This test verifies that the
    problem doesn't reappear in the future.
    """
    d = dt.DataTable({col: [-0.006492080633259916]})
    d.to_csv(tempfile)
    exp_text = scol + "\n-0.006492080633259916\n"
    assert open(tempfile, "rb").read() == exp_text.encode()



#-------------------------------------------------------------------------------
# Test writing different data types
#-------------------------------------------------------------------------------

def test_save_bool():
    d = dt.DataTable([True, False, None, None, False, False, True, False])
    assert d.stypes == (stype.bool8, )
    assert d.to_csv() == "C1\n1\n0\n\n\n0\n0\n1\n0\n"


def test_save_int8():
    src = [1, 0, -5, None, 127, -127, 99, 10, 100, -100, -10]
    res = "\n".join(stringify(x) for x in src)
    d = dt.DataTable(src)
    assert d.stypes == (stype.int8, )
    assert d.to_csv() == "C1\n" + res + "\n"


def test_save_int16():
    src = [0, 10, 100, 1000, 10000, 32767, -32767, -10, -20, 5, 5125, 3791,
           21, -45, 93, 0, None, 0, None, None, 499, 999, 1001, -8, None, -1]
    res = "\n".join(stringify(x) for x in src)
    d = dt.DataTable(src)
    assert d.stypes == (stype.int16, )
    assert d.to_csv() == "C1\n" + res + "\n"


def test_save_int32():
    src = [0, None, 1, 10, 100, 1000, 1000000, 1000000000, 2147483647,
           -2147483647, -25, 111, 215932, -1, None, 34857304, 999999, -1048573,
           123456789, 2, 97, 100100, 99, None, -5]
    res = "\n".join(stringify(x) for x in src)
    d = dt.DataTable(src)
    assert d.stypes == (stype.int32, )
    assert d.to_csv() == "C1\n" + res + "\n"


def test_save_int64():
    src = [0, None, 1, 10, 100, 1000, 1000000, 1000000000, 10000000000, -3, 0,
           1000000000000000, 1000000000000000000, 9223372036854775807, 95, 1,
           -9223372036854775807, None, 123456789876543210, -245253356, None,
           328365213640134, 99999999999, 10000000000001, 33, -245, 3]
    res = "\n".join(stringify(x) for x in src)
    d = dt.DataTable(src)
    assert d.stypes == (stype.int64, )
    assert d.to_csv() == "C1\n" + res + "\n"


def test_save_double():
    src = ([0.0, -0.0, 1.5, 0.0034876143, 10.3074, 83476101.13487,
            # 7.0785040837745274,
            # 23.898342808714432,
            34981703410983.12, -3.232e-8, -4.241e+67] +
           [10**p for p in range(320) if p != 126])
    d = dt.DataTable(src)
    dd = dt.fread(text=d.to_csv())
    assert d.topython()[0] == dd.topython()[0]
    # .split() in order to produce better error messages
    assert d.to_csv(hex=True).split("\n") == dd.to_csv(hex=True).split("\n")


def test_save_double2():
    src = [10**i for i in range(-307, 308)]
    res = (["1.0e%02d" % i for i in range(-307, -4)] +
           ["0.0001", "0.001", "0.01", "0.1"] +
           [str(10.0**i) for i in range(15)] +
           ["1.0e+%02d" % i for i in range(15, 308)])
    d = dt.DataTable(src)
    assert d.stypes == (stype.float64, )
    assert d.to_csv().split("\n")[1:-1] == res


def test_save_round_doubles():
    src = [1.0, 0.0, -3.0, 123.0, 5e55]
    res = ["1.0", "0.0", "-3.0", "123.0", "5.0e+55"]
    d = dt.DataTable(src)
    assert d.stypes == (stype.float64, )
    assert d.to_csv().split("\n")[1:-1] == res


def test_save_hexdouble_subnormal():
    src = [1e-308, 1e-309, 1e-310, 1e-311, 1e-312, 1e-313, 1e-314, 1e-315,
           1e-316, 1e-317, 1e-318, 1e-319, 1e-320, 1e-321, 1e-322, 1e-323,
           0.0, 1e-324, -0.0,
           -1e-323, -1e-300, -2.76e-312, -6.487202e-309]
    d = dt.DataTable(src)
    assert d.internal.check()
    hexxed = d.to_csv(hex=True).split("\n")[1:-1]  # remove header & last \n
    assert hexxed == [pyhex(v) for v in src]


def test_save_hexdouble_special():
    from math import nan, inf
    src = [nan, inf, -inf, 0]
    d = dt.DataTable(src)
    hexxed = d.to_csv(hex=True).split("\n")[1:-1]
    # Note: we serialize nan as an empty string
    assert hexxed == ["", "inf", "-inf", "0x0p+0"]


@pytest.mark.parametrize("seed", [random.randint(0, 2**64 - 1)])
def test_save_hexdouble_random(seed):
    random.seed(seed)
    src = [(1.5 * random.random() - 0.5) * 10**random.randint(-325, 308)
           for _ in range(1000)]
    d = dt.DataTable(src)
    hexxed = d.to_csv(hex=True).split("\n")[1:-1]
    assert hexxed == [pyhex(v) for v in src]


# TODO: remove dependency on Pandas once #415 is implemented
def test_save_hexfloat_sample(pandas):
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
    d = dt.DataTable(pandas.DataFrame({"A": list(src.keys())}, dtype="float32"))
    assert d.stypes == (stype.float32, )
    hexxed = d.to_csv(hex=True).split("\n")[1:-1]
    assert hexxed == list(src.values())


# TODO: remove dependency on Pandas once #415 is implemented
def test_save_float_sample(pandas):
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
    d = dt.DataTable(pandas.DataFrame(list(src.keys()), dtype="float32"))
    assert d.stypes == (stype.float32, )
    decs = d.to_csv().split("\n")[1:-1]
    assert decs == list(src.values())


def test_save_strings():
    src1 = ["foo", "bar", 'bazz"', 'tri"cky', 'tri""ckier', "simple\nnewline",
            r"A backslash!\n", r"Anoth\\er", "weird\rnewline", "\n\n\n",
            "\x01\x02\x03\x04\x05\x06\x07", '"""""', None]
    src2 = ["", "empty", "None", None, "   with whitespace ", "with,commas",
            "\twith tabs\t", '"oh-no', "single'quote", "'squoted'",
            "\0bwahaha!", "?", "here be dragons"]
    d = dt.DataTable([src1, src2], names=["A", "B"])
    assert d.stypes == (stype.str32, stype.str32)
    assert d.internal.check()
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
        '"weird\rnewline",single\'quote',
        '"', '', '', '",\'squoted\'',
        '"\x01\x02\x03\x04\x05\x06\x07","\x00bwahaha!"',
        '"""""""""""",?',
        ',here be dragons',
        '']
