import pytest
from datatable import dt, f, by
from tests import assert_equals

df = dt.Frame({'A': [1, 1, 1, 2, 2, 2, 3, 3, 3], 'B': [3, 2, 20, 1, 6, 2, 3, 22, 1]}, stype='int32')


def test_single_column():
    expect_min = dt.Frame({'B': [1]}, stype='int32')
    expect_max = dt.Frame({'B': [22]}, stype='int32')

    assert_equals(df[:, dt.min(f.B)], expect_min)
    assert_equals(df[:, dt.max(f.B)], expect_max)


def test_list_of_minmax():
    expect_min = dt.Frame({'A': [1], 'B': [1]}, stype='int32')
    expect_max = dt.Frame({'A': [3], 'B': [22]}, stype='int32')

    assert_equals(df[:, [dt.min(f.A), dt.min(f.B)]], expect_min)
    assert_equals(df[:, [dt.max(f.A), dt.max(f.B)]], expect_max)


def test_slice_of_columns():
    expect_min = dt.Frame({'A': [1], 'B': [1]}, stype='int32')
    expect_max = dt.Frame({'A': [3], 'B': [22]}, stype='int32')

    assert_equals(df[:, dt.min(f[:])], expect_min)
    assert_equals(df[:, dt.max(f[:])], expect_max)


def test_minmax_with_column_list():
    expect_min = dt.Frame({'A': [1], 'B': [1]}, stype='int32')
    expect_max = dt.Frame({'A': [3], 'B': [22]}, stype='int32')

    assert_equals(df[:, dt.min([f.A, f.B])], expect_min)
    assert_equals(df[:, dt.max([f.A, f.B])], expect_max)


def test_minmax_with_column_dict():
    expect_min = dt.Frame({'A_min': [1], 'B_min': [1]}, stype='int32')
    expect_max = dt.Frame({'A_max': [3], 'B_max': [22]}, stype='int32')

    assert_equals(df[:, dt.min({"A_min": f.A, "B_min": f.B})], expect_min)
    assert_equals(df[:, dt.max({"A_max": f.A, "B_max": f.B})], expect_max)


def test_minmax_with_by():
    expect_min = dt.Frame({'A': [1, 2, 3], 'B': [2, 1, 1]}, stype='int32')
    expect_max = dt.Frame({'A': [1, 2, 3], 'B': [20, 6, 22]}, stype='int32')

    assert_equals(df[:, dt.min(f.B), by("A")], expect_min)
    assert_equals(df[:, dt.max(f.B), by("A")], expect_max)