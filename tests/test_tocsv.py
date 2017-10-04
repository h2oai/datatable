#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import datatable as dt
import re
from tests import list_equals


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
    assert d1.types == d0.types
    assert d1.topython() == d0.topython()


def test_save_simple():
    d = dt.DataTable([[1, 4, 5], [True, False, None], ["foo", None, "bar"]],
                     colnames=["A", "B", "C"])
    out = d.to_csv()
    assert out == "A,B,C\n1,1,foo\n4,0,\n5,,bar\n"


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


def test_save_floats():
    src = [0.0, -0.0, 1.5, 0.0034876143, 10.3074, 83476101.13487,
           34981703410983.12, -3.232e-8, -4.241e+67]
    d = dt.DataTable(src)
    dd = dt.fread(text=d.to_csv())
    assert list_equals(d.topython()[0], dd.topython()[0])
    # .split() in order to produce better error messages
    assert d.to_csv(hex=True).split("\n") == dd.to_csv(hex=True).split("\n")


def test_save_double2():
    src = [10**p for p in range(-307, 308)]
    res = (["1e%02d" % i for i in range(-307, -4)] +
           ["0.0001", "0.001", "0.01", "0.1"] +
           [str(10**i) for i in range(15)] +
           ["1e+%02d" % i for i in range(15, 308)])
    d = dt.DataTable(src)
    assert d.stypes == ("f8r", )
    assert d.to_csv().split("\n")[1:-1] == res


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

def test_save_hexdouble_subnormal():
    src = [1e-308, 1e-309, 1e-310, 1e-311, 1e-312, 1e-313, 1e-314, 1e-315,
           1e-316, 1e-317, 1e-318, 1e-319, 1e-320, 1e-321, 1e-322, 1e-323,
           0.0, 1e-324, -0.0,
           -1e-323, -1e-300, -2.76e-312, -6.487202e-309]
    d = dt.DataTable(src)
    assert d.internal.check()
    hexxed = d.to_csv(hex=True).split("\n")[1:-1]  # remove header & last \n
    assert hexxed == [pyhex(v) for v in src]
