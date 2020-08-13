#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2020 H2O.ai
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
from datatable import f


def test_colindex():
    DT = dt.Frame(names=list("ABCDEFG"))
    assert DT.colindex(0) == 0
    assert DT.colindex(1) == 1
    assert DT.colindex(-1) == 6
    for i, ch in enumerate("ABCDEFG"):
        assert DT.colindex(i) == i
        assert DT.colindex(ch) == i


def test_colindex_f():
    DT = dt.Frame(names=list("ABCD"))
    assert DT.colindex(f.A) == 0
    assert DT.colindex(f.D) == 3
    assert DT.colindex(f["B"]) == 1
    assert DT.colindex(f[1]) == 1
    assert DT.colindex(f[-1]) == 3


def test_name_doesnt_exist():
    DT = dt.Frame(range(5))
    msg = "Column a does not exist in the Frame"
    with pytest.raises(KeyError, match=msg):
        DT.colindex("a")
    with pytest.raises(KeyError, match=msg):
        DT.colindex(f.a)


def test_index_too_large():
    DT = dt.Frame([[3]] * 7)
    msg = "Column index 7 is invalid for a frame with 7 columns"
    with pytest.raises(IndexError, match=msg):
        DT.colindex(7)


def test_index_too_negative():
    DT = dt.Frame([[3]] * 7)
    msg = "Column index -8 is invalid for a frame with 7 columns"
    with pytest.raises(IndexError, match=msg):
        DT.colindex(-8)


def test_colindex_no_args():
    DT = dt.Frame()
    msg = r"Frame\.colindex\(\) is missing the required positional argument " \
          "column"
    with pytest.raises(TypeError, match=msg):
        DT.colindex()


def test_colindex_too_many_args():
    DT = dt.Frame()
    msg = r"Frame\.colindex\(\) takes only one positional argument, " \
          "but 2 were given"
    with pytest.raises(TypeError, match=msg):
        DT.colindex(0, 1)


def test_colindex_named_arg():
    DT = dt.Frame(A=[0])
    msg = r"Frame\.colindex\(\) got argument column as a keyword, but it " \
          r"should be positional-only"
    with pytest.raises(TypeError, match=msg):
        DT.colindex(column="A")



@pytest.mark.parametrize("x", [False, None, 1.99, [1, 2, 3], -f.x])
def test_arg_wrong_type(x):
    DT = dt.Frame(names=list("ABCDEFG"))
    msg = r"The argument to Frame\.colindex\(\) should be a string or an " \
          r"integer, not %s" % type(x)
    with pytest.raises(TypeError, match=msg):
        DT.colindex(x)


def test_colindex_fuzzy_suggestions():
    def check(DT, name, suggestions):
        with pytest.raises(KeyError) as e:
            DT.colindex(name)
        assert str(e.value).endswith(suggestions)

    d0 = dt.Frame([[0]] * 3, names=["foo", "bar", "baz"])
    check(d0, "fo", "; did you mean foo?")
    check(d0, "foe", "; did you mean foo?")
    check(d0, "fooo", "; did you mean foo?")
    check(d0, "ba", "; did you mean bar or baz?")
    check(d0, "barb", "; did you mean bar or baz?")
    check(d0, "bazb", "; did you mean baz or bar?")
    check(d0, "ababa", "Frame")
    d1 = dt.Frame([[0]] * 50)
    check(d1, "A", "Frame")
    check(d1, "C", "; did you mean C0, C1 or C2?")
    check(d1, "c1", "; did you mean C1, C0 or C2?")
    check(d1, "C 1", "; did you mean C1, C11 or C21?")
    check(d1, f.V0, "; did you mean C0?")
    check(d1, "Va", "Frame")
    d2 = dt.Frame(varname=[1])
    check(d2, "vraname", "; did you mean varname?")
    check(d2, "VRANAME", "; did you mean varname?")
    check(d2, "var_name", "; did you mean varname?")
    check(d2, "variable", "; did you mean varname?")



def test_colindex_after_column_deleted():
    DT = dt.Frame(names=["A", "B", "C", "D"])
    assert DT.colindex("D") == 3
    del DT["B"]
    assert DT.colindex("D") == 2
    del DT["D"]
    msg = "Column D does not exist in the Frame"
    with pytest.raises(KeyError, match=msg):
        DT.colindex("D")


def test_keyerror():
    import pickle
    import traceback
    try:
        dt.Frame()["A"]
        assert False
    except KeyError as e:
        obj = pickle.dumps(e)
    ee = pickle.loads(obj)
    assert isinstance(ee, KeyError)
    assert str(ee) == "Column A does not exist in the Frame"
    # Check that there are no extra quotes
    assert traceback.format_exception_only(type(ee), ee) == \
        ["datatable.exceptions.KeyError: Column A does not exist in the Frame\n"]


def test_columns_with_backticks():
    DT = dt.Frame(names=["`abc", "a`bc", "`abc`", "\\", "\\`"])
    msg = "Column abc does not exist in the Frame; did you mean "\
          "`abc or a`bc?"
    with pytest.raises(KeyError, match=msg):
        DT.colindex("abc")

    msg = "Column abc` does not exist in the Frame; did you mean "\
          "`abc`, `abc or a`bc?"
    with pytest.raises(KeyError, match=msg):
        DT.colindex("abc`")

    try:
        DT.colindex("\\a")
    except KeyError as e:
        assert str(e) == r"Column \a does not exist in the Frame; "\
                         r"did you mean \ or \`?"
