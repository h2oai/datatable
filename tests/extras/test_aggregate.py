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

def test_aggregate_1d_continuous_integer_equal():
    n_bins = 3
    d_in = dt.Frame([0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
    d_members = aggregate(d_in, n_bins)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0, 0, 0, 0, 0, 0, 0, 0, 0, 0]]
    d_in.internal.check()
    assert d_in.shape == (1, 2)
    assert d_in.ltypes == (ltype.bool, ltype.int)
    assert d_in.topython() == [[0],
                               [10]]

def test_aggregate_1d_continuous_integer_sorted():
    n_bins = 3
    d_in = dt.Frame([0, 1, 2, 3, 4, 5, 6, 7, 8, 9])
    d_members = aggregate(d_in, n_bins)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0, 0, 0, 0, 1, 1, 1, 2, 2, 2]]
    d_in.internal.check()
    assert d_in.shape == (3, 2)
    assert d_in.ltypes == (ltype.int, ltype.int)
    assert d_in.topython() == [[0, 4, 7],
                               [4, 3, 3]]


def test_aggregate_1d_continuous_integer_random():
    n_bins = 3
    d_in = dt.Frame([9, 8, 2, 3, 3, 0, 5, 5, 8, 1])
    d_members = aggregate(d_in, n_bins)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[2, 2, 0, 0, 0, 0, 1, 1, 2, 0]]
    d_in.internal.check()
    assert d_in.shape == (3, 2)
    assert d_in.ltypes == (ltype.int, ltype.int)
    assert d_in.topython() == [[2, 5, 9],
                               [5, 2, 3]]


def test_aggregate_1d_continuous_real_sorted():
    n_bins = 3
    d_in = dt.Frame([0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9])
    d_members = aggregate(d_in, n_bins)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0, 0, 0, 0, 1, 1, 1, 2, 2, 2]]
    d_in.internal.check()
    assert d_in.shape == (3, 2)
    assert d_in.ltypes == (ltype.real, ltype.int)
    assert d_in.topython() == [[0.0, 0.4, 0.7],
                               [4, 3, 3]]


def test_aggregate_1d_continuous_real_random():
    n_bins = 3
    d_in = dt.Frame([0.7, 0.7, 0.5, 0.1, 0.0, 0.9, 0.1, 0.3, 0.4, 0.2])
    d_members = aggregate(d_in, n_bins)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[2, 2, 1, 0, 0, 2, 0, 0, 1, 0]]
    d_in.internal.check()
    assert d_in.shape == (3, 2)
    assert d_in.ltypes == (ltype.real, ltype.int)
    assert d_in.topython() == [[0.1, 0.5, 0.7],
                               [5, 2, 3]]


#-------------------------------------------------------------------------------
# Aggregate 2D
#-------------------------------------------------------------------------------

def test_aggregate_2d_continuous_integer_sorted():
    nx_bins = 3
    ny_bins = 3
    d_in = dt.Frame([[0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
                   [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]])
    d_members = aggregate(d_in, 0, nx_bins, ny_bins)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0, 0, 0, 0, 1, 1, 1, 2, 2, 2]]
    d_in.internal.check()
    assert d_in.shape == (3, 3)
    assert d_in.ltypes == (ltype.int, ltype.int, ltype.int)
    assert d_in.topython() == [[0, 4, 7],
                               [0, 4, 7],
                               [4, 3, 3]]


def test_aggregate_2d_continuous_integer_random():
    nx_bins = 3
    ny_bins = 3
    d_in = dt.Frame([[9, 8, 2, 3, 3, 0, 5, 5, 8, 1],
                   [3, 5, 8, 1, 4, 4, 8, 7, 6, 1]])
    d_members = aggregate(d_in, 0, nx_bins, ny_bins)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[1, 3, 4, 0, 2, 2, 5, 5, 6, 0]]
    d_in.internal.check()
    assert d_in.shape == (7, 3)
    assert d_in.ltypes == (ltype.int, ltype.int, ltype.int)
    assert d_in.topython() == [[3, 9, 3, 8, 2, 5, 8],
                               [1, 3, 4, 5, 8, 8, 6],
                               [2, 1, 2, 1, 1, 2, 1]]


def test_aggregate_2d_continuous_real_sorted():
    nx_bins = 3
    ny_bins = 3
    d_in = dt.Frame([[0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9],
                   [0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9]])
    d_members = aggregate(d_in, 0, nx_bins, ny_bins)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0, 0, 0, 0, 1, 1, 1, 2, 2, 2]]
    d_in.internal.check()
    assert d_in.shape == (3, 3)
    assert d_in.ltypes == (ltype.real, ltype.real, ltype.int)
    assert d_in.topython() == [[0.0, 0.4, 0.7],
                               [0.0, 0.4, 0.7],
                               [4, 3, 3]]


