#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2021 H2O.ai
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
# Test creating a datatable from various sources
#
#-------------------------------------------------------------------------------
import math
import os
import pytest
import random
import datatable as dt
from datatable import ltype, stype
from datatable.exceptions import DatatableWarning
from datatable.internal import frame_integrity_check
from tests import list_equals, assert_equals


#-------------------------------------------------------------------------------
# Test wrong parameters
#-------------------------------------------------------------------------------

def test_types_type():
    msg = r"You can pass either parameter types or type to Frame\(\) " \
          r"constructor, but not both at the same time"
    with pytest.raises(TypeError, match=msg):
        dt.Frame(types=[], type="float32")


def test_bad_type():
    msg = r"Cannot create Type object from -1"
    with pytest.raises(ValueError, match=msg):
        dt.Frame(type=-1)


def test_bad_types():
    msg = r"Argument types in Frame\(\) constructor should be a list of " \
          r"types, instead received <class 'float'>"
    with pytest.raises(TypeError, match=msg):
        dt.Frame([], types=2.5)


def test_unknown_arg():
    msg = r"Frame\(\) constructor got an unexpected keyword argument 'dtype'"
    with pytest.raises(TypeError, match=msg):
        dt.Frame([1], dtype="int32")


def test_unknown_args1():
    msg = r"Frame\(\) constructor got 2 unexpected keyword arguments: " \
          r"'A' and 'B'"
    with pytest.raises(TypeError, match=msg):
        dt.Frame([1], A=1, B=2)


def test_unknown_args2():
    msg = r"Frame\(\) constructor got 3 unexpected keyword arguments: " \
          r"'A', 'B' and 'C'"
    with pytest.raises(TypeError, match=msg):
        dt.Frame([1], A=1, B=2, C=3)


def test_unknown_args3():
    msg = r"Frame\(\) constructor got 4 unexpected keyword arguments: " \
          r"'A', 'B', ..., 'D'"
    with pytest.raises(TypeError, match=msg):
        dt.Frame([1], A=1, B=2, C=3, D=4)


def test_unknown_args4():
    msg = r"Frame\(\) constructor got 5 unexpected keyword arguments: " \
          r"'A', 'B', ..., 'E'"
    with pytest.raises(TypeError, match=msg):
        dt.Frame([1], A=1, B=2, C=3, D=4, E=5)


def test_types_dict():
    msg = "When parameter types is a dictionary, column names must " \
          "be explicitly specified"
    with pytest.raises(TypeError, match=msg):
        dt.Frame([1, 2, 3], stypes={"A": float})


def test_create_from_set():
    msg = "Cannot create Frame from <class 'set'>"
    with pytest.raises(TypeError, match=msg):
        dt.Frame({1, 13, 15, -16, -10, 7, 9, 1})


def test_wrong_source():
    msg = "Cannot create a column from <class 'int'>"
    with pytest.raises(TypeError, match=msg):
        dt.Frame(A=[1], B=2)


def test_wrong_source_heavy():
    # Repeatedly try to fail the constructor; make sure that the internal state
    # does not get corrupted in the process...
    for _ in range(100):
        try:
            dt.Frame(A=[1], B=2)
        except TypeError:
            pass


def test_different_column_lengths():
    msg = r"Column 1 has different number of rows \(3\) than the preceding " \
          r"columns \(10\)"
    with pytest.raises(ValueError, match=msg):
        dt.Frame([range(10), [3, 4, 6]])


def test_from_frame_as_column():
    DT = dt.Frame(A=[1, 2, 3], B=[7, 4, 1])
    msg = "A column cannot be constructed from a Frame with 2 columns"
    with pytest.raises(ValueError, match=msg):
        dt.Frame(X=DT)




#-------------------------------------------------------------------------------
# Create empty Frame
#-------------------------------------------------------------------------------

def test_create_from_nothing():
    d0 = dt.Frame()
    frame_integrity_check(d0)
    assert d0.shape == (0, 0)
    assert d0.names == tuple()
    assert d0.types == []


def test_create_from_none():
    d0 = dt.Frame(None)
    frame_integrity_check(d0)
    assert d0.shape == (0, 0)
    assert d0.names == tuple()
    assert d0.types == []


def test_create_from_empty_list():
    d0 = dt.Frame([])
    frame_integrity_check(d0)
    assert d0.shape == (0, 0)
    assert d0.names == tuple()
    assert d0.types == []


def test_create_from_empty_list_with_params():
    d0 = dt.Frame([], names=[], types=[])
    frame_integrity_check(d0)
    assert d0.shape == (0, 0)
    d1 = dt.Frame([], type=dt.int64)
    frame_integrity_check(d1)
    assert d1.shape == (0, 0)


def test_create_from_nothing_with_names():
    d0 = dt.Frame(names=["A", "B"])
    frame_integrity_check(d0)
    assert d0.shape == (0, 2)
    assert d0.names == ("A", "B")


def test_create_from_empty_list_bad():
    with pytest.raises(ValueError) as e:
        dt.Frame([], stypes=["int32", "str32"])
    assert ("The types argument contains 2 elements, which is more than the "
            "number of columns being created (0)" in str(e.value))



#-------------------------------------------------------------------------------
# Create from a primitive list
#-------------------------------------------------------------------------------

def test_create_from_list():
    d0 = dt.Frame([1, 2, 3])
    frame_integrity_check(d0)
    assert d0.shape == (3, 1)
    assert d0.names == ("C0", )
    assert d0.ltypes == (ltype.int, )


def test_create_from_tuple():
    d2 = dt.Frame((3, 5, 6, 0))
    frame_integrity_check(d2)
    assert d2.shape == (4, 1)
    assert d2.ltypes == (ltype.int, )


def test_create_from_range():
    d0 = dt.Frame(range(8))
    frame_integrity_check(d0)
    assert d0.shape == (8, 1)
    assert d0.to_list() == [list(range(8))]


@pytest.mark.parametrize("r", [range(1, 3), range(2, 7, 3), range(-1, 4, 2),
                               range(5, 2, -1), range(278, -14, -5),
                               range(2, 2, 2), range(2, 3, 15), range(-3, -1),
                               range(4, 3, 3), range(2, 3, -2),
                               range(4, 6, -10), range(4, 8, -2),
                               range(4, 2, -2)])
def test_create_from_range2(r):
    DT = dt.Frame(A=r)
    frame_integrity_check(DT)
    assert DT.ncols == 1
    assert DT.to_list()[0] == list(r)



