#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
#
# Test aggregating different types of data
#
#-------------------------------------------------------------------------------
import math
import pytest
import datatable as dt
from datatable import ltype, stype, DatatableWarning
from datatable.extras.aggregate import aggregate
from datatable import stype


#-------------------------------------------------------------------------------
# Aggregate 1D 
#-------------------------------------------------------------------------------

def test_aggregate_1d_continuous_integer_sorted():
    d0 = dt.Frame([0, 1, 2, 3, 4, 5, 6, 7, 8, 9])
    d1 = aggregate(d0, 1e-15, 3)
    d1.internal.check()
    assert d1.shape == (10, 2)
    assert d1.ltypes == (ltype.real, ltype.int)
    assert d1.topython() == [[0, 1, 2, 3, 4, 5, 6, 7, 8, 9], 
                             [0, 0, 0, 0, 1, 1, 1, 2, 2, 2]]
    

def test_aggregate_1d_continuous_integer_random():
    d0 = dt.Frame([9, 8, 2, 3, 3, 0, 5, 5, 8, 1])
    d1 = aggregate(d0, 1e-15, 3)
    d1.internal.check()
    assert d1.shape == (10, 2)
    assert d1.ltypes == (ltype.real, ltype.int)
    assert d1.topython() == [[9, 8, 2, 3, 3, 0, 5, 5, 8, 1], 
                             [2, 2, 0, 0, 0, 0, 1, 1, 2, 0]]
    
    
def test_aggregate_1d_continuous_real_sorted():
    d0 = dt.Frame([0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9])
    d1 = aggregate(d0, 1e-15, 3)
    d1.internal.check()
    assert d1.shape == (10, 2)
    assert d1.ltypes == (ltype.real, ltype.int)
    assert d1.topython() == [[0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9], 
                             [0, 0, 0, 0, 1, 1, 1, 2, 2, 2]]
    
     
def test_aggregate_1d_continuous_real_random():
    d0 = dt.Frame([0.7, 0.7, 0.5, 0.1, 0.0, 0.9, 0.1, 0.3, 0.4, 0.2])
    d1 = aggregate(d0, 1e-15, 3)
    d1.internal.check()
    assert d1.shape == (10, 2)
    assert d1.ltypes == (ltype.real, ltype.int)
    assert d1.topython() == [[0.7, 0.7, 0.5, 0.1, 0.0, 0.9, 0.1, 0.3, 0.4, 0.2], 
                             [2, 2, 1, 0, 0, 2, 0, 0, 1, 0]]
    
#-------------------------------------------------------------------------------
# Aggregate 2D 
#-------------------------------------------------------------------------------    
    
