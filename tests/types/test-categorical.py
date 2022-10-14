#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2022 H2O.ai
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
import pytest
import random
from datatable import dt, f
from tests import assert_equals


#-------------------------------------------------------------------------------
# Type object
#-------------------------------------------------------------------------------

@pytest.mark.parametrize('t', [dt.Type.cat8,
                               dt.Type.cat16,
                               dt.Type.cat32])
def test_type_categorical_wrong(t):
    msg = r"Categories are not allowed to be of a categorical type"
    with pytest.raises(TypeError, match=msg):
        t(t(dt.str64))


def test_type_categorical_repr():
    assert repr(dt.Type.cat8(int)) == "Type.cat8(int64)"
    assert repr(dt.Type.cat16(dt.float32)) == "Type.cat16(float32)"
    assert repr(dt.Type.cat32(str)) == "Type.cat32(str32)"


def test_type_categorical_name():
    assert dt.Type.cat8(int).name == "cat8(int64)"
    assert dt.Type.cat16(dt.int8).name == "cat16(int8)"
    assert dt.Type.cat32(None).name == "cat32(void)"


@pytest.mark.parametrize('t', [dt.Type.cat8(int),
                               dt.Type.cat16(str),
                               dt.Type.cat32(float)])
def test_type_categorical_properties(t):
    assert t.min is None
    assert t.max is None


def test_type_categorical_equality():
    t1 = dt.Type.cat8(int)
    t2 = dt.Type.cat32(int)
    assert t1 != t2
    assert t1 == dt.Type.cat8(dt.int64)
    assert t1 != dt.Type.cat8(dt.int32)
    assert t1 != dt.Type.cat8(dt.Type.void)
    assert t1 != dt.Type.int64
    assert t2 == dt.Type.cat32(int)
    assert t2 != dt.Type.cat8(int)
    assert t2 != dt.Type.cat32(str)
    assert dt.Type.cat8(int) != \
           dt.Type.cat8(float)


def test_type_categorical_hashable():
    store = {dt.Type.cat8(str): 1, dt.Type.cat32('float32'): 2,
             dt.Type.cat16(str): 3}
    assert dt.Type.cat8(str) in store
    assert dt.Type.cat32(dt.float32) in store
    assert dt.Type.cat16(str) in store
    assert store[dt.Type.cat8("str")] == 1
    assert store[dt.Type.cat32('float32')] == 2
    assert store[dt.Type.cat16(str)] == 3
    assert dt.Type.cat8(int) not in store
    assert dt.Type.cat32(int) not in store
    assert dt.Type.cat32(float) not in store


@pytest.mark.parametrize('t', [dt.Type.cat8(int),
                               dt.Type.cat16(str),
                               dt.Type.cat32(float)])
def test_type_categorical_query_methods(t):
    assert     t.is_categorical
    assert not t.is_boolean
    assert     t.is_compound
    assert not t.is_float
    assert not t.is_integer
    assert not t.is_numeric
    assert not t.is_object
    assert not t.is_string
    assert not t.is_temporal
    assert not t.is_void


#-------------------------------------------------------------------------------
# Create from various types
#-------------------------------------------------------------------------------

@pytest.mark.parametrize('t', [dt.Type.cat8,
                               dt.Type.cat16,
                               dt.Type.cat32])
def test_create_too_many_cats_error(t):
    src = list(range(1000))

    if t is dt.Type.cat8:
        msg = "Number of categories in the column is 1000, that is larger " \
              r"than cat8\(int32\) type supports, i.e. 256"
        with pytest.raises(ValueError, match=msg):
            DT = dt.Frame(src, types = [t(dt.Type.int32)])
    else:
        DT1 = dt.Frame(src)
        DT2 = dt.Frame(src, types = [t(dt.Type.int32)])
        assert DT1.shape == DT2.shape
        assert DT1.names == DT2.names
        assert DT1.to_list() == DT2.to_list()


@pytest.mark.parametrize('t', [dt.Type.cat8,
                               dt.Type.cat16,
                               dt.Type.cat32])
