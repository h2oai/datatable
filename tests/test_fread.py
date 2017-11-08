#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import pytest
import datatable as dt
import random
import math
from datatable import stype, ltype


#-------------------------------------------------------------------------------
# Tests for the `columns` argument
#-------------------------------------------------------------------------------

def test_fread_columns_slice():
    d0 = dt.fread(text="A,B,C,D,E\n1,2,3,4,5", columns=slice(None, None, 2))
    assert d0.internal.check()
    assert d0.names == ("A", "C", "E")
    assert d0.topython() == [[1], [3], [5]]


def test_fread_columns_range():
    d0 = dt.fread(text="A,B,C,D,E\n1,2,3,4,5", columns=range(3))
    assert d0.internal.check()
    assert d0.names == ("A", "B", "C")
    assert d0.topython() == [[1], [2], [3]]


def test_fread_columns_range_bad1():
    with pytest.raises(ValueError) as e:
        dt.fread(text="A,B,C,D,E\n1,2,3,4,5", columns=range(3, 0, -1))
    assert "Cannot use slice/range with negative step" in str(e.value)


def test_fread_columns_range_bad2():
    with pytest.raises(ValueError) as e:
        dt.fread(text="A,B,C,D,E\n1,2,3,4,5", columns=range(13))
    assert "Invalid range iterator" in str(e.value)


def test_fread_columns_list1():
    d0 = dt.fread(text="A,B,C\n1,2,3", columns=["foo", "bar", "baz"])
    assert d0.internal.check()
    assert d0.names == ("foo", "bar", "baz")
    assert d0.topython() == [[1], [2], [3]]


def test_fread_columns_list2():
    d0 = dt.fread(text="A,B,C\n1,2,3", columns=["foo", None, "baz"])
    assert d0.internal.check()
    assert d0.names == ("foo", "baz")
    assert d0.topython() == [[1], [3]]


def test_fread_columns_list3():
    d0 = dt.fread(text="A,B,C\n1,2,3", columns=[("foo", str), None, None])
    assert d0.internal.check()
    assert d0.names == ("foo", )
    assert d0.topython() == [["1"]]


def test_fread_columns_list_bad1():
    with pytest.raises(ValueError) as e:
        dt.fread(text="C1,C2\n1,2\n3,4\n", columns=["C2"])
    assert ("Input file contains 2 columns, whereas `columns` parameter "
            "specifies only 1 column" in str(e.value))


def test_fread_columns_list_bad2():
    with pytest.raises(TypeError):
        dt.fread(text="C1,C2\n1,2\n3,4\n", columns=["C1", 2])


def test_fread_columns_list_bad3():
    with pytest.raises(ValueError) as e:
        dt.fread(text="C1,C2\n1,2", columns=["C1", ("C2", bytes)])
    assert "Unknown type <class 'bytes'> used as an override for column 'C2'" \
           in str(e)


def test_fread_columns_set1():
    text = ("C1,C2,C3,C4\n"
            "1,3.3,7,\"Alice\"\n"
            "2,,,\"Bob\"")
    d0 = dt.fread(text=text, columns={"C1", "C3"})
    assert d0.internal.check()
    assert d0.names == ("C1", "C3")
    assert d0.topython() == [[1, 2], [7, None]]


def test_fread_columns_set2():
    with pytest.warns(UserWarning) as ws:
        d0 = dt.fread(text="A,B,A\n1,2,3\n", columns={"A"})
    assert d0.names == ("A", "A.1")
    assert d0.topython() == [[1], [3]]
    assert len(ws) == 1
    assert "Duplicate column names found: ['A']" in ws[0].message.args[0]


def test_fread_columns_set_bad():
    with pytest.warns(UserWarning) as ws:
        dt.fread(text="A,B,C\n1,2,3", columns={"A", "foo"})
    assert len(ws) == 1
    assert "Column(s) ['foo'] not found in the input" in ws[0].message.args[0]


def test_fread_columns_dict1():
    d0 = dt.fread(text="C1,C2,C3\n1,2,3\n4,5,6", columns={"C3": "A", "C1": "B"})
    assert d0.names == ("B", "C2", "A")
    assert d0.topython() == [[1, 4], [2, 5], [3, 6]]


def test_fread_columns_dict2():
    d0 = dt.fread(text="A,B,C,D\n1,2,3,4", columns={"A": "a", ...: None})
    assert d0.names == ("a", )
    assert d0.topython() == [[1]]


def test_fread_columns_dict3():
    d0 = dt.fread(text="A,B,C\n1,2,3",
                  columns={"A": ("foo", float), "B": (..., str), "C": None})
    assert d0.names == ("foo", "B")
    assert d0.ltypes == (dt.ltype.real, dt.ltype.str)
    assert d0.topython() == [[1.0], ["2"]]


def test_fread_columns_fn1():
    text = ("A1,A2,A3,A4,A5,x16,b333\n"
            "5,4,3,2,1,0,0\n")
    d0 = dt.fread(text=text, columns=lambda col: int(col[1:]) % 2 == 0)
    assert d0.internal.check()
    assert d0.names == ("A2", "A4", "x16")
    assert d0.topython() == [[4], [2], [0]]




#-------------------------------------------------------------------------------
# Other
#-------------------------------------------------------------------------------

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


def test_fread_logger():
    class MyLogger:
        def __init__(self):
            self.count = 0
            self.msg = ""

        def debug(self, msg):
            self.msg += msg + "\n"
            self.count += 1

    lg = MyLogger()
    dt.fread(text="A\n1\n2\n3", logger=lg)
    assert lg.count > 10
    assert "\n\n" not in lg.msg


def test_fread_nthreads(capsys):
    """Check that the `nthreads` parameter is respected"""
    dt.fread(text="A\n1\n2\n3", verbose=True, nthreads=1)
    out, err = capsys.readouterr()
    assert "Using 1 thread" in out


def test_fread_fillna():
    src = ("Row,bool8,int32,int64,float32x,float64,float64+,float64x,str\n"
           "1,True,1234,1234567890987654321,0x1.123p-03,2.3,-inf,"
           "0x1.123456789abp+100,the end\n"
           "2\n"
           "3\n"
           "4\n"
           "5\n")
    d = dt.fread(text=src, fill=True)
    assert d.internal.check()
    p = d[1:, 1:].topython()
    assert p == [[None] * 4] * 8
