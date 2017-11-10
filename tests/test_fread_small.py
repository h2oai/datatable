#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import datatable as dt
import pytest
import random


#-------------------------------------------------------------------------------
alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

def random_string(n):
    return "".join(random.choice(alphabet) for _ in range(n))


#-------------------------------------------------------------------------------

@pytest.mark.skip(reason="Not implemented")
def test_empty():
    d0 = dt.fread(text="")
    d1 = dt.fread(text=" ")
    d2 = dt.fread(text="\n")
    d3 = dt.fread(text="  \n" * 3)
    d4 = dt.fread(text="\t\n  \n\n        \t  ")
    for d in [d0, d1, d2, d3, d4]:
        assert d0.shape == (0, 0)


# TODO: also test repl=None, which currently gets deserialized into empty
# strings.
@pytest.mark.parametrize("seed", [random.randint(0, 2**31)])
@pytest.mark.parametrize("repl", ["", "?"])
def test_empty_strings(seed, repl):
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


def test_select_some_columns():
    # * Last field of last line contains separator
    # * The file doesn't end with \n
    # * Only subset of columns is requested
    f = dt.fread('A,B,C\n1,2,"a,b"', columns={'A', 'B'})
    assert f.internal.check()
    assert f.names == ("A", "B")
    assert f.topython() == [[1], [2]]


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
        0,0,,7.2
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