#-------------------------------------------------------------------------------
# Create from a list of lists
#-------------------------------------------------------------------------------

def test_create_from_list_of_lists():
    d1 = dt.Frame([[1, 2], [True, False], [.3, -0]], names=list("ABC"))
    frame_integrity_check(d1)
    assert d1.shape == (2, 3)
    assert d1.names == ("A", "B", "C")
    assert d1.ltypes == (ltype.int, ltype.bool, ltype.real)


def test_create_from_list_of_lists_with_stypes_dict():
    d0 = dt.Frame([[4], [9], [3]], names=("a", "b", "c"), stypes={"c": float})
    frame_integrity_check(d0)
    assert d0.names == ("a", "b", "c")
    assert d0.ltypes == (ltype.int, ltype.int, ltype.real)
    assert d0.to_list() == [[4], [9], [3.0]]


def test_create_from_list_of_lists_with_stypes_dict_bad():
    with pytest.raises(TypeError) as e:
        dt.Frame([[4], [9], [3]], stypes={"c": float})
    assert ("When parameter types is a dictionary, column names must be "
            "explicitly specified" == str(e.value))


def test_create_from_list_of_ranges():
    d0 = dt.Frame([range(6), range(0, 12, 2)])
    frame_integrity_check(d0)
    assert d0.shape == (6, 2)
    assert d0.to_list() == [list(range(6)), list(range(0, 12, 2))]


def test_create_from_empty_list_of_lists():
    d6 = dt.Frame([[]])
    frame_integrity_check(d6)
    assert d6.shape == (0, 1)
    assert d6.names == ("C0", )
    assert d6.types == [dt.Type.void]



#-------------------------------------------------------------------------------
# Create from a dict
#-------------------------------------------------------------------------------

def test_create_from_dict():
    d7 = dt.Frame({"A": [1, 5, 10],
                   "B": [True, False, None],
                   "C": ["alpha", "beta", "gamma"]})
    assert d7.shape == (3, 3)
    assert list_equals(d7.names, ("A", "B", "C"))
    assert list_equals(d7.ltypes, (ltype.int, ltype.bool, ltype.str))
    frame_integrity_check(d7)


def test_create_from_dict_error():
    with pytest.raises(TypeError) as e:
        dt.Frame({"A": range(3), "B": list('abc')}, names=("x", "y"))
    assert ("Parameter names cannot be used when constructing a Frame from "
            "a dictionary" == str(e.value))



#-------------------------------------------------------------------------------
# Create from keyword arguments
#-------------------------------------------------------------------------------

def test_create_from_kwargs0():
    d0 = dt.Frame(varname=[1])
    frame_integrity_check(d0)
    assert d0.names == ("varname",)
    assert d0.to_list() == [[1]]


def test_create_from_kwargs1():
    d0 = dt.Frame(A=[1, 2, 3], B=[True, None, False], C=["a", "b", "c"])
    frame_integrity_check(d0)
    assert list_equals(d0.names, ("A", "B", "C"))
    assert list_equals(d0.to_list(), [[1, 2, 3],
                                         [True, None, False],
                                         ["a", "b", "c"]])


def test_create_from_kwargs2():
    d0 = dt.Frame(x=range(4), y=[1, 3, 8, 0], stypes=[dt.int64, dt.float32])
    frame_integrity_check(d0)
    assert d0.shape == (4, 2)
    assert list_equals(d0.names, ("x", "y"))
    assert list_equals(d0.stypes, (dt.int64, dt.float32))
    assert list_equals(d0.to_list(), [[0, 1, 2, 3], [1, 3, 8, 0]])


def test_create_from_kwargs_error():
    with pytest.raises(TypeError) as e:
        dt.Frame(A=range(3), B=list('abc'), names=("x", "y"))
    assert ("Parameter names cannot be used when constructing a Frame from "
            "varkwd arguments" == str(e.value))




#-------------------------------------------------------------------------------
# Create from a typed list
#-------------------------------------------------------------------------------

def test_create_from_typed_list():
    DT = dt.Frame([1, 2, 3] / dt.int16)
    assert DT.stype == dt.int16
    assert DT.to_list() == [[1, 2, 3]]
    frame_integrity_check(DT)


def test_create_from_typed_list2():
    DT = dt.Frame([[1, 2, 3] / dt.int64,
                   [1, 2, 3] / dt.float64])
    assert DT.stypes == (dt.int64, dt.float64)
    assert DT.to_list() == [[1, 2, 3]] * 2
    frame_integrity_check(DT)


def test_create_from_typed_list3():
    DT = dt.Frame(A=[1, 2, 3] / dt.str32,
                  B=[1, 2, 3] / dt.bool8,
                  C=[1, 2, 3] / dt.float64)
    assert_equals(DT, dt.Frame(A=["1", "2", "3"], B=[True, True, True],
                               C=[1.0, 2.0, 3.0]))


def test_create_from_typed_list4():
    DT = dt.Frame({"A": [1, 2, 3] / dt.int8})
    assert_equals(DT, dt.Frame([1, 2, 3], names=["A"], stypes=[dt.int8]))



#-------------------------------------------------------------------------------
# Create from a Frame (copy)
#-------------------------------------------------------------------------------

def test_create_from_frame():
    d0 = dt.Frame(A=[1, 4, 3],
                  B=[False, True, False],
                  C=["str1", "str2", "str3"])
    frame_integrity_check(d0)
    d1 = dt.Frame(d0)
    frame_integrity_check(d1)
    assert_equals(d0, d1)
    # Now check that d1 is a true copy of d0, rather than reference
    del d1[:, "C"]
    d1.nrows = 10
    assert d1.nrows == 10
    assert d1.ncols == 2
    assert d0.nrows == 3
    assert d0.ncols == 3


def test_create_from_frame2():
    d0 = dt.Frame([range(5), [7] * 5, list("owbke")])
    d1 = dt.Frame(d0, names=["A", "B", "C"])
    frame_integrity_check(d1)
    assert d1.to_list() == d0.to_list()
    assert d1.stypes == d0.stypes
    assert d1.names != d0.names
    assert d1.names == ("A", "B", "C")


def test_create_from_frame_error():
    d0 = dt.Frame([[3], [5.78], ["af"]], names=("A", "B", "C"))
    frame_integrity_check(d0)
    with pytest.raises(TypeError) as e1:
        dt.Frame(d0, stypes=[stype.int64, stype.float64, stype.str64])
    assert ("Parameter types is not allowed when making a copy of a "
            "Frame" == str(e1.value))
    with pytest.raises(TypeError) as e2:
        dt.Frame(d0, stypes=[stype.str32])
    assert str(e1.value) == str(e2.value)


