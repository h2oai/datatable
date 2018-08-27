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
    a_in = [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1]
    a_reduce = count(a_in)
    assert a_reduce == 10


def test_count_dt_integer():    
    df_in = dt.Frame([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    df_reduce = df_in[:,[count(f.C0),count()]]
    df_reduce.internal.check()
    assert df_reduce.shape == (1, 2)
    assert df_reduce.ltypes == (ltype.int,ltype.int,)
    assert df_reduce.topython() == [[10], [13]]    


def test_count_dt_groupby_integer():    
    df_in = dt.Frame([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    df_reduce = df_in[:, [count(f.C0),count()], "C0"]
    df_reduce.internal.check()
    assert df_reduce.shape == (8, 3)
    assert df_reduce.ltypes == (ltype.int,ltype.int,ltype.int,)
    assert df_reduce.topython() == [[None, 0, 1, 2, 3, 5, 8, 9],
                              [0, 1, 1, 1, 2, 2, 2, 1],
                              [3, 1, 1, 1, 2, 2, 2, 1]]


def test_count_2d_array_integer():
    a_in = [[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
         [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, None, 8, 1]]
    a_reduce = count(a_in)
    assert a_reduce == 2
    
        
def test_count_2d_dt_integer():
    df_in = dt.Frame([[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
                   [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, None, 8, 1]])
    df_reduce = df_in[:, [count(f.C0),count(f.C1),count()]]
    df_reduce.internal.check()
    assert df_reduce.shape == (1, 3)
    assert df_reduce.ltypes == (ltype.int,ltype.int,ltype.int,)
    assert df_reduce.topython() == [[10],[12],[13]]
    

def test_count_2d_dt_groupby_integer():
    df_in = dt.Frame([[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
                   [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, None, 8, 1]])
    df_reduce = df_in[:, [count(f.C0),count(f.C1),count()], "C0"]
    df_reduce.internal.check()
    assert df_reduce.shape == (8, 4)
    assert df_reduce.ltypes == (ltype.int,ltype.int,ltype.int,ltype.int,)
    assert df_reduce.topython() == [[None, 0, 1, 2, 3, 5, 8, 9],
                              [0, 1, 1, 1, 2, 2, 2, 1],
                              [3, 1, 1, 1, 2, 2, 1, 1],
                              [3, 1, 1, 1, 2, 2, 2, 1]]


def test_count_array_string():
    a_in = [None, "blue", "green", "indico", None, None, "orange", "red", 
         "violet", "yellow", "green", None, "blue"]
    a_reduce = count(a_in)
    assert a_reduce == 9


def test_count_dt_string():    
    df_in = dt.Frame([None, "blue", "green", "indico", None, None, "orange", 
                   "red", "violet", "yellow", "green", None, "blue"])
    df_reduce = df_in[:, [count(f.C0),count()]]
    df_reduce.internal.check()
    assert df_reduce.shape == (1, 2)
    assert df_reduce.ltypes == (ltype.int,ltype.int,)
    assert df_reduce.topython() == [[9], [13]]
    
        
def test_count_dt_groupby_string():    
    df_in = dt.Frame([None, "blue", "green", "indico", None, None, "orange", 
                   "red", "violet", "yellow", "green", None, "blue"])
    df_reduce = df_in[:, [count(f.C0),count()], "C0"]
    df_reduce.internal.check()
    assert df_reduce.shape == (8, 3)
    assert df_reduce.ltypes == (ltype.str,ltype.int,ltype.int,)
    assert df_reduce.topython() == [[None, "blue", "green", "indico", "orange", 
                               "red", "violet", "yellow"],
                              [0, 2, 2, 1, 1, 1, 1, 1],
                              [4, 2, 2, 1, 1, 1, 1, 1]]
        
    
def test_count_dt_integer_large(numpy):
    n = 12345678
    a_in = numpy.random.randint(2**20, size=n, dtype=numpy.int32)
    df_in = dt.Frame(a_in)
    df_reduce = df_in[:, count()]
    assert df_reduce.shape == (1, 1)
    assert df_reduce.ltypes == (ltype.int,)
    assert df_reduce.topython() == [[n]]

#-------------------------------------------------------------------------------
# First
#-------------------------------------------------------------------------------

def test_first_array():
    a_in = [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1]
    a_reduce = first(a_in)
    assert a_reduce == 9


def test_first_dt():
    df_in = dt.Frame([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    df_reduce = df_in[:,first(f.C0)]
    df_reduce.internal.check()
    assert df_reduce.shape == (1, 1)
    assert df_reduce.ltypes == (ltype.int,)
    assert df_reduce.topython() == [[9]]

def test_first_dt_range():
    df_in = dt.Frame(A=range(10))[3::3, :]
    df_reduce = df_in[:,first(f.A)]
    df_reduce.internal.check()
    assert df_reduce.shape == (1, 1)
    assert df_reduce.ltypes == (ltype.int,)
    assert df_reduce.topython() == [[3]]


def test_first_dt_groupby():
    df_in = dt.Frame([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    df_reduce = df_in[:, first(f.C0), "C0"]
    df_reduce.internal.check()
    assert df_reduce.shape == (8, 2)
    assert df_reduce.ltypes == (ltype.int,ltype.int,)
    assert df_reduce.topython() == [[None, 0, 1, 2, 3, 5, 8, 9],
                              [None, 0, 1, 2, 3, 5, 8, 9]]

def test_first_dt_integer_large(numpy):
    n = 12345678
    a_in = numpy.random.randint(2**20, size=n, dtype=numpy.int32)
    df_in = dt.Frame(a_in)
    df_reduce = df_in[:, first(f.C0)]
    assert df_reduce.shape == (1, 1)
    assert df_reduce.ltypes == (ltype.int,)
    assert df_reduce.topython() == [[a_in[0]]]


def test_first_2d_dt():
    df_in = dt.Frame([[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
                   [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, 8, None, 1]])
    df_reduce = df_in[:, [first(f.C0),first(f.C1)], "C0"]
    df_reduce.internal.check()
    assert df_reduce.shape == (8, 3)
    assert df_reduce.ltypes == (ltype.int,ltype.int,ltype.int,)
    assert df_reduce.topython() == [[None, 0, 1, 2, 3, 5, 8, 9],
                              [None, 0, 1, 2, 3, 5, 8, 9],
                              [3, 0, 1, 0, 5, 2, 1, 0]]


def test_first_2d_array():
    a_in = [[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
         [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, 8, None, 1]]
    a_reduce = first(a_in)
    assert a_reduce == [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1]