def test_create_from_zero_rows(t):
    src = [[]]
    DT1 = dt.Frame(src)
    DT2 = dt.Frame(src, types = [t(dt.Type.bool8)])
    assert DT2.type == t(dt.Type.bool8)
    assert DT1.shape == DT2.shape
    assert DT1.names == DT2.names
    assert DT1.to_list() == DT2.to_list()
    assert_equals(DT2, DT2[:, :])


@pytest.mark.parametrize('t', [dt.Type.cat8,
                               dt.Type.cat16,
                               dt.Type.cat32])
def test_create_from_void(t):
    src = [None] * 10
    DT1 = dt.Frame(src)
    DT2 = dt.Frame(src, types = [t(dt.Type.bool8)])
    assert DT2.type == t(dt.Type.bool8)
    assert DT1.shape == DT2.shape
    assert DT1.names == DT2.names
    assert DT1.to_list() == DT2.to_list()
    assert_equals(DT2, DT2[:, :])


@pytest.mark.parametrize('t', [dt.Type.cat8,
                               dt.Type.cat16,
                               dt.Type.cat32])
def test_create_from_bools(t):
    src = [None, True, False, None, False, False]
    DT1 = dt.Frame(src)
    DT2 = dt.Frame(src, types = [t(dt.Type.bool8)])
    assert DT2.type == t(dt.Type.bool8)
    assert DT1.shape == DT2.shape
    assert DT1.names == DT2.names
    assert DT1.to_list() == DT2.to_list()
    assert_equals(DT2, DT2[:, :])


@pytest.mark.parametrize('t', [dt.Type.cat8,
                               dt.Type.cat16,
                               dt.Type.cat32])
def test_create_from_ints(t):
    src = [3, None, 1, 4, 1, None, 5, 9, 2, 6]
    DT1 = dt.Frame(src)
    DT2 = dt.Frame(src, types = [t(dt.Type.int32)])
    assert DT2.type == t(dt.Type.int32)
    assert DT1.shape == DT2.shape
    assert DT1.names == DT2.names
    assert DT1.to_list() == DT2.to_list()
    assert_equals(DT2, DT2[:, :])


def test_create_from_ints_large():
    src = list(range(10**6))
    DT1 = dt.Frame(src)
    DT2 = dt.Frame(src, types = [dt.Type.cat32(dt.Type.int32)])
    assert DT1.shape == DT2.shape
    assert DT1.names == DT2.names
    assert DT1.to_list() == DT2.to_list()
    assert_equals(DT2, DT2[:, :])


@pytest.mark.parametrize('t', [dt.Type.cat8,
                               dt.Type.cat16,
                               dt.Type.cat32])
def test_create_from_floats(t):
    src = [3.14, None, 1.15, 4.92, 1.6, None, 5.5, 9, 2.35, 6.0]
    DT1 = dt.Frame(src)
    DT2 = dt.Frame(src, types = [t(dt.Type.float64)])
    assert DT2.type == t(dt.Type.float64)
    assert DT1.shape == DT2.shape
    assert DT1.names == DT2.names
    assert DT1.to_list() == DT2.to_list()
    assert_equals(DT2, DT2[:, :])


@pytest.mark.parametrize('t', [dt.Type.cat8,
                               dt.Type.cat16,
                               dt.Type.cat32])
def test_create_from_strings(t):
    src = ["dog", "mouse", None, "dog", "cat", None, "1", "pig"]
    DT1 = dt.Frame(src)
    DT2 = dt.Frame(src, types = [t(dt.Type.str32)])
    assert DT2.type == t(dt.Type.str32)
    assert DT1.shape == DT2.shape
    assert DT1.names == DT2.names
    assert DT1.to_list() == DT2.to_list()
    assert_equals(DT2, DT2[:, :])


@pytest.mark.parametrize('t', [dt.Type.cat8,
                               dt.Type.cat16,
                               dt.Type.cat32])
def test_create_from_dates(t):
    from datetime import date as d
    src = [d(1997, 9, 1), d(2002, 7, 31), d(2000, 2, 20), None]
    DT1 = dt.Frame(src)
    DT2 = dt.Frame(src, types = [t(dt.Type.date32)])
    assert DT2.type == t(dt.Type.date32)
    assert DT1.shape == DT2.shape
    assert DT1.names == DT2.names
    assert DT1.to_list() == DT2.to_list()
    assert_equals(DT2, DT2[:, :])