def test_aggregate_2d_continuous_real_random():
    nx_bins = 3
    ny_bins = 3
    d_in = dt.Frame([[0.9, 0.8, 0.2, 0.3, 0.3, 0.0, 0.5, 0.5, 0.8, 0.1],
                   [0.3, 0.5, 0.8, 0.1, 0.4, 0.4, 0.8, 0.7, 0.6, 0.1]])
    d_members = aggregate(d_in, 0, nx_bins, ny_bins)
    d_members.internal.check()
    assert d_members.shape == (10, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[1, 3, 4, 0, 2, 2, 5, 5, 6, 0]]
    d_in.internal.check()
    assert d_in.shape == (7, 3)
    assert d_in.ltypes == (ltype.real, ltype.real, ltype.int)
    assert d_in.topython() == [[0.3, 0.9, 0.3, 0.8, 0.2, 0.5, 0.8],
                               [0.1, 0.3, 0.4, 0.5, 0.8, 0.8, 0.6],
                               [2, 1, 2, 1, 1, 2, 1]]


def test_aggregate_1d_categorical_sorted():
    d_in = dt.Frame(["blue", "green", "indigo", "orange", "red", "violet", "yellow"])
    d_members = aggregate(d_in)
    assert d_members.shape == (7, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0, 1, 2, 3, 4, 5, 6]]
    d_in.internal.check()
    assert d_in.shape == (7, 2)
    assert d_in.ltypes == (ltype.str, ltype.int)
    assert d_in.topython() == [["blue", "green", "indigo", "orange", "red", "violet", "yellow"],
                               [1, 1, 1, 1, 1, 1, 1]]


def test_aggregate_1d_categorical_random():
    d_in = dt.Frame(["blue", "orange", "yellow", "green", "blue", "indigo", "violet"])
    d_members = aggregate(d_in)
    assert d_members.shape == (7, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0, 3, 5, 1, 0, 2, 4]]
    d_in.internal.check()
    assert d_in.shape == (6, 2)
    assert d_in.ltypes == (ltype.str, ltype.int)
    assert d_in.topython() == [["blue", "green", "indigo", "orange", "violet", "yellow"],
                               [2, 1, 1, 1, 1, 1]]


def test_aggregate_2d_categorical_sorted():
    d_in = dt.Frame([["blue", "green", "indigo", "orange", "red", "violet", "yellow"],
                     ["Friday", "Monday", "Saturday", "Sunday", "Thursday", "Tuesday", "Wednesday"]])
    d_members = aggregate(d_in)
    d_members.internal.check()
    assert d_members.shape == (7, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0, 1, 2, 3, 4, 5, 6]]
    d_in.internal.check()
    assert d_in.shape == (7, 3)
    assert d_in.ltypes == (ltype.str, ltype.str, ltype.int)
    assert d_in.topython() == [["blue", "green", "indigo", "orange", "red", "violet", "yellow"],
                               ["Friday", "Monday", "Saturday", "Sunday", "Thursday", "Tuesday", "Wednesday"],
                               [1, 1, 1, 1, 1, 1, 1]]


def test_aggregate_2d_categorical_random():
    d_in = dt.Frame([["blue", "indigo", "red", "violet", "yellow", "violet", "red"],
                     ["Monday", "Monday", "Wednesday", "Saturday", "Thursday", "Friday", "Wednesday"]])

    d_members = aggregate(d_in)
    d_members.internal.check()
    assert d_members.shape == (7, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[1, 2, 5, 3, 4, 0, 5]]
    d_in.internal.check()
    assert d_in.shape == (6, 3)
    assert d_in.ltypes == (ltype.str, ltype.str, ltype.int)
    assert d_in.topython() == [['violet', 'blue', 'indigo', 'violet', 'yellow', 'red'],
                               ['Friday', 'Monday', 'Monday', 'Saturday', 'Thursday', 'Wednesday'],
                               [1, 1, 1, 1, 1, 2]]


def test_aggregate_2d_mixed_sorted():
    nx_bins = 7
    d_in = dt.Frame([[0, 1, 2, 3, 4, 5, 6],
                     ["blue", "green", "indigo", "orange", "red", "violet", "yellow"]])
    d_members = aggregate(d_in, 0, nx_bins)
    d_members.internal.check()
    assert d_members.shape == (7, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0, 1, 2, 3, 4, 5, 6]]
    d_in.internal.check()
    assert d_in.shape == (7, 3)
    assert d_in.ltypes == (ltype.int, ltype.str, ltype.int)
    assert d_in.topython() == [[0, 1, 2, 3, 4, 5, 6],
                               ["blue", "green", "indigo", "orange", "red", "violet", "yellow"],
                               [1, 1, 1, 1, 1, 1, 1]]


def test_aggregate_2d_mixed_random():
    nx_bins = 6
    d_in = dt.Frame([[3, 0, 6, 6, 1, 2, 4],
                     ["blue", "indigo", "red", "violet", "yellow", "violet", "red"]])
    d_members = aggregate(d_in, 0, nx_bins)
    d_members.internal.check()
    assert d_members.shape == (7, 1)
    assert d_members.ltypes == (ltype.int,)
    assert d_members.topython() == [[0, 1, 3, 5, 6, 4, 2]]
    d_in.internal.check()
    assert d_in.shape == (7, 3)
    assert d_in.ltypes == (ltype.int, ltype.str, ltype.int)
    assert d_in.topython() == [[3, 0, 4, 6, 2, 6, 1],
                               ['blue', 'indigo', 'red', 'red', 'violet', 'violet', 'yellow'],
                               [1, 1, 1, 1, 1, 1, 1]]