def test_create_from_column_frames():
    DT0 = dt.Frame(A=range(5), B=list("dfkjd"),
                   C=[False, True, True, None, True])
    DT1 = dt.Frame(a=DT0["A"], b=DT0["B"], c=DT0["C"])
    assert DT1.names == ("a", "b", "c")
    assert DT1.stypes == DT0.stypes
    assert DT1.to_list() == DT0.to_list()


def test_create_from_doublestar_expansion():
    DT0 = dt.Frame(A=range(3), B=["df", "qe;r", None])
    DT1 = dt.Frame(D=[7.99, -12.5, 0.1], E=[None]*3)
    DT = dt.Frame(**DT0, **DT1)
    assert_equals(DT, dt.cbind(DT0, DT1))



#-------------------------------------------------------------------------------
# Create from a string (fread)
#-------------------------------------------------------------------------------

def test_create_from_string():
    d0 = dt.Frame("""
        A,B,C,D
        1,2,3,boo
        0,5.5,,bar
        ,NaN,1000,""
    """)
    frame_integrity_check(d0)
    assert d0.names == ("A", "B", "C", "D")
    assert d0.ltypes == (dt.ltype.bool, dt.ltype.real, dt.ltype.int,
                         dt.ltype.str)
    assert d0.to_list() == [[True, False, None], [2.0, 5.5, None],
                            [3, None, 1000], ["boo", "bar", ""]]


def test_cannot_create_from_multiple_files(tempfile):
    file1 = tempfile + ".1.csv"
    file2 = tempfile + ".2.csv"
    file3 = tempfile + ".3.csv"
    try:
        with open(file1, "w") as o1, open(file2, "w") as o2, \
                open(file3, "w") as o3:
            o1.write("A,B\nfoo,2\n")
            o2.write("3\n4\n5\n6\n")
            o3.write("qw\n1\n2\n5\n")
        ff = dt.iread(tempfile + ".*.csv")
        assert len(list(ff)) == 3
        msg = r"fread\(\) input contains multiple sources"
        with pytest.raises(IOError, match=msg):
            dt.Frame(tempfile + ".*.csv")
    finally:
        os.remove(file1)
        os.remove(file2)



#-------------------------------------------------------------------------------
# Create from a list of tuples (as rows)
#-------------------------------------------------------------------------------

def test_create_from_list_of_tuples1():
    d0 = dt.Frame([(1, 2.0, "foo"),
                   (3, 1.5, "zee"),
                   (9, 0.1, "xyx"),
                   (0, -10, None)])
    frame_integrity_check(d0)
    assert d0.shape == (4, 3)
    assert d0.ltypes == (ltype.int, ltype.real, ltype.str)
    assert d0.to_list() == [[1, 3, 9, 0],
                            [2.0, 1.5, 0.1, -10.0],
                            ["foo", "zee", "xyx", None]]


def test_create_from_list_of_tuples2():
    d0 = dt.Frame([(1, 3, 5)], names=["a", "b", "c"], stypes=[int, float, str])
    frame_integrity_check(d0)
    assert d0.shape == (1, 3)
    assert d0.ltypes == (ltype.int, ltype.real, ltype.str)
    assert d0.names == ("a", "b", "c")
    assert d0.to_list() == [[1], [3.0], ["5"]]


def test_create_from_list_of_tuples_bad():
    with pytest.raises(TypeError) as e:
        dt.Frame([(1, 2, 3), (3, 4, 5), "4, 5, 6"])
    assert ("The source is not a list of tuples: element 2 is a "
            "<class 'str'>" == str(e.value))

    with pytest.raises(ValueError) as e:
        dt.Frame([(1, 2, 3), (4, 5), (5, 6, 7)])
    assert ("Misshaped rows in Frame() constructor: row 1 contains 2 elements, "
            "while the previous row had 3 elements" == str(e.value))

    with pytest.raises(ValueError) as e:
        dt.Frame([(1, 2, 3)], names=["a", "b"])
    assert ("The names argument contains 2 elements, which is less than "
            "the number of columns being created (3)" == str(e.value))

    with pytest.raises(ValueError) as e:
        dt.Frame([(1, 2, 3)], stypes=(stype.float32,) * 10)
    assert ("The types argument contains 10 elements, which is more than "
            "the number of columns being created (3)" == str(e.value))


def test_create_from_list_of_namedtuples():
    from collections import namedtuple
    Person = namedtuple("Person", ["name", "age", "sex"])
    d0 = dt.Frame([Person("Grogg", 21, "M"),
                   Person("Alexx", 14, "M"),
                   Person("Fiona", 24, "F")])
    frame_integrity_check(d0)
    assert d0.shape == (3, 3)
    assert d0.names == ("name", "age", "sex")
    assert d0.ltypes == (ltype.str, ltype.int, ltype.str)
    assert d0.to_list() == [["Grogg", "Alexx", "Fiona"],
                            [21, 14, 24], ["M", "M", "F"]]


def test_create_from_list_of_namedtuples_names_override():
    from collections import namedtuple
    abc = namedtuple("ABC", ["a", "b", "c"])
    d0 = dt.Frame([abc(5, 6, 7), abc(3, 2, 1)], names=["x", "y", "z"])
    frame_integrity_check(d0)
    assert d0.shape == (2, 3)
    assert d0.names == ("x", "y", "z")
    assert d0.ltypes == (ltype.int,) * 3
    assert d0.to_list() == [[5, 3], [6, 2], [7, 1]]



#-------------------------------------------------------------------------------
# Create from a list of dictionaries (as rows)
#-------------------------------------------------------------------------------

def test_create_from_list_of_dicts1():
    d0 = dt.Frame([{"a": 5, "b": 7, "c": "Hey"},
                   {"a": 99},
                   {"a": -4, "c": "Yay", "d": 2.17},
                   {"d": 1e10}, {}])
    frame_integrity_check(d0)
    assert d0.shape == (5, 4)
    assert d0.names == ("a", "b", "c", "d")
    assert d0.ltypes == (ltype.int, ltype.int, ltype.str, ltype.real)
    assert d0.to_list() == [[5, 99, -4, None, None],
                            [7, None, None, None, None],
                            ["Hey", None, "Yay", None, None],
                            [None, None, 2.17, 1e10, None]]