@pytest.mark.parametrize('t', [dt.Type.cat8,
                               dt.Type.cat16,
                               dt.Type.cat32])
def test_create_from_times(t):
    from datetime import datetime as d
    src = [d(2000, 10, 18, 3, 30),
           d(2010, 11, 13, 15, 11, 59),
           d(2020, 2, 29, 20, 20, 20, 20), None]
    DT1 = dt.Frame(src)
    DT2 = dt.Frame(src, types = [t(dt.Type.time64)])
    assert DT2.type == t(dt.Type.time64)
    assert DT1.shape == DT2.shape
    assert DT1.names == DT2.names
    assert DT1.to_list() == DT2.to_list()
    assert_equals(DT2, DT2[:, :])


@pytest.mark.parametrize('t', [dt.Type.cat8,
                               dt.Type.cat16,
                               dt.Type.cat32])
def test_create_multicolumn(t):
    src = [[6, 8, 8, None, 20],
           ["six", "eight", "eight", None, "twenty"]]
    DT1 = dt.Frame(src)
    DT2 = dt.Frame(src, types = [t(dt.Type.int32), t(dt.Type.str32)])
    assert DT2[:, 0].type == t(dt.Type.int32)
    assert DT2[:, 1].type == t(dt.Type.str32)
    assert DT1.shape == DT2.shape
    assert DT1.names == DT2.names
    assert DT1.to_list() == DT2.to_list()
    assert_equals(DT2, DT2[:, :])



#-------------------------------------------------------------------------------
# Casting to other types
#-------------------------------------------------------------------------------

@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
def test_void_to_cat(cat_type):
    src = [None] * 11
    DT = dt.Frame(src)
    DT[0] = cat_type(dt.Type.str32)
    DT_ref = dt.Frame(src, type=cat_type(dt.Type.str32))
    assert_equals(DT, DT_ref)


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
def test_bool_to_cat(cat_type):
    src = [True, False, False, False, True, True]
    DT = dt.Frame(src)
    DT[0] = cat_type(dt.Type.bool8)
    DT_ref = dt.Frame(src, type=cat_type(dt.Type.bool8))
    assert_equals(DT, DT_ref)



@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
@pytest.mark.parametrize('type', [dt.Type.int8,
                                  dt.Type.int16,
                                  dt.Type.int32,
                                  dt.Type.int64])
def test_int_to_cat(cat_type, type):
    src = [100, 500, 100500, 500, 100, 1]
    DT = dt.Frame(src, type=type)
    DT[0] = cat_type(type)
    DT_ref = dt.Frame(src, type=cat_type(type))
    assert_equals(DT, DT_ref)


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
@pytest.mark.parametrize('type', [dt.Type.float32,
                                  dt.Type.float64])
def test_float_to_cat(cat_type, type):
    src = [100.1, -3.14, 500.1, 100500.5, None, 100500900.9, None]
    DT = dt.Frame(src, type=type)
    DT[0] = cat_type(type)
    DT_ref = dt.Frame(src, type=cat_type(type))
    assert_equals(DT, DT_ref)


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
@pytest.mark.parametrize('type', [dt.Type.str32,
                                  dt.Type.str64])
def test_str_to_cat(cat_type, type):
    src = ["cat", "dog", "mouse", "dog", "mouse", "a"]
    DT = dt.Frame(src, type=type)
    DT[0] = cat_type(type)
    DT_ref = dt.Frame(src, type=cat_type(type))
    assert_equals(DT, DT_ref)


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
@pytest.mark.parametrize('type', [dt.Type.date32,
                                  dt.Type.time64])