def test_aggregate_2d_continuous_integer_sorted():
    d0 = dt.Frame([[0, 1, 2, 3, 4, 5, 6, 7, 8, 9], 
                   [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]])
    d1 = aggregate(d0, 1e-15, 0, 3, 3)
    d1.internal.check()
    assert d1.shape == (10, 3)
    assert d1.ltypes == (ltype.real, ltype.real, ltype.int)
    assert d1.topython() == [[0, 1, 2, 3, 4, 5, 6, 7, 8, 9], 
                             [0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [0, 0, 0, 0, 4, 4, 4, 8, 8, 8]]
    

def test_aggregate_2d_continuous_integer_random():
    d0 = dt.Frame([[9, 8, 2, 3, 3, 0, 5, 5, 8, 1], 
                   [3, 5, 8, 1, 4, 4, 8, 7, 6, 1]])
    d1 = aggregate(d0, 1e-15, 0, 3, 3)
    d1.internal.check()
    assert d1.shape == (10, 3)
    assert d1.ltypes == (ltype.real, ltype.real, ltype.int)
    assert d1.topython() == [[9, 8, 2, 3, 3, 0, 5, 5, 8, 1], 
                             [3, 5, 8, 1, 4, 4, 8, 7, 6, 1], 
                             [2, 5, 6, 0, 3, 3, 7, 7, 8, 0]]


def test_aggregate_2d_continuous_real_sorted():
    d0 = dt.Frame([[0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9], 
                   [0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9]])
    d1 = aggregate(d0, 1e-15, 0, 3, 3)
    d1.internal.check()
    assert d1.shape == (10, 3)
    assert d1.ltypes == (ltype.real, ltype.real, ltype.int)
    assert d1.topython() == [[0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9], 
                             [0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9], 
                             [0, 0, 0, 0, 4, 4, 4, 8, 8, 8]]
    

def test_aggregate_2d_continuous_real_random():
    d0 = dt.Frame([[0.9, 0.8, 0.2, 0.3, 0.3, 0.0, 0.5, 0.5, 0.8, 0.1], 
                   [0.3, 0.5, 0.8, 0.1, 0.4, 0.4, 0.8, 0.7, 0.6, 0.1]])
    d1 = aggregate(d0, 1e-15, 0, 3, 3)
    d1.internal.check()
    assert d1.shape == (10, 3)
    assert d1.ltypes == (ltype.real, ltype.real, ltype.int)
    assert d1.topython() == [[0.9, 0.8, 0.2, 0.3, 0.3, 0.0, 0.5, 0.5, 0.8, 0.1], 
                             [0.3, 0.5, 0.8, 0.1, 0.4, 0.4, 0.8, 0.7, 0.6, 0.1], 
                             [2, 5, 6, 0, 3, 3, 7, 7, 8, 0]]
    
def test_aggregate_1d_categorical_sorted():
    d0 = dt.Frame(["blue", "green", "indigo", "orange", "red", "violet", "yellow"])
    d1 = aggregate(d0, 1e-15, 3)
    d1.internal.check()
    assert d1.shape == (7, 2)
    assert d1.topython() == [["blue", "green", "indigo", "orange", "red", "violet", "yellow"], 
                             [0, 1, 2, 3, 4, 5, 6]]

    
def test_aggregate_1d_categorical_random():
    d0 = dt.Frame(["blue", "orange", "yellow", "green", "blue", "indigo", "violet"])
    d1 = aggregate(d0, 1e-15, 3)
    d1.internal.check()
    assert d1.shape == (7, 2)
    assert d1.topython() == [["blue", "orange", "yellow", "green", "blue", "indigo", "violet"], 
                             [0, 3, 5, 1, 0, 2, 4]]
    
def test_aggregate_2d_categorical_sorted():
    d0 = dt.Frame([["blue", "green", "indigo", "orange", "red", "violet", "yellow"], 
                  ["Friday", "Monday", "Saturday", "Sunday", "Thursday", "Tuesday", "Wednesday"]])
    d1 = aggregate(d0, 1e-15, 0, 7, 7)
    d1.internal.check()
    assert d1.shape == (7, 3)
    assert d1.topython() == [["blue", "green", "indigo", "orange", "red", "violet", "yellow"], 
                             ["Friday", "Monday", "Saturday", "Sunday", "Thursday", "Tuesday", "Wednesday"],
                             [0, 8, 16, 24, 32, 40, 48]]

    
def test_aggregate_2d_categorical_random():
    d0 = dt.Frame([["blue", "indigo", "red", "violet", "yellow", "violet", "red"], 
                  ["Monday", "Monday", "Wednesday", "Saturday", "Thursday", "Friday", "Wednesday"]])
    d1 = aggregate(d0, 1e-15, 0, 7, 7)
    d1.internal.check()
    assert d1.shape == (7, 3)
    assert d1.topython() == [["blue", "indigo", "red", "violet", "yellow", "violet", "red"],
                             ["Monday", "Monday", "Wednesday", "Saturday", "Thursday", "Friday", "Wednesday"],
                             [5, 6, 22, 13, 19, 3, 22]]
    
def test_aggregate_2d_mixed_sorted():
    d0 = dt.Frame([[0, 1, 2, 3, 4, 5, 6], 
                  ["blue", "green", "indigo", "orange", "red", "violet", "yellow"]])
    d1 = aggregate(d0, 1e-15, 0, 7, 7)
    d1.internal.check()
    assert d1.shape == (7, 3)
    assert d1.topython() == [[0, 1, 2, 3, 4, 5, 6], 
                             ["blue", "green", "indigo", "orange", "red", "violet", "yellow"],
                             [0, 8, 16, 24, 32, 40, 48]]

def test_aggregate_2d_mixed_random():
    d0 = dt.Frame([[3, 0, 6, 6, 1, 2, 4], ["blue", "indigo", "red", "violet", "yellow", "violet", "red"]])
    d1 = aggregate(d0, 1e-15, 0, 6, 6)
    d1.internal.check()
    assert d1.shape == (7, 3)
    assert d1.topython() == [[3, 0, 6, 6, 1, 2, 4], 
                             ["blue", "indigo", "red", "violet", "yellow", "violet", "red"],
                             [2, 6, 17, 23, 24, 19, 15]]