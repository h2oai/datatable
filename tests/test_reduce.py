#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable as dt
from datatable import f, stype, ltype, first, count


#-------------------------------------------------------------------------------
# Count
#-------------------------------------------------------------------------------

def test_count_array_integer():
    a = [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1]
    af = count(a)
    assert af == 10


def test_count_dt_integer():    
    df = dt.Frame([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    dfg = df(select=[count(f.C0),count()])
    dfg.internal.check()
    assert dfg.shape == (1, 2)
    assert dfg.ltypes == (ltype.int,ltype.int,)
    assert dfg.topython() == [[10], [13]]    


def test_count_dt_groupby_integer():    
    df = dt.Frame([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    dfg = df(select=[count(f.C0),count()], groupby="C0")
    dfg.internal.check()
    assert dfg.shape == (8, 3)
    assert dfg.ltypes == (ltype.int,ltype.int,ltype.int,)
    assert dfg.topython() == [[None, 0, 1, 2, 3, 5, 8, 9],
                              [0, 1, 1, 1, 2, 2, 2, 1],
                              [3, 1, 1, 1, 2, 2, 2, 1]]


def test_count_2d_array_integer():
    a = [[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
         [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, None, 8, 1]]
    af = count(a)
    assert af == 2
    
        
def test_count_2d_dt_integer():
    df = dt.Frame([[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
                   [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, None, 8, 1]])
    dfg = df(select=[count(f.C0),count(f.C1),count()])
    dfg.internal.check()
    assert dfg.shape == (1, 3)
    assert dfg.ltypes == (ltype.int,ltype.int,ltype.int,)
    assert dfg.topython() == [[10],[12],[13]]
    

def test_count_2d_dt_groupby_integer():
    df = dt.Frame([[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
                   [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, None, 8, 1]])
    dfg = df(select=[count(f.C0),count(f.C1),count()], groupby="C0")
    dfg.internal.check()
    assert dfg.shape == (8, 4)
    assert dfg.ltypes == (ltype.int,ltype.int,ltype.int,ltype.int,)
    assert dfg.topython() == [[None, 0, 1, 2, 3, 5, 8, 9],
                              [0, 1, 1, 1, 2, 2, 2, 1],
                              [3, 1, 1, 1, 2, 2, 1, 1],
                              [3, 1, 1, 1, 2, 2, 2, 1]]


def test_count_array_string():
    a = [None, "blue", "green", "indico", None, None, "orange", "red", 
         "violet", "yellow", "green", None, "blue"]
    af = count(a)
    assert af == 9


def test_count_dt_string():    
    df = dt.Frame([None, "blue", "green", "indico", None, None, "orange", 
                   "red", "violet", "yellow", "green", None, "blue"])
    dfg = df(select=[count(f.C0),count()])
    dfg.internal.check()
    assert dfg.shape == (1, 2)
    assert dfg.ltypes == (ltype.int,ltype.int,)
    assert dfg.topython() == [[9], [13]]
    
        
def test_count_dt_groupby_string():    
    df = dt.Frame([None, "blue", "green", "indico", None, None, "orange", 
                   "red", "violet", "yellow", "green", None, "blue"])
    dfg = df(select=[count(f.C0),count()], groupby="C0")
    dfg.internal.check()
    assert dfg.shape == (8, 3)
    assert dfg.ltypes == (ltype.str,ltype.int,ltype.int,)
    assert dfg.topython() == [[None, "blue", "green", "indico", "orange", 
                               "red", "violet", "yellow"],
                              [0, 2, 2, 1, 1, 1, 1, 1],
                              [4, 2, 2, 1, 1, 1, 1, 1]]
        
    
def test_count_dt_integer_large(numpy):
    n = 12345678
    a = numpy.random.randint(2**20, size=n, dtype=numpy.int32)
    df = dt.Frame(a)
    dfg = df(select=[count()])
    assert dfg.shape == (1, 1)
    assert dfg.ltypes == (ltype.int,)
    assert dfg.topython() == [[n]]
    
    
    
#-------------------------------------------------------------------------------
# First
#-------------------------------------------------------------------------------

def test_first_array():
    a = [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1]
    af = first(a)
    assert af == 9


def test_first_dt():
    df = dt.Frame([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    dfg = df(select=first(f.C0))
    dfg.internal.check()
    assert dfg.shape == (1, 1)
    assert dfg.ltypes == (ltype.int,)
    assert dfg.topython() == [[9]]
    

def test_first_dt_groupby():
    df = dt.Frame([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    dfg = df(select=first(f.C0), groupby="C0")
    dfg.internal.check()
    assert dfg.shape == (8, 2)
    assert dfg.ltypes == (ltype.int,ltype.int,)
    assert dfg.topython() == [[None, 0, 1, 2, 3, 5, 8, 9],
                              [None, 0, 1, 2, 3, 5, 8, 9]]
    
    
def test_first_dt_integer_large(numpy):
    n = 12345678
    a = numpy.random.randint(2**20, size=n, dtype=numpy.int32)
    df = dt.Frame(a)
    dfg = df(select=[first(f.C0)])
    assert dfg.shape == (1, 1)
    assert dfg.ltypes == (ltype.int,)
    assert dfg.topython() == [[a[0]]]

    
def test_first_2d_dt():
    df = dt.Frame([[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
                   [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, 8, None, 1]])
    dfg = df(select=[first(f.C0),first(f.C1)], groupby="C0")
    dfg.internal.check()
    assert dfg.shape == (8, 3)
    assert dfg.ltypes == (ltype.int,ltype.int,ltype.int,)
    assert dfg.topython() == [[None, 0, 1, 2, 3, 5, 8, 9],
                              [None, 0, 1, 2, 3, 5, 8, 9],
                              [3, 0, 1, 0, 5, 2, 1, 0]]


def test_first_2d_array():
    a = [[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
         [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, 8, None, 1]]
    af = first(a)
    assert af == [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1]