def test_datetime_to_cat(cat_type, type):
    from datetime import datetime as d
    src = [d(1991, 1, 9, 10, 2, 3, 500),
           d(2014, 7, 2, 1, 3, 5, 100),
           d(2021, 2, 2, 1, 3, 5, 1000),
           d(2014, 7, 2, 1, 3, 5, 100),
           d(2020, 2, 22, 1, 3, 5, 129),
           d(2020, 2, 22, 1, 3, 5, 129),
           d(1950, 8, 2, 4, 3, 5, 10)]
    DT = dt.Frame(src, type=type)
    DT[0] = cat_type(type)
    DT_ref = dt.Frame(src, type=cat_type(type))
    assert_equals(DT, DT_ref)


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
def test_obj_to_cat(cat_type):
    src = [None, "cat", "cat", "dog", "mouse", None, "panda", "dog"]
    DT = dt.Frame(A=src, type=object)
    DT['A'] = cat_type(dt.Type.str32)
    DT_ref = dt.Frame(A=src, type=cat_type(dt.Type.str32))
    assert_equals(DT, DT_ref)


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
def test_str_to_cat_bool(cat_type):
    src = ["True", "weird", "False", "False", "None", "True", "True"]
    src_ref = [True, None, False, False, None, True, True]
    DT = dt.Frame(src)
    DT[0] = cat_type(dt.Type.bool8)
    DT_ref = dt.Frame(src_ref, type=cat_type(dt.Type.bool8))
    assert_equals(DT, DT_ref)


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
def test_bool_to_cat_str(cat_type):
    src = [True, None, False, False, None, True, True]
    src_ref = ["True", None, "False", "False", None, "True", "True"]
    DT = dt.Frame(src)
    DT[0] = cat_type(dt.Type.str32)
    DT_ref = dt.Frame(src_ref, type=cat_type(dt.Type.str32))
    assert_equals(DT, DT_ref)


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
def test_str_to_cat_int(cat_type):
    src = ["weird", "500", "100500", None, "100500900"]
    src_ref = [None, 500, 100500, None, 100500900]
    DT = dt.Frame(src)
    DT[0] = cat_type(dt.Type.int32)
    DT_ref = dt.Frame(src_ref, type=cat_type(dt.Type.int32))
    assert_equals(DT, DT_ref)


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
def test_int_to_cat_str(cat_type):
    src = [None, 500, 100500, None, 100500900]
    src_ref = [None, "500", "100500", None, "100500900"]
    DT = dt.Frame(src)
    DT[0] = cat_type(dt.Type.str32)
    DT_ref = dt.Frame(src_ref, type=cat_type(dt.Type.str32))
    assert_equals(DT, DT_ref)


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
def test_str_to_cat_float(cat_type):
    src = ["100.1", "500.1", "100500.5", None, "100500900.9", "yeah"]
    src_ref = [100.1, 500.1, 100500.5, None, 100500900.9, None]
    DT = dt.Frame(src)
    DT[0] = cat_type(dt.Type.float32)
    DT_ref = dt.Frame(src_ref, type=cat_type(dt.Type.float32))
    assert_equals(DT, DT_ref)


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
def test_float_to_cat_str(cat_type):
    src = [100.1, 500.1, 100500.5, None, 100500900.9, None]
    src_ref = ["100.1", "500.1", "100500.5", None, "100500900.9", None]
    DT = dt.Frame(src)
    DT[0] = cat_type(dt.Type.str32)
    DT_ref = dt.Frame(src_ref, type=cat_type(dt.Type.str32))
    assert_equals(DT, DT_ref)



#-------------------------------------------------------------------------------
# Conversion to other formats
#-------------------------------------------------------------------------------

@pytest.mark.parametrize('t', [dt.Type.cat8,
                               dt.Type.cat16,
                               dt.Type.cat32])
def test_repr_strings_in_terminal(t):
    tcat = t(dt.Type.str32)
    DT = dt.Frame(Animals=["cat", "dog", None, "cat", "guinea piglet"],
                  types = [tcat])
    type_name = tcat.name + (" " if t is dt.Type.cat8 else "")
    assert str(DT) == (
        "   | Animals      \n"
        "   | " + type_name + " \n"
        "-- + -------------\n"
        " 0 | cat          \n"
        " 1 | dog          \n"
        " 2 | NA           \n"
        " 3 | cat          \n"
        " 4 | guinea piglet\n"
        "[5 rows x 1 column]\n"
    )