def test_create_from_list_of_dicts2():
    d0 = dt.Frame([{"foo": 11, "bar": 34}, {"argh": 17, "foo": 4}, {"_": 0}])
    frame_integrity_check(d0)
    assert d0.shape == (3, 4)
    assert d0.names == ("foo", "bar", "argh", "_")
    assert d0.to_list() == [[11, 4, None], [34, None, None],
                            [None, 17, None], [None, None, 0]]


def test_create_from_list_of_dicts_with_names1():
    d0 = dt.Frame([{"a": 12, "b": 77797, "c": "Rose"},
                   {"a": 37},
                   {"a": 80, "c": "Lily", "d": 3.14159},
                   {"d": 1.7e10}, {}],
                  names=["c", "a", "d", "e"])
    frame_integrity_check(d0)
    assert d0.shape == (5, 4)
    assert d0.names == ("c", "a", "d", "e")
    assert d0.ltypes == (ltype.str, ltype.int, ltype.real, ltype.void)
    assert d0.to_list() == [["Rose", None, "Lily", None, None],
                            [12, 37, 80, None, None],
                            [None, None, 3.14159, 1.7e10, None],
                            [None, None, None, None, None]]


def test_create_from_list_of_dicts_with_names2():
    d0 = dt.Frame([{"a": 5}, {"b": 6}, {"c": 11}, {}], names=[])
    frame_integrity_check(d0)
    assert d0.shape == (0, 0)


def test_create_from_list_of_dicts_bad1():
    with pytest.raises(TypeError) as e:
        dt.Frame([{"a": 5}, {"b": 6}, None, {"c": 11}], names=[])
    assert ("The source is not a list of dicts: element 2 is a "
            "<class 'NoneType'>" == str(e.value))
    with pytest.raises(TypeError) as e:
        dt.Frame([{"a": 5}, {"b": 6}, None, {"c": 11}])
    assert ("The source is not a list of dicts: element 2 is a "
            "<class 'NoneType'>" == str(e.value))


def test_create_from_list_of_dicts_bad2():
    with pytest.raises(TypeError) as e:
        dt.Frame([{"a": 11}, {1: 4}])
    assert ("Invalid data in Frame() constructor: row 1 dictionary contains "
            "a key of type <class 'int'>, only string keys are allowed" ==
            str(e.value))


def test_create_from_list_of_dicts_bad3():
    with pytest.raises(TypeError) as e:
        dt.Frame([{"a": 11}, {"b": 4}], stypes=[int, int])
    assert ("If the Frame() source is a list of dicts, then either the names "
            "list has to be provided explicitly, or stypes parameter has "
            "to be a dictionary (or missing)" == str(e.value))



#-------------------------------------------------------------------------------
# Type auto-detection
#-------------------------------------------------------------------------------

def test_auto_str32_1():
    d0 = dt.Frame(["start", None, "end"])
    frame_integrity_check(d0)
    assert d0.stypes == (stype.str32,)


def test_auto_str32_2():
    DT = dt.Frame([None, "1a", "12", "fini"])
    frame_integrity_check(DT)
    assert DT.stype == stype.str32
    assert DT.to_list() == [[None, "1a", "12", "fini"]]


def test_auto_str32_3():
    DT = dt.Frame([math.nan, "start", None, "end"])
    frame_integrity_check(DT)
    assert DT.types == [dt.Type.str32]
    assert DT.to_list() == [[None, "start", None, "end"]]


def test_create_from_strings_and_nans1():
    DT = dt.Frame(["a", 'bb', math.nan])
    frame_integrity_check(DT)
    assert DT.type == dt.Type.str32
    assert DT.to_list() == [["a", "bb", None]]


def test_create_from_strings_and_nans2():
    DT = dt.Frame([math.nan, None, math.nan, "a", 'bb'])
    frame_integrity_check(DT)
    assert DT.type == dt.Type.str32
    assert DT.to_list() == [[None, None, None, "a", "bb"]]


def test_auto_str32_3():
    msg = "Cannot create column: element at index 1 is of type " \
          "<class 'float'>, whereas previous elements were boolean"
    with pytest.raises(TypeError, match=msg):
        DT = dt.Frame([True, 5.75, 9, "hi", 4, False])


def test_auto_str64():
    hello = 'hello' * 50000
    d0 = dt.Frame([hello] * 10000)
    frame_integrity_check(d0)
    assert d0.stypes == (stype.str64,)


def test_create_large_string_column():
    s = 'a' * (1 << 20)
    DT = dt.Frame([s] * (1 << 12))
    frame_integrity_check(DT)
    assert DT.stype == dt.str64
    assert DT[-1, 0] == s


def test_no_auto_object_column():
    src = [3, "ha!", dt, [0]]
    msg = r"Cannot create column: element at index 1 is of type " \
          r"<class 'str'>, whereas previous elements were int32"
    with pytest.raises(TypeError, match=msg):
        dt.Frame(src)
    # Requesting 'object' stype explicitly should create a valid frame
    DT = dt.Frame(src, type=object)
    assert DT.shape == (4, 1)
    assert DT.types == [dt.Type.obj64]
    assert DT.to_list() == [src]



#-------------------------------------------------------------------------------
# types= argument
#-------------------------------------------------------------------------------

def test_create_from_nones():
    d0 = dt.Frame([None, None, None])
    frame_integrity_check(d0)
    assert d0.stypes == (stype.void, )
    assert d0.shape == (3, 1)


def test_create_as_int32():
    d0 = dt.Frame([1, 2, 5, 3.14, (1, 2)], type=stype.int32)
    frame_integrity_check(d0)
    assert d0.stypes == (stype.int32, )
    assert d0.shape == (5, 1)
    assert d0.to_list() == [[1, 2, 5, 3, None]]


def test_create_as_float64():
    d0 = dt.Frame([[1, 2, 3, 4, 5, None],
                   [2.7, "3.1", False, "foo", 10**1000, -12**321]],
                  stype=float)
    frame_integrity_check(d0)
    assert d0.stypes == (stype.float64, stype.float64)
    assert d0.shape == (6, 2)
    assert d0.to_list() == [[1.0, 2.0, 3.0, 4.0, 5.0, None],
                            [2.7, 3.1, 0, None, math.inf, -math.inf]]


def test_create_as_str32():
    d0 = dt.Frame([1, 2.7, "foo", None, (3, 4)], stype=stype.str32)
    frame_integrity_check(d0)
    assert d0.stypes == (stype.str32, )
    assert d0.shape == (5, 1)
    assert d0.to_list() == [["1", "2.7", "foo", None, "(3, 4)"]]


