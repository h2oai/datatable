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
import datatable as dt
import pytest
from datatable import f, g, isna, ltype
from tests import assert_equals


# Not a fixture: create a new datatable each time this function is called
def smalldt():
    res = dt.Frame([[i] for i in range(16)], names=list("ABCDEFGHIJKLMNOP"))
    assert res.ltypes == (ltype.bool, ltype.bool) + (ltype.int,) * 14
    return res



#-------------------------------------------------------------------------------
# Delete columns
#-------------------------------------------------------------------------------

def test_del_cols_all():
    d0 = smalldt()
    del d0[:, :]
    assert_equals(d0, dt.Frame())


def test_del_0cols():
    d0 = smalldt()
    del d0[:, []]
    assert_equals(d0, smalldt())


def test_del_1col_str_1():
    d0 = smalldt()
    del d0[:, "A"]
    assert_equals(d0, dt.Frame([[i] for i in range(1, 16)],
                               names=list("BCDEFGHIJKLMNOP")))


def test_del_1col_str_2():
    d0 = smalldt()
    del d0[:, "B"]
    assert_equals(d0, dt.Frame([[i] for i in range(16) if i != 1],
                               names=list("ACDEFGHIJKLMNOP")))


def test_del_1col_str_3():
    d0 = smalldt()
    del d0[:, "P"]
    assert_equals(d0, dt.Frame([[i] for i in range(16) if i != 15],
                               names=list("ABCDEFGHIJKLMNO")))


def test_del_1col_str_nonexistent():
    d0 = smalldt()
    with pytest.raises(ValueError) as e:
        del d0[:, "Z"]
    assert ("Column `Z` does not exist in the Frame" in str(e.value))


def test_del_1col_int():
    d0 = smalldt()
    del d0[:, -1]
    assert_equals(d0, dt.Frame([[i] for i in range(16) if i != 15],
                               names=list("ABCDEFGHIJKLMNO")))


def test_del_1col_expr():
    d0 = smalldt()
    del d0[:, f.E]
    assert_equals(d0, dt.Frame([[i] for i in range(16) if i != 4],
                               names=list("ABCDFGHIJKLMNOP")))


def test_del_cols_strslice():
    d0 = smalldt()
    del d0[:, "E":"K"]
    d1 = dt.Frame([[0], [1], [2], [3], [11], [12], [13], [14], [15]],
                  names=list("ABCDLMNOP"))
    assert_equals(d0, d1)


def test_del_cols_intslice1():
    d0 = smalldt()
    del d0[:, ::2]
    d1 = dt.Frame([[i] for i in range(1, 16, 2)], names=list("BDFHJLNP"))
    assert_equals(d0, d1)


def test_del_cols_intslice2():
    d0 = smalldt()
    del d0[:, ::-2]
    d1 = dt.Frame([[i] for i in range(0, 16, 2)], names=list("ACEGIKMO"))
    assert_equals(d0, d1)


def test_del_cols_intlist():
    d0 = smalldt()
    del d0[:, [0, 3, 0, 5, 0, 9]]
    d1 = dt.Frame([[1], [2], [4], [6], [7], [8], [10], [11], [12], [13],
                   [14], [15]], names=list("BCEGHIKLMNOP"))
    assert_equals(d0, d1)


def test_del_cols_strlist():
    d0 = smalldt()
    del d0[:, ["A", "B", "C"]]
    d1 = dt.Frame([[i] for i in range(3, 16)], names=list("DEFGHIJKLMNOP"))
    assert_equals(d0, d1)


def test_del_cols_boollist():
    d0 = smalldt()
    del d0[:, [i % 3 == 1 for i in range(16)]]
    d1 = smalldt()[:, [i % 3 != 1 for i in range(16)]]
    assert_equals(d0, d1)


def test_del_cols_multislice():
    d0 = smalldt()
    del d0[:, [slice(10), 12, -1]]
    d1 = dt.Frame([[10], [11], [13], [14]], names=list("KLNO"))
    assert_equals(d0, d1)


def test_del_cols_generator():
    d0 = smalldt()
    del d0[:, (i**2 for i in range(4))]
    d1 = dt.Frame([[i] for i in range(16) if i not in [0, 1, 4, 9]],
                  names=list("CDFGHIKLMNOP"))
    assert_equals(d0, d1)


def test_del_cols_exprlist():
    d0 = smalldt()
    del d0[:, (f.A, f.B, f.C, f.D)]
    d1 = smalldt()[:, 4:]
    assert_equals(d0, d1)


def test_delcols_invalid_selector1():
    d0 = smalldt()
    with pytest.raises(TypeError) as e:
        del d0[:, 0.5]
    assert "Unsupported `j` selector of type <class 'float'>" in str(e.value)


def test_del_cols_invalid_selector2():
    d0 = smalldt()
    with pytest.raises(TypeError) as e:
        del d0[:, d0]
    assert ("Unsupported `j` selector of type <class 'datatable.Frame'>"
            in str(e.value))


