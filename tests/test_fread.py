#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import pytest
import datatable


def test_fread_columns_set():
    text = ("C1,C2,C3,C4\n"
            "1,3.3,7,\"Alice\"\n"
            "2,,,\"Bob\"")
    d0 = datatable.fread(text=text, columns={"C1", "C3"})
    assert d0.internal.check()
    assert d0.names == ("C1", "C3")
    assert d0.topython() == [[1, 2], [7, None]]


def test_fread_columns_fn1():
    text = ("A1,A2,A3,A4,A5,x16,b333\n"
            "5,4,3,2,1,0,0\n")
    d0 = datatable.fread(text=text,
                         columns=lambda col: int(col[1:]) % 2 == 0)
    assert d0.internal.check()
    assert d0.names == ("A2", "A4", "x16")
    assert d0.topython() == [[4], [2], [0]]


def test_fread_columns_bad():
    with pytest.raises(ValueError) as e:
        datatable.fread(text="C1,C2\n1,2\n3,4\n", columns=["C2"])
    assert ("Input file contains 2 columns, whereas `columns` parameter "
            "specifies only 1 column" in str(e.value))