def test_create_as_str64():
    d0 = dt.Frame(range(10), type=dt.Type.str64)
    frame_integrity_check(d0)
    assert d0.stypes == (stype.str64, )
    assert d0.shape == (10, 1)
    assert d0.to_list() == [[str(n) for n in range(10)]]


@pytest.mark.parametrize("st", [stype.int8, stype.int16, stype.int64,
                                stype.float32, stype.float64, stype.str32])
def test_create_range_as_stype(st):
    d0 = dt.Frame(range(10), stype=st)
    frame_integrity_check(d0)
    assert d0.stypes == (st,)
    if st == stype.str32:
        assert d0.to_list()[0] == [str(x) for x in range(10)]
    else:
        assert d0.to_list()[0] == list(range(10))


@pytest.mark.parametrize('t', [stype.float64, dt.Type.float64, float, 'double',
                               'float64'])
def test_create_from_various_types(t):
    DT = dt.Frame(range(5), type=t)
    frame_integrity_check(DT)
    assert DT.type == dt.Type.float64
    assert_equals(DT, dt.Frame([0.0, 1.0, 2.0, 3.0, 4.0]))


def test_create_from_numpy_dtype1(np):
    DT = dt.Frame(range(5), type=np.dtype('float64'))
    assert_equals(DT, dt.Frame([0.0, 1.0, 2.0, 3.0, 4.0]))


def test_create_from_numpy_dtype2(np):
    DT = dt.Frame(range(5), type=np.float64)
    assert_equals(DT, dt.Frame([0.0, 1.0, 2.0, 3.0, 4.0]))


def test_create_from_arrow_type(pa):
    DT = dt.Frame(range(5), type=pa.float64())
    assert_equals(DT, dt.Frame([0.0, 1.0, 2.0, 3.0, 4.0]))



#-------------------------------------------------------------------------------
# Create with names
#-------------------------------------------------------------------------------

def test_create_names0():
    d1 = dt.Frame(range(10), names=["xyz"])
    d2 = dt.Frame(range(10), names=("xyz",))
    frame_integrity_check(d1)
    frame_integrity_check(d2)
    assert d1.shape == d2.shape == (10, 1)
    assert d1.names == d2.names == ("xyz",)


def test_create_names_bad1():
    with pytest.raises(ValueError) as e:
        dt.Frame(range(10), names=["a", "b"])
    assert ("The names argument contains 2 elements, which is more than the "
            "number of columns being created (1)" == str(e.value))


def test_create_names_bad2():
    with pytest.raises(TypeError) as e:
        dt.Frame([[1], [2], [3]], names="xyz")
    assert ("Argument names in Frame() constructor should be a list of "
            "strings, instead received <class 'str'>" == str(e.value))


def test_create_names_bad3():
    with pytest.raises(TypeError) as e:
        dt.Frame(range(5), names={"x": 1})
    assert ("Argument names in Frame() constructor should be a list of "
            "strings, instead received <class 'dict'>" == str(e.value))


def test_create_names_bad4():
    with pytest.raises(TypeError) as e:
        dt.Frame(range(5), names=[3])
    assert ("Invalid names list: element 0 is not a string"
            in str(e.value))



#-------------------------------------------------------------------------------
# Create from Pandas
#-------------------------------------------------------------------------------

def test_create_from_pandas(pd):
    src = {"A": [2, 5, 8], "B": ["e", "r", "qq"]}
    p = pd.DataFrame(src)
    d = dt.Frame(p)
    frame_integrity_check(d)
    assert d.shape == (3, 2)
    assert d.to_dict() == src


def test_create_from_pandas2(pd, np):
    p = pd.DataFrame(np.ones((3, 5)))
    d = dt.Frame(p)
    frame_integrity_check(d)
    assert d.shape == (3, 5)
    assert d.names == ("0", "1", "2", "3", "4")


def test_create_from_pandas_series(pd):
    src = [1, 5, 9, -12, 0, 123]
    p = pd.Series(src)
    d = dt.Frame(p)
    frame_integrity_check(d)
    assert d.shape == (len(src), 1)
    assert d.to_list() == [src]


def test_create_from_pandas_with_names(pd):
    p = pd.DataFrame({"A": [2, 5, 8], "B": ["e", "r", "qq"]})
    d = dt.Frame(p, names=["miniature", "miniscule"])
    frame_integrity_check(d)
    assert d.shape == (3, 2)
    assert list_equals(d.names, ("miniature", "miniscule"))


def test_create_from_pandas_series_with_names(pd):
    p = pd.Series([10000, 5, 19, -12])
    d = dt.Frame(p, names=["ha!"])
    frame_integrity_check(d)
    assert d.shape == (4, 1)
    assert d.names == ("ha!", )
    assert d.to_list() == [[10000, 5, 19, -12]]


def test_create_from_pandas_float16_series(pd):
    src = [1.5, 2.6, 7.8]
    p = pd.Series(src, dtype="float16")
    d = dt.Frame(p)
    frame_integrity_check(d)
    assert d.stypes == (stype.float32, )
    assert d.shape == (3, 1)
    # The precision of `float16`s is too low for `list_equals()` method.
    res = d.to_list()[0]
    assert all(abs(src[i] - res[i]) < 1e-3 for i in range(3))


def test_create_from_pandas_float16_dataframe(pd):
    p = pd.DataFrame([[1, 3, 5], [7, 8, 9]], dtype="float16")
    d = dt.Frame(p)
    frame_integrity_check(d)
    assert d.stypes == (stype.float32,) * 3
    assert d.shape == (2, 3)


@pytest.mark.usefixtures("pandas")
def test_create_from_pandas_issue1235():
    df = dt.fread("A\n" + "\U00010000" * 50).to_pandas()
    table = dt.Frame(df)
    frame_integrity_check(table)
    assert table.shape == (1, 1)
    assert table[0, 0] == "\U00010000" * 50


def test_create_from_pandas_with_stypes(pd):
    with pytest.raises(TypeError) as e:
        p = pd.DataFrame([[1, 2, 3]])
        dt.Frame(p, stype=str)
    assert ("Argument types is not supported in Frame() constructor "
            "when creating a Frame from pandas DataFrame" == str(e.value))


def test_create_from_pandas_with_bad_names(pd):
    with pytest.raises(ValueError) as e:
        p = pd.DataFrame([[1, 2, 3]])
        dt.Frame(p, names=["A", "Z"])
    assert ("The names argument contains 2 elements, which is less than the "
            "number of columns being created (3)" == str(e.value))


