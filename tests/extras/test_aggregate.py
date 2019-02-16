#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------
#
# Test 0D, 1D, 2D and ND aggregators.
#
#-------------------------------------------------------------------------------
import datatable as dt
from datatable import ltype
from datatable.extras import aggregate
from tests import assert_equals
from datatable.internal import frame_column_rowindex


#-------------------------------------------------------------------------------
# Test the progress reporting function at the same time
#-------------------------------------------------------------------------------

def report_progress(progress, status_code):
    assert status_code in (0, 1)
    assert progress >= 0 and progress <= 1


#-------------------------------------------------------------------------------
# Aggregate 0D
#-------------------------------------------------------------------------------

# Issue #1429
def test_aggregate_0d_continuous_integer_random():
    n_bins = 3  # `nrows < min_rows`, so we also test that this input is ignored
    min_rows = 500
    d_in = dt.Frame([None, 9, 8, None, 2, 3, 3, 0, 5, 5, 8, 1, None])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=min_rows, n_bins=n_bins,
                                         progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (13, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[0, 12, 10, 1, 5, 6, 7, 3, 8, 9, 11, 4, 2]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (13, 2)
    assert d_exemplars.ltypes == (ltype.int, ltype.int)
    assert d_exemplars.to_list() == [[None, None, None, 0, 1, 2, 3, 3, 5, 5, 8, 8, 9],
                                     [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1]]
    assert_equals(d_in, d_in_copy)

#-------------------------------------------------------------------------------
# Aggregate 1D
#-------------------------------------------------------------------------------

def test_aggregate_1d_empty():
    n_bins = 1
    d_in = dt.Frame([])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, n_bins=n_bins,
                                         progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (0, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (0, 1)
    assert d_exemplars.ltypes == (ltype.int,)
    assert d_exemplars.to_list() == [[]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_1d_continuous_integer_tiny():
    n_bins = 1
    d_in = dt.Frame([5])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, n_bins=n_bins,
                                         progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (1, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[0]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (1, 2)
    assert d_exemplars.ltypes == (ltype.int, ltype.int)
    assert d_exemplars.to_list() == [[5], [1]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_1d_continuous_integer_equal():
    n_bins = 2
    d_in = dt.Frame([0, 0, None, 0, None, 0, 0, 0, 0, 0])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, n_bins=n_bins,
                                         progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[1, 1, 0, 1, 0, 1, 1, 1, 1, 1]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (2, 2)
    assert d_exemplars.ltypes == (ltype.bool, ltype.int)
    assert d_exemplars.to_list() == [[None, 0], [2, 8]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_1d_continuous_integer_sorted():
    n_bins = 3
    d_in = dt.Frame([0, 1, None, 2, 3, 4, 5, 6, 7, None, 8, 9])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, n_bins=n_bins,
                                         progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (12, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[1, 1, 0, 1, 1, 2, 2, 2, 3, 0, 3, 3]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (4, 2)
    assert d_exemplars.ltypes == (ltype.int, ltype.int)
    assert d_exemplars.to_list() == [[None, 0, 4, 7],
                                    [2, 4, 3, 3]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_1d_continuous_integer_random():
    n_bins = 3
    d_in = dt.Frame([None, 9, 8, None, 2, 3, 3, 0, 5, 5, 8, 1, None])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, n_bins=n_bins,
                                         progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (13, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[0, 3, 3, 0, 1, 1, 1, 1, 2, 2, 3, 1, 0]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (4, 2)
    assert d_exemplars.ltypes == (ltype.int, ltype.int)
    assert d_exemplars.to_list() == [[None, 2, 5, 9],
                                    [3, 5, 2, 3]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_1d_continuous_real_sorted():
    n_bins = 3
    d_in = dt.Frame([0.0, None, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, n_bins=n_bins,
                                         progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (11, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[1, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (4, 2)
    assert d_exemplars.ltypes == (ltype.real, ltype.int)
    assert d_exemplars.to_list() == [[None, 0.0, 0.4, 0.7],
                                    [1, 4, 3, 3]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_1d_continuous_real_random():
    n_bins = 3
    d_in = dt.Frame([0.7, 0.7, 0.5, 0.1, 0.0, 0.9, 0.1, 0.3, 0.4, 0.2,
                     None, None, None])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, n_bins=n_bins,
                                         progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (13, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[3, 3, 2, 1, 1, 3, 1, 1, 2, 1, 0, 0, 0]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (4, 2)
    assert d_exemplars.ltypes == (ltype.real, ltype.int)
    assert d_exemplars.to_list() == [[None, 0.1, 0.5, 0.7],
                                    [3, 5, 2, 3]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_1d_categorical_sorted():
    d_in = dt.Frame([None, "blue", "green", "indigo", "orange", "red", "violet",
                     "yellow"])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (8, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[0, 1, 2, 3, 4, 5, 6, 7]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (8, 2)
    assert d_exemplars.ltypes == (ltype.str, ltype.int)
    assert d_exemplars.to_list() == [[None, "blue", "green", "indigo", "orange",
                                      "red", "violet", "yellow"],
                                     [1, 1, 1, 1, 1, 1, 1, 1]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_1d_categorical_unsorted():
    d_in = dt.Frame(["blue", "orange", "yellow", None, "green", "blue",
                     "indigo", None, "violet"])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (9, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[1, 4, 6, 0, 2, 1, 3, 0, 5]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (7, 2)
    assert d_exemplars.ltypes == (ltype.str, ltype.int)
    assert d_exemplars.to_list() == [[None, "blue", "green", "indigo", "orange",
                                      "violet", "yellow"],
                                     [2, 2, 1, 1, 1, 1, 1]]
    assert_equals(d_in, d_in_copy)


# Disable some of the checks for the moment, as even with the same seed
# random generator may behave differently on different platforms.
def test_aggregate_1d_categorical_sampling():
    d_in = dt.Frame(["blue", "orange", "yellow", None, "green", "blue",
                     "indigo", None, "violet"])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, n_bins=3, min_rows=0,
                                         progress_fn=report_progress, seed=1)
    d_members.internal.check()
    assert d_members.shape == (9, 1)
    assert d_members.ltypes == (ltype.int,)
    # assert d_members.to_list() == [[3, None, 2, 0, 1, 3, None, 0, None]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (3, 2)
    assert d_exemplars.ltypes == (ltype.str, ltype.int)
    # assert d_exemplars.to_list() == [[None, 'green', 'yellow', 'blue'],
    #                                  [2, 1, 1, 2]]
    assert_equals(d_in, d_in_copy)


#-------------------------------------------------------------------------------
# Aggregate 2D
#-------------------------------------------------------------------------------

def test_aggregate_2d_continuous_integer_sorted():
    nx_bins = 3
    ny_bins = 3
    d_in = dt.Frame([[None, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
                     [0, None, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9]])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, nx_bins=nx_bins, ny_bins=ny_bins,
                                         progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (12, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[1, 0, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (5, 3)
    assert d_exemplars.ltypes == (ltype.int, ltype.int, ltype.int)
    assert d_exemplars.to_list() == [[0, None, 0, 4, 7],
                                     [None, 0, 0, 4, 7],
                                     [1, 1, 4, 3, 3]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_2d_continuous_integer_random():
    nx_bins = 3
    ny_bins = 3
    d_in = dt.Frame([[9, None, 8, 2, 3, 3, 0, 5, 5, 8, 1],
                     [3, None, 5, 8, 1, 4, 4, 8, 7, 6, 1]])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, nx_bins=nx_bins, ny_bins=ny_bins,
                                         progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (11, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[2, 0, 4, 5, 1, 3, 3, 6, 6, 7, 1]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (8, 3)
    assert d_exemplars.ltypes == (ltype.int, ltype.int, ltype.int)
    assert d_exemplars.to_list() == [[None, 3, 9, 3, 8, 2, 5, 8],
                                     [None, 1, 3, 4, 5, 8, 8, 6],
                                     [1, 2, 1, 2, 1, 1, 2, 1]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_2d_continuous_real_sorted():
    nx_bins = 3
    ny_bins = 3
    d_in = dt.Frame([[0.0, 0.0, 0.1, 0.2, None, 0.3, 0.4, 0.5, 0.6,
                      0.7, 0.8, 0.9, None],
                     [None, 0.0, 0.1, 0.2, None, 0.3, 0.4, 0.5, 0.6,
                      0.7, 0.8, 0.9, 0.0]])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, nx_bins=nx_bins, ny_bins=ny_bins,
                                         progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (13, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[1, 3, 3, 3, 0, 3, 4, 4, 4, 5, 5, 5, 2]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (6, 3)
    assert d_exemplars.ltypes == (ltype.real, ltype.real, ltype.int)
    assert d_exemplars.to_list() == [[None, 0.0, None, 0.0, 0.4, 0.7],
                                     [None, None, 0.0, 0.0, 0.4, 0.7],
                                     [1, 1, 1, 4, 3, 3]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_2d_continuous_real_random():
    nx_bins = 3
    ny_bins = 3
    d_in = dt.Frame([
        [None, 0.9, 0.8, 0.2, 0.3, None, 0.3, 0.0, 0.5, 0.5, 0.8, 0.1],
        [None, 0.3, 0.5, 0.8, 0.1, None, 0.4, 0.4, 0.8, 0.7, 0.6, 0.1]
    ])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, nx_bins=nx_bins, ny_bins=ny_bins,
                                         progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (12, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[0, 2, 4, 5, 1, 0, 3, 3, 6, 6, 7, 1]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (8, 3)
    assert d_exemplars.ltypes == (ltype.real, ltype.real, ltype.int)
    assert d_exemplars.to_list() == [[None, 0.3, 0.9, 0.3, 0.8, 0.2, 0.5, 0.8],
                                     [None, 0.1, 0.3, 0.4, 0.5, 0.8, 0.8, 0.6],
                                     [2, 2, 1, 2, 1, 1, 2, 1]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_2d_categorical_sorted():
    d_in = dt.Frame([[None, None, "abc", "blue", "green", "indigo", "orange",
                      "red", "violet", "yellow"],
                     [None, "abc", None, "Friday", "Monday", "Saturday",
                      "Sunday", "Thursday", "Tuesday", "Wednesday"]])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[0, 2, 1, 3, 4, 5, 6, 7, 8, 9]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (10, 3)
    assert d_exemplars.ltypes == (ltype.str, ltype.str, ltype.int)
    assert d_exemplars.to_list() == [[None, "abc", None, "blue", "green", "indigo",
                                      "orange", "red", "violet", "yellow"],
                                     [None, None, "abc", "Friday", "Monday",
                                      "Saturday", "Sunday", "Thursday", "Tuesday",
                                      "Wednesday"],
                                     [1, 1, 1, 1, 1, 1, 1, 1, 1, 1]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_2d_categorical_unsorted():
    d_in = dt.Frame([["blue", "indigo", "red", "violet", "yellow", "violet", "red"],
                     ["Monday", "Monday", "Wednesday", "Saturday", "Thursday", "Friday", "Wednesday"]])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (7, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[0, 1, 2, 4, 5, 3, 2]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (6, 3)
    assert d_exemplars.ltypes == (ltype.str, ltype.str, ltype.int)
    assert d_exemplars.to_list() == [['blue', 'indigo', 'red', 'violet', 'violet',
                                      'yellow'],
                                     ['Monday', 'Monday', 'Wednesday', 'Friday',
                                      'Saturday', 'Thursday'],
                                     [1, 1, 2, 1, 1, 1]]
    assert_equals(d_in, d_in_copy)


# Disable some of the checks for the moment, as even with the same seed
# random generator may behave differently on different platforms.
def test_aggregate_2d_categorical_sampling():
    d_in = dt.Frame([["blue", None, "indigo", "red", "violet", "yellow",
                      "violet", "green", None, None],
                     ["Monday", "abc", "Monday", "Wednesday", "Saturday",
                      "Thursday", "Friday", "Wednesday", "def", "ghi"]])
    d_in_copy = dt.Frame(d_in)

    [d_exemplars, d_members] = aggregate(d_in, nx_bins=2, ny_bins=2, min_rows=0,
                          progress_fn=report_progress, seed=1)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    # assert d_members.to_list() == [[...]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (4, 3)
    assert d_exemplars.ltypes == (ltype.str, ltype.str, ltype.int)
    # assert d_exemplars.to_list() == [[...],
    #                                  [...],
    #                                  [...]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_2d_mixed_sorted():
    nx_bins = 7
    d_in = dt.Frame([[None, None, 0, 0, 1, 2, 3, 4, 5, 6],
                     [None, "a", None, "blue", "green", "indigo", "orange",
                      "red", "violet", "yellow"]])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, nx_bins=nx_bins,
                          progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[0, 2, 1, 3, 4, 5, 6, 7, 8, 9]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (10, 3)
    assert d_exemplars.ltypes == (ltype.int, ltype.str, ltype.int)
    assert d_exemplars.to_list() == [[None, 0, None, 0, 1, 2, 3, 4, 5, 6],
                                     [None, None, "a", "blue", "green", "indigo",
                                      "orange", "red", "violet", "yellow"],
                                     [1, 1, 1, 1, 1, 1, 1, 1, 1, 1]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_2d_mixed_random():
    nx_bins = 6
    d_in = dt.Frame([[1, 3, 0, None, None, 6, 6, None, 1, 2, 4],
                     [None, "blue", "indigo", "abc", "def", "red", "violet",
                      "ghi", "yellow", "violet", "red"]])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, nx_bins=nx_bins,
                          progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (11, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[0, 2, 3, 1, 1, 5, 7, 1, 8, 6, 4]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (9, 3)
    assert d_exemplars.ltypes == (ltype.int, ltype.str, ltype.int)
    assert d_exemplars.to_list() == [[1, None, 3, 0, 4, 6, 2, 6, 1],
                                     [None, 'abc', 'blue', 'indigo', 'red', 'red',
                                      'violet', 'violet', 'yellow'],
                                     [1, 3, 1, 1, 1, 1, 1, 1, 1]]
    assert_equals(d_in, d_in_copy)


#-------------------------------------------------------------------------------
# Aggregate ND, take into account ND is parallelized with OpenMP
#-------------------------------------------------------------------------------

def test_aggregate_3d_categorical():
    min_rows = 500
    rows = min_rows * 2
    nd_max_bins_in = rows
    a_in = [["blue"] * rows, ["orange"] * rows, ["yellow"] * rows]
    members_count = [[1] * rows]
    exemplar_id = list(range(rows))
    d_in = dt.Frame(a_in)
    d_in_copy = dt.Frame(d_in)

    [d_exemplars, d_members] = aggregate(d_in, nd_max_bins=nd_max_bins_in,
                                         progress_fn=report_progress)
    assert d_members.shape == (rows, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [exemplar_id]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (rows, 4)
    assert d_exemplars.ltypes == (ltype.str, ltype.str, ltype.str, ltype.int)
    assert d_exemplars.to_list() == a_in + members_count
    assert_equals(d_in, d_in_copy)


def test_aggregate_3d_real():
    d_in = dt.Frame([
        [0.95, 0.50, 0.55, 0.10, 0.90, 0.50, 0.90, 0.50, 0.90, 1.00],
        [1.00, 0.55, 0.45, 0.05, 0.95, 0.45, 0.90, 0.40, 1.00, 0.90],
        [0.90, 0.50, 0.55, 0.00, 1.00, 0.50, 0.95, 0.45, 0.95, 0.95]
    ])
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, nd_max_bins=3,
                                         progress_fn=report_progress)
    a_members = d_members.to_list()[0]
    d = d_exemplars.sort("C0")
    ri = frame_column_rowindex(d, 0).to_list()
    for i, member in enumerate(a_members):
        a_members[i] = ri.index(member)

    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert a_members == [2, 1, 1, 0, 2, 1, 2, 1, 2, 2]

    d_exemplars.internal.check()
    assert d_exemplars.shape == (3, 4)
    assert d_exemplars.ltypes == (ltype.real, ltype.real, ltype.real, ltype.int)
    assert d.to_list() == [[0.10, 0.50, 0.95],
                           [0.05, 0.55, 1.00],
                           [0.00, 0.50, 0.90],
                           [1, 4, 5]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_nd_direct():
    max_dimensions = 50
    aggregate_nd(max_dimensions // 2)


def test_aggregate_nd_projection():
    max_dimensions = 50
    aggregate_nd(max_dimensions * 2)


def aggregate_nd(nd):
    nrows = 1000
    div = 50
    column = [i % div for i in range(nrows)]
    matrix = [column] * nd
    out_types = [ltype.int] * nd + [ltype.int]
    out_value = [list(range(div))] * nd + \
                [[nrows // div] * div]

    d_in = dt.Frame(matrix)
    d_in_copy = dt.Frame(d_in)
    [d_exemplars, d_members] = aggregate(d_in, min_rows=0, nd_max_bins=div, seed=1,
                                         progress_fn=report_progress)

    a_members = d_members.to_list()[0]
    d = d_exemplars.sort("C0")
    ri = frame_column_rowindex(d, 0).to_list()
    for i, member in enumerate(a_members):
        a_members[i] = ri.index(member)

    d_members.internal.check()
    assert d_members.shape == (nrows, 1)
    assert d_members.ltypes == (ltype.int,)
    assert a_members == column
    d_exemplars.internal.check()
    assert d_exemplars.shape == (div, nd + 1)
    assert d_exemplars.ltypes == tuple(out_types)
    assert d.to_list() == out_value
    assert_equals(d_in, d_in_copy)


#-------------------------------------------------------------------------------
# Aggregate views
#-------------------------------------------------------------------------------

def test_aggregate_view_0d_continuous_integer():
    d_in = dt.Frame([0, 1, None, 2, None, 3, 3, 4, 4, None, 5])
    d_in_copy = dt.Frame(d_in)
    d_in_view = d_in[5:11, :]
    [d_exemplars, d_members] = aggregate(d_in_view, min_rows=100, n_bins=10,
                                         progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (6, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[1, 2, 3, 4, 0, 5]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (6, 2)
    assert d_exemplars.ltypes == (ltype.int, ltype.int)
    assert d_exemplars.to_list() == [[None, 3, 3, 4, 4, 5], [1, 1, 1, 1, 1, 1]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_view_1d_categorical():
    d_in = dt.Frame(["alpha", "bravo", "delta", None, "charlie", "charlie", "echo", None])
    d_in_copy = dt.Frame(d_in)
    d_in_view = d_in[2:6, :]
    [d_exemplars, d_members] = aggregate(d_in_view, min_rows=0, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (4, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[2, 0, 1, 1]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (3, 2)
    assert d_exemplars.ltypes == (ltype.str, ltype.int)
    assert d_exemplars.to_list() == [[None, "charlie", "delta"],
                                     [1, 2, 1]]
    assert_equals(d_in, d_in_copy)


def test_aggregate_view_2d_categorical():
    d_in = dt.Frame([["alpha", None, "bravo", "charlie", "charlie", "bravo", "echo"],
                     ["red", "green", "blue", None, "red", "blue", "orange"]])
    d_in_copy = dt.Frame(d_in)
    d_in_view = d_in[2:6, :]
    [d_exemplars, d_members] = aggregate(d_in_view, min_rows=0, progress_fn=report_progress)
    d_members.internal.check()
    assert d_members.shape == (4, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.to_list() == [[1, 0, 2, 1]]
    d_exemplars.internal.check()
    assert d_exemplars.shape == (3, 3)
    assert d_exemplars.ltypes == (ltype.str, ltype.str, ltype.int)
    assert d_exemplars.to_list() == [["charlie", "bravo", "charlie"],
                                     [None, "blue", "red"],
                                     [1, 2, 1]]
    assert_equals(d_in, d_in_copy)
