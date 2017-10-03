#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import datatable as dt
from tests import list_equals


def test_save_simple():
    d = dt.DataTable([[1, 4, 5], [True, False, None], ["foo", None, "bar"]],
                     colnames=["A", "B", "C"])
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
    assert d1.types == d0.types
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


def test_save_bool():
    d = dt.DataTable([True, False, None, None, False, False, True, False])
    assert d.types == ("bool", )
    assert d.to_csv() == "C1\n1\n0\n\n\n0\n0\n1\n0\n"


def test_save_int8():
    d = dt.DataTable([1, 0, -5, None, 127, -127, 99, 10, 100, -100, -10])
    assert d.stypes == ("i1i", )
    assert d.to_csv() == "C1\n1\n0\n-5\n\n127\n-127\n99\n10\n100\n-100\n-10\n"


def test_save_int16():
    d = dt.DataTable([0, 10, 100, 1000, 10000, 32767, -32767, -10, -20,
                      5, 5125, 3791, 21, -45, 499, 999, 1001, -8, None, -1])
    assert d.stypes == ("i2i", )
    assert d.to_csv() == "C1\n0\n10\n100\n1000\n10000\n32767\n-32767\n" \
                         "-10\n-20\n5\n5125\n3791\n21\n-45\n499\n999\n" \
                         "1001\n-8\n\n-1\n"


def test_save_int32():
    d = dt.DataTable([0, None, 1, 10, 100, 1000, 1000000, 1000000000,
                      2147483647, -2147483647, -25, 111, 215932, -1,
                      None, 34857304, 999999, -1048573, 123456789])
    assert d.stypes == ("i4i", )
    assert d.to_csv() == "C1\n0\n\n1\n10\n100\n1000\n1000000\n1000000000\n" \
                         "2147483647\n-2147483647\n-25\n111\n215932\n-1\n" \
                         "\n34857304\n999999\n-1048573\n123456789\n"


def test_save_int64():
    d = dt.DataTable([0, None, 1, 10, 100, 1000, 1000000, 1000000000,
                      10000000000, 1000000000000000, 1000000000000000000,
                      9223372036854775807, -9223372036854775807, None,
                      123456789876543210, -245253356, 328365213640134,
                      99999999999, 10000000000001, 33, -245, 3])
    assert d.stypes == ("i8i", )
    assert d.to_csv() == ("C1\n0\n\n1\n10\n100\n1000\n1000000\n1000000000\n"
                          "10000000000\n1000000000000000\n1000000000000000000\n"
                          "9223372036854775807\n-9223372036854775807\n\n"
                          "123456789876543210\n-245253356\n328365213640134\n"
                          "99999999999\n10000000000001\n33\n-245\n3\n")


def test_save_double():
    src = ([0.0, -0.0, 1.5, 0.0034876143, 10.3074, 83476101.13487,
            34981703410983.12, -3.232e-8, -4.241e+67] +
           [10**p for p in range(320)])
    d = dt.DataTable(src)
    dd = dt.fread(text=d.to_csv())
    assert list_equals(d.topython()[0], dd.topython()[0])
    # .split() in order to produce better error messages
    assert d.to_csv(hex=True).split("\n") == dd.to_csv(hex=True).split("\n")