def test_create_from_pandas_string_with_nans(pd):
    pf = pd.DataFrame(["Hello", math.nan, "world!"])
    df = dt.Frame(pf)
    assert_equals(df, dt.Frame(["Hello", None, "world!"], names=["0"]))


def test_create_from_pandas_boolean_with_nans(pd):
    pf = pd.DataFrame({"A": [True, math.nan, False]})
    df = dt.Frame(pf)
    assert_equals(df, dt.Frame(A=[True, None, False]))


def test_create_from_pandas_with_duplicate_names(pd):
    # See issue 1816
    Y = pd.DataFrame([[1, 2, 3]], columns=list("AAA"))
    with pytest.warns(DatatableWarning):
        X = dt.Frame(Y)
    assert X.shape == (1, 3)
    assert X.names == ("A", "A.0", "A.1")
    assert X.to_list() == [[1], [2], [3]]


def test_create_from_pandas_categorical(pd):
    df = pd.cut(pd.Series(range(10)), bins=3)
    DT = dt.Frame(df)
    assert_equals(DT, dt.Frame(["(-0.009, 3.0]"] * 4 +
                               ["(3.0, 6.0]"] * 3 +
                               ["(6.0, 9.0]"] * 3))


def test_issue2517(pd):
    DT = dt.Frame(pd.DataFrame(['секрет秘密']))
    assert_equals(DT, dt.Frame(['секрет秘密'], names=["0"]))


def test_create_from_pandas_nones(pd):
    df = pd.DataFrame(dict(N=[None] * 10))
    DT = dt.Frame(df)
    assert_equals(DT, dt.Frame(N=[None]*10))


@pytest.mark.usefixtures("pandas")
def test_void_frame_roundtrip():
    pf = dt.Frame([None]*10).to_pandas()
    DT = dt.Frame(pf)
    assert_equals(DT, dt.Frame([None] * 10))


@pytest.mark.usefixtures("release_only")
def test_create_from_pandas_large(pd):
    # See issue 3169
    text = "0123456789" * 30
    nrows = 2 * 10**7
    PD = pd.DataFrame([text] * nrows)
    DT = dt.Frame(PD)
    assert DT.shape == (nrows, 1)
    assert DT.type == dt.Type.str64
    assert DT[0, 0] == text
    assert DT.nunique1() == 1


#-------------------------------------------------------------------------------
# Create from Numpy
#-------------------------------------------------------------------------------

def test_create_from_0d_numpy_array(numpy):
    a = numpy.array(100)
    d = dt.Frame(a)
    frame_integrity_check(d)
    assert d.shape == (1, 1)
    assert d.names == ("C0", )
    assert d.to_list() == [[100]]


def test_create_from_1d_numpy_array(numpy):
    a = numpy.array([1, 2, 3])
    d = dt.Frame(a)
    frame_integrity_check(d)
    assert d.shape == (3, 1)
    assert d.names == ("C0", )
    assert d.to_list() == [[1, 2, 3]]


def test_create_from_2d_numpy_array(numpy):
    a = numpy.array([[5, 4, 3, 10, 12], [-2, -1, 0, 1, 7]])
    d = dt.Frame(a)
    frame_integrity_check(d)
    assert d.shape == a.shape
    assert d.names == ("C0", "C1", "C2", "C3", "C4")
    assert d.to_list() == a.T.tolist()


def test_create_from_3d_numpy_array(numpy):
    a = numpy.array([[[1, 2, 3]]])
    with pytest.raises(ValueError) as e:
        dt.Frame(a)
    assert "Cannot create Frame from a 3-D numpy array" in str(e.value)


def test_create_from_string_numpy_array(numpy):
    a = numpy.array(["alef", "bet", "gimel", "dalet", "he", "юйґї"])
    d = dt.Frame(a)
    frame_integrity_check(d)
    assert d.shape == (6, 1)
    assert d.names == ("C0", )
    assert d.to_list() == [a.tolist()]


def test_create_from_masked_numpy_array1(numpy):
    a = numpy.array([True, False, True, False, True])
    m = numpy.array([False, True, False, False, True])
    arr = numpy.ma.array(a, mask=m)
    d = dt.Frame(arr)
    frame_integrity_check(d)
    assert d.shape == (5, 1)
    assert d.stypes == (stype.bool8, )
    assert d.to_list() == [[True, None, True, False, None]]


def test_create_from_masked_numpy_array2(numpy):
    n = 100
    a = numpy.random.randn(n) * 1000
    m = numpy.random.randn(n) > 1
    arr = numpy.ma.array(a, mask=m, dtype="int16")
    d = dt.Frame(arr)
    frame_integrity_check(d)
    assert d.shape == (n, 1)
    assert d.stypes == (stype.int16, )
    assert d.to_list() == [arr.tolist()]


def test_create_from_masked_numpy_array3(numpy):
    n = 100
    a = numpy.random.randn(n) * 1e6
    m = numpy.random.randn(n) > 1
    arr = numpy.ma.array(a, mask=m, dtype="int32")
    d = dt.Frame(arr)
    frame_integrity_check(d)
    assert d.shape == (n, 1)
    assert d.stypes == (stype.int32, )
    assert d.to_list() == [arr.tolist()]


def test_create_from_masked_numpy_array4(numpy):
    n = 10
    a = numpy.random.randn(n) * 10
    m = numpy.random.randn(n) > 1
    arr = numpy.ma.array(a, mask=m, dtype="float64")
    d = dt.Frame(arr)
    frame_integrity_check(d)
    assert d.shape == (n, 1)
    assert d.stypes == (stype.float64, )
    assert list_equals(d.to_list(), [arr.tolist()])


def test_create_from_numpy_array_with_names(numpy):
    a = numpy.array([1, 2, 3])
    d = dt.Frame(a, names=["gargantuan"])
    frame_integrity_check(d)
    assert d.shape == (3, 1)
    assert d.names == ("gargantuan", )
    assert d.to_list() == [[1, 2, 3]]


def test_create_from_numpy_float16(numpy):
    src = [11.11, -3.162, 4.93, 0, 17.2]
    a = numpy.array(src, dtype="float16")
    d = dt.Frame(a)
    frame_integrity_check(d)
    assert d.stypes == (stype.float32, )
    assert d.shape == (len(src), 1)
    # The precision of `float16`s is too low for `list_equals()` method.
    res = d.to_list()[0]
    assert all(abs(src[i] - res[i]) < 1e-3 for i in range(3))