def test_del_cols_invalid_selector3():
    d0 = smalldt()
    with pytest.raises(TypeError) as e:
        del d0[:, [1, 2, 1, 0.7]]
    assert ("Element 3 in the `j` selector list has type `<class 'float'>`, "
            "which is not supported" in str(e.value))


def test_del_cols_computed1():
    d0 = smalldt()
    with pytest.raises(TypeError) as e:
        del d0[:, f.A + f.B]
    assert ("Item 0 in the `j` selector list is a computed expression and "
            "cannot be deleted" == str(e.value))


def test_del_cols_computed2():
    d0 = smalldt()
    with pytest.raises(TypeError) as e:
        del d0[:, [f.A, f.B, f.A + f.B]]
    assert ("Item 2 in the `j` selector list is a computed expression and "
            "cannot be deleted" == str(e.value))


def test_del_cols_dict():
    d0 = smalldt()
    with pytest.raises(TypeError) as e:
        del d0[:, {"X": f.A}]
    assert ("When del operator is applied, the `j` selector cannot be a "
            "dictionary" in str(e.value))


def test_del_cols_g1():
    d0 = smalldt()
    with pytest.raises(ValueError) as e:
        del d0[:, g[1]]
    assert ("Item 0 of the `j` selector list references a non-existing join "
            "frame" == str(e.value))


# def test_del_cols_g2():
#     d0 = dt.Frame(A=range(10), G=[True, False] * 5)
#     d1 = dt.Frame(A=[1, 5, 7], B=["moo", None, "kewt"])
#     d1.key = "A"
#     del d0[:, [f.G, g.B], dt.join(d1)]




#-------------------------------------------------------------------------------
# Delete rows
#-------------------------------------------------------------------------------

def test_del_rows_all():
    d0 = dt.Frame(range(10))
    del d0[:, :]
    d0.internal.check()
    assert d0.shape == (0, 0)


def test_del_rows_single():
    d0 = dt.Frame(range(10))
    del d0[3, :]
    assert_equals(d0, dt.Frame([0, 1, 2, 4, 5, 6, 7, 8, 9], stype="int32"))


def test_del_rows_slice0():
    d0 = dt.Frame(N=range(10))
    del d0[:3, :]
    assert_equals(d0, dt.Frame(N=range(3, 10)))


def test_del_rows_slice1():
    d0 = dt.Frame(range(10))
    del d0[-4:, :]
    assert_equals(d0, dt.Frame(range(6)))


def test_del_rows_slice2():
    d0 = dt.Frame(range(10))
    del d0[2:6, :]
    assert_equals(d0, dt.Frame([0, 1, 6, 7, 8, 9], stype="int32"))


def test_del_rows_slice_empty():
    d0 = dt.Frame(range(10))
    del d0[4:4, :]
    assert_equals(d0, dt.Frame(range(10)))


def test_del_rows_slice_reverse():
    d0 = dt.Frame(range(10))
    s0 = list(range(10))
    del d0[:4:-1, :]
    del s0[:4:-1]
    d0.internal.check()
    assert d0.to_list() == [s0]


def test_del_rows_slice_all():
    d0 = dt.Frame(range(10))
    del d0[::-1, :]
    d0.internal.check()
    assert d0.shape == (0, 1)


def test_del_rows_slice_step():
    d0 = dt.Frame(range(10))
    del d0[::3, :]
    d0.internal.check()
    assert d0.to_list() == [[1, 2, 4, 5, 7, 8]]


def test_del_rows_array():
    d0 = dt.Frame(range(10))
    del d0[[0, 7, 8], :]
    d0.internal.check()
    assert d0.to_list() == [[1, 2, 3, 4, 5, 6, 9]]


@pytest.mark.skip("Issue #805")
def test_del_rows_array_unordered():
    d0 = dt.Frame(range(10))
    del d0[[3, 1, 5, 2, 2, 0, -1], :]
    d0.internal.check()
    assert d0.to_list() == [[4, 6, 7, 8]]


def test_del_rows_filter():
    d0 = dt.Frame(range(10), names=["A"], stype="int32")
    del d0[f.A < 4, :]
    d0.internal.check()
    assert d0.to_list() == [[4, 5, 6, 7, 8, 9]]


def test_del_rows_nas():
    d0 = dt.Frame({"A": [1, 5, None, 12, 7, None, -3]})
    del d0[isna(f.A), :]
    d0.internal.check()
    assert d0.to_list() == [[1, 5, 12, 7, -3]]


def test_del_rows_from_view1():
    d0 = dt.Frame(range(10))
    d1 = d0[::2, :]  # 0 2 4 6 8
    del d1[3, :]
    assert d1.to_list() == [[0, 2, 4, 8]]
    del d1[[0, 3], :]
    assert d1.to_list() == [[2, 4]]


def test_del_rows_from_view2():
    f0 = dt.Frame([1, 3, None, 4, 5, None, None, 2, None, None, None])
    f1 = f0[5:, :]
    del f1[isna(f[0]), :]
    assert f1.to_list() == [[2]]



#-------------------------------------------------------------------------------
# Delete rows & columns
#-------------------------------------------------------------------------------
