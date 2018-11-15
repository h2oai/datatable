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
import datatable as dt
from datatable import ltype
from datatable.extras.aggregate import aggregate
import inspect
from collections import Counter

#-------------------------------------------------------------------------------
# Get default arguments of a function
#-------------------------------------------------------------------------------

def get_default_args(func):
    signature = inspect.signature(func)
    return {
        k: v.default
        for k, v in signature.parameters.items()
        if v.default is not inspect.Parameter.empty
    }
    
    
#-------------------------------------------------------------------------------
# Test the progress reporting function at the same time
#-------------------------------------------------------------------------------
    
def report_progress(progress, status_code):
    assert status_code in (0, 1)
    assert progress >= 0 and progress <= 1

#-------------------------------------------------------------------------------
# Aggregate 1D
#-------------------------------------------------------------------------------

def test_aggregate_1d_empty():
    n_bins = 1
    d_in = dt.Frame([])
    d_members = aggregate(d_in, min_rows=0, n_bins=n_bins, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (0, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[]]
    d_in.internal.check()
    assert d_in.shape == (0, 1)
    assert d_in.ltypes == (ltype.int,)
    assert d_in.topython() == [[]]
    
    
def test_aggregate_1d_continuous_integer_tiny():
    n_bins = 1
    d_in = dt.Frame([5])
    d_members = aggregate(d_in, min_rows=0, n_bins=n_bins, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (1, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0]]
    d_in.internal.check()
    assert d_in.shape == (1, 2)
    assert d_in.ltypes == (ltype.int, ltype.int)
    assert d_in.topython() == [[5],
                               [1]]
    
    
def test_aggregate_1d_continuous_integer_equal():
    n_bins = 2
    d_in = dt.Frame([0, 0, None, 0, None, 0, 0, 0, 0, 0])
    d_members = aggregate(d_in, min_rows=0, n_bins=n_bins, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[1, 1, 0, 1, 0, 1, 1, 1, 1, 1]]
    d_in.internal.check()
    assert d_in.shape == (2, 2)
    assert d_in.ltypes == (ltype.bool, ltype.int)
    assert d_in.topython() == [[None, 0],
                               [2, 8]]


def test_aggregate_1d_continuous_integer_sorted():
    n_bins = 3
    d_in = dt.Frame([0, 1, None, 2, 3, 4, 5, 6, 7, None, 8, 9])
    d_members = aggregate(d_in, min_rows=0, n_bins=n_bins, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (12, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[1, 1, 0, 1, 1, 2, 2, 2, 3, 0, 3, 3]]
    d_in.internal.check()
    assert d_in.shape == (4, 2)
    assert d_in.ltypes == (ltype.int, ltype.int)
    assert d_in.topython() == [[None, 0, 4, 7],
                               [2, 4, 3, 3]]


def test_aggregate_1d_continuous_integer_random():
    n_bins = 3
    d_in = dt.Frame([None, 9, 8, None, 2, 3, 3, 0, 5, 5, 8, 1, None])
    d_members = aggregate(d_in, min_rows=0, n_bins=n_bins, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (13, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0, 3, 3, 0, 1, 1, 1, 1, 2, 2, 3, 1, 0]]
    d_in.internal.check()
    assert d_in.shape == (4, 2)
    assert d_in.ltypes == (ltype.int, ltype.int)
    assert d_in.topython() == [[None, 2, 5, 9],
                               [3, 5, 2, 3]]


def test_aggregate_1d_continuous_real_sorted():
    n_bins = 3
    d_in = dt.Frame([0.0, None, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9])
    d_members = aggregate(d_in, min_rows=0, n_bins=n_bins, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (11, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[1, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3]]
    d_in.internal.check()
    assert d_in.shape == (4, 2)
    assert d_in.ltypes == (ltype.real, ltype.int)
    assert d_in.topython() == [[None, 0.0, 0.4, 0.7],
                               [1, 4, 3, 3]]


def test_aggregate_1d_continuous_real_random():
    n_bins = 3
    d_in = dt.Frame([0.7, 0.7, 0.5, 0.1, 0.0, 0.9, 0.1, 0.3, 0.4, 0.2, None, None, None])
    d_members = aggregate(d_in, min_rows=0, n_bins=n_bins, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (13, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[3, 3, 2, 1, 1, 3, 1, 1, 2, 1, 0, 0, 0]]
    d_in.internal.check()
    assert d_in.shape == (4, 2)
    assert d_in.ltypes == (ltype.real, ltype.int)
    assert d_in.topython() == [[None, 0.1, 0.5, 0.7],
                               [3, 5, 2, 3]]
    
    
def test_aggregate_1d_categorical_sorted():
    d_in = dt.Frame([None, "blue", "green", "indigo", "orange", "red", "violet",
                     "yellow"])
    d_members = aggregate(d_in, min_rows=0, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (8, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0, 1, 2, 3, 4, 5, 6, 7]]
    d_in.internal.check()
    assert d_in.shape == (8, 2)
    assert d_in.ltypes == (ltype.str, ltype.int)
    assert d_in.topython() == [[None, "blue", "green", "indigo", "orange", "red",
                                "violet", "yellow"],
                               [1, 1, 1, 1, 1, 1, 1, 1]]


def test_aggregate_1d_categorical_unsorted():
    d_in = dt.Frame(["blue", "orange", "yellow", None, "green", "blue", "indigo",
                     None, "violet"])
    d_members = aggregate(d_in, min_rows=0, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (9, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[1, 4, 6, 0, 2, 1, 3, 0, 5]]
    d_in.internal.check()
    assert d_in.shape == (7, 2)
    assert d_in.ltypes == (ltype.str, ltype.int)
    assert d_in.topython() == [[None, "blue", "green", "indigo", "orange", "violet",
                                "yellow"],
                               [2, 2, 1, 1, 1, 1, 1]]
    
    
# Disable some of the checks for the moment, as even with the same seed 
# random generator may behave differently on different platforms.
def test_aggregate_1d_categorical_sampling():
    d_in = dt.Frame(["blue", "orange", "yellow", None, "green", "blue", "indigo",
                     None, "violet"])
    d_members = aggregate(d_in, n_bins=3, min_rows=0, progress_fn=report_progress, seed=1)
    d_members.internal.check()
    assert d_members.shape == (9, 1)
    assert d_members.ltypes == (ltype.int,)
#     assert d_members.topython() == [[3, None, 2, 0, 1, 3, None, 0, None]]
    d_in.internal.check()
    assert d_in.shape == (3, 2)
    assert d_in.ltypes == (ltype.str, ltype.int)
#     assert d_in.topython() == [[None, 'green', 'yellow', 'blue'],
#                                [2, 1, 1, 2]]


#-------------------------------------------------------------------------------
# Aggregate 2D
#-------------------------------------------------------------------------------

def test_aggregate_2d_continuous_integer_sorted():
    nx_bins = 3
    ny_bins = 3
    d_in = dt.Frame([[None, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
                     [0, None, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9]])
    d_members = aggregate(d_in, min_rows=0, nx_bins=nx_bins, ny_bins=ny_bins,
                          progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (12, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[1, 0, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4]]
    d_in.internal.check()
    assert d_in.shape == (5, 3)
    assert d_in.ltypes == (ltype.int, ltype.int, ltype.int)
    assert d_in.topython() == [[0, None, 0, 4, 7],
                               [None, 0, 0, 4, 7],
                               [1, 1, 4, 3, 3]]


def test_aggregate_2d_continuous_integer_random():
    nx_bins = 3
    ny_bins = 3
    d_in = dt.Frame([[9, None, 8, 2, 3, 3, 0, 5, 5, 8, 1],
                     [3, None, 5, 8, 1, 4, 4, 8, 7, 6, 1]])
    d_members = aggregate(d_in, min_rows=0, nx_bins=nx_bins, ny_bins=ny_bins,
                          progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (11, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[2, 0, 4, 5, 1, 3, 3, 6, 6, 7, 1]]
    d_in.internal.check()
    assert d_in.shape == (8, 3)
    assert d_in.ltypes == (ltype.int, ltype.int, ltype.int)
    assert d_in.topython() == [[None, 3, 9, 3, 8, 2, 5, 8],
                               [None, 1, 3, 4, 5, 8, 8, 6],
                               [1, 2, 1, 2, 1, 1, 2, 1]]


def test_aggregate_2d_continuous_real_sorted():
    nx_bins = 3
    ny_bins = 3
    d_in = dt.Frame([[0.0, 0.0, 0.1, 0.2, None, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, None],
                     [None, 0.0, 0.1, 0.2, None, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.0]])
    d_members = aggregate(d_in, min_rows=0, nx_bins=nx_bins, ny_bins=ny_bins,
                          progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (13, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[1, 3, 3, 3, 0, 3, 4, 4, 4, 5, 5, 5, 2]]
    d_in.internal.check()
    assert d_in.shape == (6, 3)
    assert d_in.ltypes == (ltype.real, ltype.real, ltype.int)
    assert d_in.topython() == [[None, 0.0, None, 0.0, 0.4, 0.7],
                               [None, None, 0.0, 0.0, 0.4, 0.7],
                               [1, 1, 1, 4, 3, 3]]


def test_aggregate_2d_continuous_real_random():
    nx_bins = 3
    ny_bins = 3
    d_in = dt.Frame([[None, 0.9, 0.8, 0.2, 0.3, None, 0.3, 0.0, 0.5, 0.5, 0.8, 0.1],
                     [None, 0.3, 0.5, 0.8, 0.1, None, 0.4, 0.4, 0.8, 0.7, 0.6, 0.1]])
    d_members = aggregate(d_in, min_rows=0, nx_bins=nx_bins, ny_bins=ny_bins,
                          progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (12, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0, 2, 4, 5, 1, 0, 3, 3, 6, 6, 7, 1]]
    d_in.internal.check()
    assert d_in.shape == (8, 3)
    assert d_in.ltypes == (ltype.real, ltype.real, ltype.int)
    assert d_in.topython() == [[None, 0.3, 0.9, 0.3, 0.8, 0.2, 0.5, 0.8],
                               [None, 0.1, 0.3, 0.4, 0.5, 0.8, 0.8, 0.6],
                               [2, 2, 1, 2, 1, 1, 2, 1]]


def test_aggregate_2d_categorical_sorted():
    d_in = dt.Frame([[None, None, "abc", "blue", "green", "indigo", "orange", "red", 
                      "violet", "yellow"],
                     [None, "abc", None, "Friday", "Monday", "Saturday", "Sunday", "Thursday",
                      "Tuesday", "Wednesday"]])
    d_members = aggregate(d_in, min_rows=0, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0, 2, 1, 3, 4, 5, 6, 7, 8, 9]]
    d_in.internal.check()
    assert d_in.shape == (10, 3)
    assert d_in.ltypes == (ltype.str, ltype.str, ltype.int)
    assert d_in.topython() == [[None, "abc", None, "blue", "green", "indigo", "orange", "red",
                                "violet", "yellow"],
                               [None, None, "abc", "Friday", "Monday", "Saturday", "Sunday",
                                "Thursday", "Tuesday", "Wednesday"],
                               [1, 1, 1, 1, 1, 1, 1, 1, 1, 1]]


def test_aggregate_2d_categorical_unsorted():
    d_in = dt.Frame([["blue", "indigo", "red", "violet", "yellow", "violet",
                      "red"],
                     ["Monday", "Monday", "Wednesday", "Saturday", "Thursday",
                      "Friday", "Wednesday"]])

    d_members = aggregate(d_in, min_rows=0, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (7, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[1, 2, 5, 3, 4, 0, 5]]
    d_in.internal.check()
    assert d_in.shape == (6, 3)
    assert d_in.ltypes == (ltype.str, ltype.str, ltype.int)
    assert d_in.topython() == [['violet', 'blue', 'indigo', 'violet', 'yellow',
                                'red'],
                               ['Friday', 'Monday', 'Monday', 'Saturday',
                                'Thursday', 'Wednesday'],
                               [1, 1, 1, 1, 1, 2]]
    
    
# Disable some of the checks for the moment, as even with the same seed 
# random generator may behave differently on different platforms.
def test_aggregate_2d_categorical_sampling():
    d_in = dt.Frame([["blue", None, "indigo", "red", "violet", "yellow", "violet",
                      "green", None, None],
                     ["Monday", "abc", "Monday", "Wednesday", "Saturday", "Thursday",
                      "Friday", "Wednesday", "def", "ghi"]])

    d_members = aggregate(d_in, nx_bins=2, ny_bins=2, min_rows=0, 
                          progress_fn=report_progress, seed=1)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
#     assert d_members.topython() == [[...]]
    d_in.internal.check()
    assert d_in.shape == (4, 3)
    assert d_in.ltypes == (ltype.str, ltype.str, ltype.int)
#     assert d_in.topython() == [[...],
#                                [...],
#                                [...]]


def test_aggregate_2d_mixed_sorted():
    nx_bins = 7
    d_in = dt.Frame([[None, None, 0, 0, 1, 2, 3, 4, 5, 6],
                     [None, "a", None, "blue", "green", "indigo", "orange", "red", 
                      "violet", "yellow"]])
    d_members = aggregate(d_in, min_rows=0, nx_bins=nx_bins, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0, 2, 1, 3, 4, 5, 6, 7, 8, 9]]
    d_in.internal.check()
    assert d_in.shape == (10, 3)
    assert d_in.ltypes == (ltype.int, ltype.str, ltype.int)
    assert d_in.topython() == [[None, 0, None, 0, 1, 2, 3, 4, 5, 6],
                               [None, None, "a", "blue", "green", "indigo", "orange", "red",
                                "violet", "yellow"],
                               [1, 1, 1, 1, 1, 1, 1, 1, 1, 1]]


def test_aggregate_2d_mixed_random():
    nx_bins = 6
    d_in = dt.Frame([[1, 3, 0, None, None, 6, 6, None, 1, 2, 4],
                     [None, "blue", "indigo", "abc", "def", "red", "violet", "ghi", "yellow", 
                      "violet", "red"]])
    d_members = aggregate(d_in, min_rows=0, nx_bins=nx_bins, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (11, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0, 2, 3, 1, 1, 5, 7, 1, 8, 6, 4]]
    d_in.internal.check()
    assert d_in.shape == (9, 3)
    assert d_in.ltypes == (ltype.int, ltype.str, ltype.int)
    assert d_in.topython() == [[1, None, 3, 0, 4, 6, 2, 6, 1],
                               [None, 'abc', 'blue', 'indigo', 'red', 'red', 'violet',
                                'violet', 'yellow'],
                               [1, 3, 1, 1, 1, 1, 1, 1, 1]]


#-------------------------------------------------------------------------------
# Aggregate ND, take into account ND is parallelized with OpenMP
#-------------------------------------------------------------------------------    
    
def test_aggregate_3d_categorical():
    args = get_default_args(aggregate)
    rows = args["min_rows"] * 2
    nd_max_bins_in = rows
    a_in = [["blue"] * rows, ["orange"] * rows, ["yellow"] * rows]
    members_count = [[1] * rows]
    exemplar_id = [i for i in range(rows)]
    d_in = dt.Frame(a_in)
                    
    d_members = aggregate(d_in, nd_max_bins = nd_max_bins_in, progress_fn=report_progress)
    assert d_members.shape == (rows, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [exemplar_id]
    d_in.internal.check()
    assert d_in.shape == (rows, 4)
    assert d_in.ltypes == (ltype.str, ltype.str, ltype.str, ltype.int)
    assert d_in.topython() == a_in + members_count
    
    
def test_aggregate_3d_real():
    d_in = dt.Frame([[0.95, 0.50, 0.55, 0.10, 0.90, 0.50, 0.90, 0.50, 0.90, 1.00],
                     [1.00, 0.55, 0.45, 0.05, 0.95, 0.45, 0.90, 0.40, 1.00, 0.90],
                     [0.90, 0.50, 0.55, 0.00, 1.00, 0.50, 0.95, 0.45, 0.95, 0.95]])
    d_members = aggregate(d_in, min_rows=0, nd_max_bins=3, progress_fn=report_progress)
    a_members = d_members.topython()[0]
    d = d_in.sort("C0")
    ri = d.internal.rowindex.tolist()    
    for i, member in enumerate(a_members):
        a_members[i] = ri.index(member)
    
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert a_members == [2, 1, 1, 0, 2, 1, 2, 1, 2, 2]
    
    d_in.internal.check()
    assert d_in.shape == (3, 4)
    assert d_in.ltypes == (ltype.real, ltype.real, ltype.real, ltype.int)

    assert d.topython() == [[0.10, 0.50, 0.95],
                            [0.05, 0.55, 1.00],
                            [0.00, 0.50, 0.90],
                            [1, 4, 5]]


def test_aggregate_nd_direct():
    args = get_default_args(aggregate)
    aggregate_nd(args["max_dimensions"]//2)


def test_aggregate_nd_projection():
    args = get_default_args(aggregate)
    aggregate_nd(args["max_dimensions"]*2)


def aggregate_nd(nd):
    nrows = 1000
    div = 50
    column = [i%div for i in range(nrows)]
    matrix = [column for i in range(nd)] 
    out_types = [ltype.int]*nd + [ltype.int]
    out_value = [[i for i in range(div)]]*nd + [[nrows//div for i in range(div)]]
        
    d_in = dt.Frame(matrix)
    d_members = aggregate(d_in, min_rows=0, nd_max_bins=div, seed=1,
                          progress_fn=report_progress)

    a_members = d_members.topython()[0]
    d = d_in.sort("C0")
    ri = d.internal.rowindex.tolist()    
    for i, member in enumerate(a_members):
        a_members[i] = ri.index(member)

    d_members.internal.check()
    assert d_members.shape == (nrows, 1)
    assert d_members.ltypes == (ltype.int,)
    assert a_members == column
    d_in.internal.check()
    assert d_in.shape == (div, nd+1)
    assert d_in.ltypes == tuple(out_types)
    assert d.topython() == out_value