def test_create_from_list_of_numpy_arrays(numpy):
    df = dt.Frame([numpy.random.randint(2**30, size=100, dtype=numpy.int64),
                   numpy.random.randn(100)], names=["A", "B"])
    frame_integrity_check(df)
    assert df.shape == (100, 2)
    assert df.names == ("A", "B")
    assert df.stypes == (stype.int64, stype.float64)


def test_create_from_dict_of_numpy_arrays(numpy):
    df = dt.Frame({"A": numpy.random.randn(67),
                   "B": numpy.random.randn(67),
                   "C": numpy.random.randn(67)})
    frame_integrity_check(df)
    assert df.shape == (67, 3)
    assert df.stypes == (stype.float64,) * 3
    assert list_equals(df.names, ("A", "B", "C"))


def test_create_from_mixed_sources(numpy):
    df = dt.Frame({"A": numpy.random.randn(5),
                   "B": range(5),
                   "C": ["foo", "baw", "garrgh", "yex", "fin"],
                   "D": numpy.array([5, 8, 1, 3, 5813], dtype="int32")})
    frame_integrity_check(df)
    assert df.shape == (5, 4)
    assert list_equals(df.names, ("A", "B", "C", "D"))
    assert list_equals(df.stypes, (stype.float64, stype.int32, stype.str32,
                                      stype.int32))


def test_create_from_datetime_array(numpy):
    from datetime import datetime as d
    a = numpy.zeros(dtype="<M8[s]", shape=(10,))
    df = dt.Frame(a)
    frame_integrity_check(df)
    assert df.shape == (10, 1)
    assert df.types == [dt.Type.time64]
    assert df.to_list() == [[d(1970, 1, 1, 0, 0, 0)] * 10]


def test_create_from_numpy_bools1(np):
    t = np.bool_(1)
    f = np.bool_(0)
    assert_equals(dt.Frame([t]), dt.Frame([True]))
    assert_equals(dt.Frame([f]), dt.Frame([False]))


def test_create_from_numpy_bools2(np):
    t = np.bool_(1)
    f = np.bool_(0)
    assert_equals(dt.Frame([[t]]), dt.Frame([True]))
    assert_equals(dt.Frame([[f]]), dt.Frame([False]))


def test_create_from_numpy_bools3(np):
    t = np.bool_(1)
    f = np.bool_(0)
    assert_equals(dt.Frame([t, f, f, t]),
                  dt.Frame([True, False, False, True]))


def test_create_from_numpy_bools4(np):
    t = np.bool_(1)
    f = np.bool_(0)
    assert_equals(dt.Frame([True, t, f, False, None]),
                  dt.Frame([True, True, False, False, None]))


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_create_from_numpy_bools_random(np, seed):
    random.seed(seed)
    src1 = [random.choice([True, False, None, np.bool_(1), np.bool_(0)])
            for i in range(int(random.expovariate(0.1) + 1))]
    src2 = [x if x is None else bool(x)
            for x in src1]
    if random.random() > 0.5:
        src1 = [src1]
    DT1 = dt.Frame(src1)
    DT2 = dt.Frame(src2)
    assert_equals(DT1, DT2)



def test_create_from_numpy_ints(numpy):
    DT = dt.Frame(A=[numpy.int32(3), numpy.int32(78), numpy.int32(0)])
    frame_integrity_check(DT)
    assert DT.shape == (3, 1)
    assert DT.stype == dt.int32
    assert DT.to_list() == [[3, 78, 0]]


def test_create_from_numpy_ints2(numpy):
    DT = dt.Frame([[numpy.int8(55), numpy.int8(7), numpy.int8(4)],
                   [None, numpy.int16(16), numpy.int16(-3)],
                   [numpy.int32(356), numpy.int32(2), numpy.int32(0)],
                   [numpy.int64(7), numpy.int64(34871), numpy.int64(66)]])
    frame_integrity_check(DT)
    assert DT.shape == (3, 4)
    assert DT.stypes == (dt.int8, dt.int16, dt.int32, dt.int64)
    assert DT.to_list() == [[55, 7, 4], [None, 16, -3], [356, 2, 0],
                            [7, 34871, 66]]


def test_create_from_numpy_floats(numpy):
    np = numpy
    DT = dt.Frame([[np.float32(4), np.float32(3.7), np.float32(88.5)],
                   [np.float16(2), np.float16(2.5), np.float16(7.75)],
                   [np.float64(0), np.float64(4.4), np.float64(99.999)]])
    frame_integrity_check(DT)
    assert DT.shape == (3, 3)
    assert DT.stypes == (dt.float32, dt.float32, dt.float64)
    assert DT.to_list() == [[4.0, float(np.float32(3.7)), 88.5],
                            [2.0, 2.5, 7.75],
                            [0.0, 4.4, 99.999]]


def test_create_from_numpy_strings(np):
    src = ["alpha", "beta", "gamma"]
    DT = dt.Frame([np.str_(s) for s in src])
    assert_equals(DT, dt.Frame(src))


def test_create_from_numpy_reversed(numpy):
    DT = dt.Frame(numpy.array(range(10), dtype=numpy.int64)[::-1])
    assert_equals(DT, dt.Frame(range(9, -1, -1), stype=dt.int64))
    assert DT.to_jay()


def test_create_from_numpy_masked_and_sliced(numpy):
    arr = numpy.ma.array([1, 2, 3], mask=[False, True, False], dtype=numpy.int64)
    DT = dt.Frame(arr[::2])
    assert_equals(DT, dt.Frame([1, 3], stype=dt.int64))
    assert DT.to_jay()


def test_create_from_0size_masked_array(numpy):
    arr = numpy.ma.array([1, 2, 3], mask=[False, True, False], dtype=numpy.int64)
    DT = dt.Frame(arr[2:0])
    assert_equals(DT, dt.Frame(C0=[], stype=dt.int64))
    assert DT.to_jay()


@pytest.mark.parametrize("seed", [random.getrandbits(32) for _ in range(10)])
def test_from_random_numpy_masked_and_sliced(numpy, seed):
    random.seed(seed)
    start = random.randint(-20, 20)
    end = random.randint(-20, 20)
    step = 0
    while step == 0:
        step = random.randint(-5, 5)
    arr = numpy.arange(20, dtype=numpy.int64)
    if random.random() < 0.5:
        arr = numpy.ma.array(arr, mask=[random.random() < 0.5
                                        for _ in range(arr.shape[0])])

    arr = arr[start:end:step]
    DT = dt.Frame(arr)
    assert_equals(DT, dt.Frame(C0=arr.tolist(), stype=dt.int64))
    assert DT.to_jay()