@pytest.mark.parametrize('t', [dt.Type.cat8,
                               dt.Type.cat16,
                               dt.Type.cat32])
def test_repr_numbers_in_terminal(t):
    tcat = t(dt.Type.int32)
    DT = dt.Frame(Prime_Numbers=[3, 5, None, 47, 7],
                  types = [tcat])
    type_name = tcat.name + ("  " if t is dt.Type.cat8 else " ")
    assert str(DT) == (
        "   | Prime_Numbers\n"
        "   | " + type_name + "\n"
        "-- + -------------\n"
        " 0 | 3            \n"
        " 1 | 5            \n"
        " 2 | NA           \n"
        " 3 | 47           \n"
        " 4 | 7            \n"
        "[5 rows x 1 column]\n"
    )



#-------------------------------------------------------------------------------
# Getting categories
#-------------------------------------------------------------------------------

def test_categories_wrong_type():
    DT = dt.Frame(range(10))
    msg = r"Invalid column of type int32 in categories\(f\.C0\)"
    with pytest.raises(TypeError, match=msg):
        DT[:, dt.categories(f.C0)]


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
def test_categories_zero_rows(cat_type):
    src = [[]]
    data_type = dt.Type.int32
    DT = dt.Frame(src, type=cat_type(data_type))
    DT_cats = DT[:, dt.categories(f[:])]
    DT_ref = dt.Frame(src, type=data_type)
    assert_equals(DT_cats, DT_ref)


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
def test_categories_void(cat_type):
    src = [None] * 11
    DT = dt.Frame(src, type=cat_type(dt.Type.void))
    DT_cats = DT[:, dt.categories(f[:])]
    DT_ref = dt.Frame([None])
    assert_equals(DT_cats, DT_ref)


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
def test_categories_one_column(cat_type):
    src = [None, "cat", "dog", None, "mouse", "cat"] * 7
    DT = dt.Frame([src], type=cat_type(dt.Type.str32))
    DT_cats = DT[:, dt.categories(f.C0)]
    DT_ref = dt.Frame([None, "cat", "dog", "mouse"])
    assert_equals(DT_cats, DT_ref)


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
def test_categories_one_column_plus_orig(cat_type):
    src = [None, "cat", "dog", None, "mouse", "cat"]
    DT = dt.Frame([src], type=cat_type(dt.Type.str32))
    DT_cats = DT[:, [f.C0, dt.categories(f.C0)]]
    DT_ref = dt.Frame([src,
                      [None, "cat", "dog", "mouse"] + [None] * 2],
                      types=[cat_type(dt.Type.str32), dt.Type.str32])
    assert_equals(DT_cats, DT_ref)


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
def test_categories_multicolumn_even_ncats(cat_type):
    src_int = [None, 100, 500, None, 100, 100500, 100, 500]
    src_str = [None, "dog", "mouse", None, "dog", "cat", "dog", "mouse"]
    DT = dt.Frame([src_int, src_str],
                  types=[cat_type(dt.Type.int32), cat_type(dt.Type.str32)])
    DT_cats = DT[:, dt.categories(f[:])]
    DT_ref = dt.Frame([[None, 100, 500, 100500],
                       [None, "cat", "dog", "mouse"]])
    assert_equals(DT_cats, DT_ref)


@pytest.mark.parametrize('cat_type', [dt.Type.cat8,
                                      dt.Type.cat16,
                                      dt.Type.cat32])
def test_categories_multicolumn_uneven_ncats(cat_type):
    src_int = [None, 100, 500, None, 100, 100500, 100, 500]
    src_str = [None, "dog", None, None, "dog", "cat", "dog", None]
    DT = dt.Frame([src_int, src_str],
                  types=[cat_type(dt.Type.int32), cat_type(dt.Type.str32)])
    DT_cats = DT[:, dt.categories(f[:])]
    DT_ref = dt.Frame([[None, 100, 500, 100500] + [None] * 4,
                       [None, "cat", "dog"] + [None] * 5])
    assert_equals(DT_cats, DT_ref)