#-------------------------------------------------------------------------------
# Create from Arrow
#-------------------------------------------------------------------------------

def test_create_from_arrow1(pa):
    df = pa.Table.from_pydict({
        "A": [3, 7, 11, 4],
        "B": [True, False, True, False],
        "C": [1.1, -2.5, 23, 0],
        "D": [2, 3, 4, -1],
        "E": ['make', 'love', 'not', 'war'],
    }, schema = pa.schema([
        pa.field("A", pa.int64()),
        pa.field("B", pa.bool_()),
        pa.field("C", pa.float64()),
        pa.field("D", pa.int8()),
        pa.field("E", pa.string())
    ]))
    assert_equals(
        dt.Frame(df),
        dt.Frame(A=[3, 7, 11, 4] / dt.int64,
                 B=[True, False, True, False],
                 C=[1.1, -2.5, 23, 0],
                 D=[2, 3, 4, -1] / dt.int8,
                 E=['make', 'love', 'not', 'war'])
    )


def test_create_from_arrow2(pa):
    df = pa.Table.from_pydict({
        "A1": [2, None, 17, -1, 3],
        "B2": ['what', 'if', None, 'munroe', None],
        "C3": [2.17, math.nan, None, -math.nan, 0.1],
        "D4": [None, None, None, True, False],
        })
    assert_equals(
        dt.Frame(df),
        dt.Frame(A1=[2, None, 17, -1, 3] / dt.int64,
                 B2=['what', 'if', None, 'munroe', None],
                 C3=[2.17, None, None, None, 0.1],
                 D4=[None, None, None, True, False])
    )


def test_create_from_arrow_null(pa):
    tbl = pa.Table.from_arrays(
            [pa.array([None] * 5)],
            names=["A"])
    assert_equals(dt.Frame(tbl),
                  dt.Frame(A = [None] * 5))


@pytest.mark.parametrize("slice_", [slice(None, None, 2), # slice(1, None),
                                    slice(None, -1), # slice(2, 5)
                                    slice(0, 3)])
def test_create_from_arrow_sliced(pa, slice_):
    src = [1, None, 2, None, 3, None, 4, 5]
    df = pa.Table.from_pydict({"A": src})
    assert_equals(dt.Frame(df[slice_]),
                  dt.Frame(A=src[slice_], stype=dt.int64))




#-------------------------------------------------------------------------------
# Issues
#-------------------------------------------------------------------------------

def test_bad():
    with pytest.raises(TypeError) as e:
        dt.Frame(1)
    assert "Cannot create Frame from <class 'int'>" == str(e.value)
    with pytest.raises(TypeError) as e:
        dt.Frame(dt)
    assert "Cannot create Frame from <class 'module'>" == str(e.value)


def test_ellipsis():
    d0 = dt.Frame(...)
    assert d0.names == ("?",)
    assert d0.shape == (1, 1)
    assert d0[0, 0] == 42
    with pytest.raises(TypeError):
        dt.Frame(..., stype=float)
    with pytest.raises(TypeError):
        dt.Frame(..., stypes=[stype.str32])
    with pytest.raises(TypeError):
        dt.Frame(..., names=["?"])


def test_issue_42():
    d = dt.Frame([-1])
    frame_integrity_check(d)
    assert d.shape == (1, 1)
    assert d.ltypes == (ltype.int, )
    d = dt.Frame([-1, 2, {5}, "hooray"], stype='obj64')
    frame_integrity_check(d)
    assert d.shape == (4, 1)
    assert d.ltypes == (ltype.obj, )


def test_issue_409():
    from math import inf, copysign
    d = dt.Frame([10**333, -10**333, 10**-333, -10**-333])
    frame_integrity_check(d)
    assert d.ltypes == (ltype.real, )
    p = d.to_list()
    assert p == [[inf, -inf, 0.0, -0.0]]
    assert copysign(1, p[0][-1]) == -1


def test_duplicate_names1():
    with pytest.warns(DatatableWarning) as ws:
        d = dt.Frame([[1], [2], [3]], names=["A", "A", "A"])
        frame_integrity_check(d)
        assert d.names == ("A", "A.0", "A.1")
    assert len(ws) == 1
    assert ("Duplicate column names found, and were assigned unique names: "
            "'A' -> 'A.0', 'A' -> 'A.1'" in ws[0].message.args[0])


def test_duplicate_names2():
    with pytest.warns(DatatableWarning):
        d = dt.Frame([[1], [2], [3], [4]], names=("A", "A.1", "A", "A.2"))
        frame_integrity_check(d)
        assert d.names == ("A", "A.1", "A.0", "A.2")


def test_duplicate_names3():
    with pytest.warns(DatatableWarning) as ws:
        d = dt.Frame([[1]] * 4, names=["A"] * 4)
        frame_integrity_check(d)
        assert d.names == ("A", "A.0", "A.1", "A.2")
    assert len(ws) == 1
    assert ("Duplicate column names found, and were assigned unique names: "
            "'A' -> 'A.0', 'A' -> 'A.1', 'A' -> 'A.2'" in ws[0].message.args[0])


def test_duplicate_names4():
    with pytest.warns(DatatableWarning) as ws:
        d = dt.Frame([[1]] * 5, names=["A"] * 5)
        frame_integrity_check(d)
        assert d.names == ("A", "A.0", "A.1", "A.2", "A.3")
    assert len(ws) == 1
    assert ("Duplicate column names found, and were assigned unique names: "
            "'A' -> 'A.0', 'A' -> 'A.1', ..., 'A' -> 'A.3'"
            in ws[0].message.args[0])


def test_special_characters_in_names():
    d = dt.Frame([[1], [2], [3], [4]],
                 names=("".join(chr(i) for i in range(32)),
                        "help\nneeded",
                        "foo\t\tbar\t \tbaz",
                        "A\n\rB\n\rC\n\rD\n\r"))
    frame_integrity_check(d)
    assert d.names == (".", "help.needed", "foo.bar. .baz", "A.B.C.D.")


def test_duplicate_mangled():
    with pytest.warns(DatatableWarning) as ws:
        DT = dt.Frame([[2]] * 5, names=["\n\n\n"] * 3 + ["\n\t2"] * 2)
        frame_integrity_check(DT)
        assert DT.names == (".", ".0", ".1", ".2", ".3")
    assert ("Duplicate column names found" in ws[0].message.args[0])
