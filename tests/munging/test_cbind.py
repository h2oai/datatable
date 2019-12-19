#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import pytest
import types
import datatable as dt
from tests import assert_equals
from datatable import stype, DatatableWarning
from datatable.internal import frame_integrity_check, frame_columns_virtual


def dt_compute_stats(*dts):
    """
    Force computing all Stats on all frames.

    Currently, computing a single stat causes all other stats to be computed
    as well.
    """
    for d in dts:
        d.min()


#-------------------------------------------------------------------------------
# cbind tests
#-------------------------------------------------------------------------------

def test_cbind_exists():
    dt0 = dt.Frame([1, 2, 3])
    assert isinstance(dt0.cbind, types.BuiltinMethodType)
    assert len(dt0.cbind.__doc__) > 1400


def test_cbind_simple():
    d0 = dt.Frame([1, 2, 3])
    d1 = dt.Frame([4, 5, 6])
    dt_compute_stats(d0, d1)
    with pytest.warns(DatatableWarning):
        d0.cbind(d1)
    dr = dt.Frame([[1, 2, 3], [4, 5, 6]], names=["C0", "C1"])
    assert_equals(d0, dr)


def test_cbind_api():
    DT1 = dt.Frame(A=[1, 2, 3])
    DT2 = dt.Frame(B=[-4, -5, None])
    DT3 = dt.Frame(X=["makes", "gonna", "make"])
    RES1 = dt.cbind(DT1, DT2, DT3)
    RES2 = dt.cbind([DT1, DT2, DT3])
    RES3 = dt.cbind((DT1, DT2, DT3))  # tuple
    RES4 = dt.cbind([DT1], [DT2, DT3])
    RES5 = dt.cbind(DT1, [DT2], DT3)
    RES6 = dt.cbind((frame for frame in [DT1, DT2, DT3]))  # generator
    assert_equals(RES1, RES2)
    assert_equals(RES1, RES3)
    assert_equals(RES1, RES4)
    assert_equals(RES1, RES5)
    assert_equals(RES1, RES6)


def test_cbind_empty1():
    d0 = dt.Frame([1, 2, 3])
    dt_compute_stats(d0)
    d0.cbind()
    assert_equals(d0, dt.Frame([1, 2, 3]))


def test_cbind_empty2():
    d0 = dt.Frame([1, 2, 3])
    dt_compute_stats(d0)
    d0.cbind(dt.Frame())
    assert_equals(d0, dt.Frame([1, 2, 3]))


def test_cbind_empty3():
    DT = dt.cbind()
    assert_equals(DT, dt.Frame())


def test_cbind_self():
    d0 = dt.Frame({"fun": [1, 2, 3]})
    dt_compute_stats(d0)
    with pytest.warns(DatatableWarning):
        d0.cbind(d0)
        d0.cbind(d0)
        d0.cbind(d0)
    dr = dt.Frame([[1, 2, 3]] * 8,
                  names=["fun"] + ["fun.%d" % i for i in range(7)])
    assert_equals(d0, dr)


def test_cbind_notinplace():
    d0 = dt.Frame({"A": [1, 2, 3]})
    d1 = dt.Frame({"B": [4, 5, 6]})
    dt_compute_stats(d0, d1)
    dd = dt.cbind(d0, d1)
    dr = dt.Frame({"A": [1, 2, 3], "B": [4, 5, 6]})
    assert_equals(dd, dr)
    assert_equals(d0, dt.Frame({"A": [1, 2, 3]}))
    assert_equals(d1, dt.Frame({"B": [4, 5, 6]}))


def test_cbind_notforced():
    d0 = dt.Frame([1, 2, 3])
    d1 = dt.Frame([4, 5])
    with pytest.raises(ValueError) as e:
        d0.cbind(d1)
    assert ("Cannot cbind frame with 2 rows to a frame with 3 rows"
            in str(e.value))


def test_cbind_forced1():
    d0 = dt.Frame([1, 2, 3])
    d1 = dt.Frame([4, 5])
    dt_compute_stats(d0, d1)
    with pytest.warns(DatatableWarning):
        d0.cbind(d1, force=True)
    dr = dt.Frame([[1, 2, 3], [4, 5, None]], names=["C0", "C1"])
    assert_equals(d0, dr)


def test_cbind_forced2():
    d0 = dt.Frame({"A": [1, 2, 3], "B": [7, None, -1]})
    d1 = dt.Frame({"C": list("abcdefghij")})
    dt_compute_stats(d0, d1)
    d0.cbind(d1, force=True)
    dr = dt.Frame({"A": [1, 2, 3] + [None] * 7,
                   "B": [7, None, -1] + [None] * 7,
                   "C": list("abcdefghij")})
    assert_equals(d0, dr)


def test_cbind_forced3():
    d0 = dt.Frame({"A": range(10)})
    d1 = dt.Frame({"B": ["one", "two", "three"]})
    dt_compute_stats(d0, d1)
    d0.cbind(d1, force=True)
    dr = dt.Frame({"A": range(10),
                   "B": ["one", "two", "three"] + [None] * 7})
    assert_equals(d0, dr)


def test_cbind_onerow1():
    d0 = dt.Frame({"A": [1, 2, 3, 4, 5]})
    d1 = dt.Frame({"B": [100.0]})
    dt_compute_stats(d0, d1)
    d0.cbind(d1)
    dr = dt.Frame({"A": [1, 2, 3, 4, 5], "B": [100.0] * 5})
    assert_equals(d0, dr)


def test_cbind_onerow2():
    d0 = dt.Frame({"A": ["mu"]})
    d1 = dt.Frame({"B": [7, 9, 10, 15]})
    dt_compute_stats(d0, d1)
    d0.cbind(d1)
    dr = dt.Frame({"A": ["mu"] * 4, "B": [7, 9, 10, 15]})
    assert_equals(d0, dr)


def test_bad_arguments():
    d0 = dt.Frame([1, 2, 3])
    d1 = dt.Frame([5])
    with pytest.raises(TypeError):
        d0.cbind(100)
    with pytest.raises(TypeError):
        d0.cbind(d1, inplace=3)
    with pytest.raises(TypeError):
        d0.cbind(d1, force=None)


def test_cbind_views1():
    d0 = dt.Frame({"A": range(100)})
    d1 = d0[:5, :]
    d2 = dt.Frame({"B": [3, 6, 9, 12, 15]}, stype=stype.int32)
    d1.cbind(d2)
    dr = dt.Frame({"A": range(5), "B": range(3, 18, 3)})
    assert_equals(d1, dr)


def test_cbind_views2():
    d0 = dt.Frame({"A": range(10)}, stype=stype.int8)
    d1 = d0[2:5, :]
    d2 = dt.Frame({"B": list("abcdefghij")})
    d3 = d2[-3:, :]
    d1.cbind(d3)
    dr = dt.Frame({"A": [2, 3, 4], "B": ["h", "i", "j"]}, stypes = {"A":"int8"})
    assert_equals(d1, dr)


def test_cbind_views3():
    d0 = dt.Frame(A=range(10))[::-1, :]
    d1 = dt.Frame(B=list("abcde") * 2)
    d2 = dt.Frame(C=range(1000))[[14, 19, 35, 17, 3, 0, 1, 0, 10, 777], :]
    d0.cbind([d1, d2])
    assert d0.to_list() == [list(range(10))[::-1],
                            list("abcde" * 2),
                            [14, 19, 35, 17, 3, 0, 1, 0, 10, 777]]
    assert frame_columns_virtual(d0) == (True, False, True)



def test_cbind_multiple():
    d0 = dt.Frame({"A": [1, 2, 3]})
    d1 = dt.Frame({"B": ["doo"]})
    d2 = dt.Frame({"C": [True, False]})
    d3 = dt.Frame({"D": [10, 9, 8, 7]})
    d4 = dt.Frame({"E": [1]})[:0, :]
    dt_compute_stats(d0, d1, d2, d3, d4)
    d0.cbind(d1, d2, d3, d4, force=True)
    dr = dt.Frame(A=[1, 2, 3, None],
                  B=["doo", "doo", "doo", "doo"],
                  C=[True, False, None, None],
                  D=[10, 9, 8, 7],
                  E=[None] * 4,
                  stypes={"E": dt.int8})
    assert_equals(d0, dr)


def test_cbind_1row_none():
    # Special case: append a single-row string column containing value NA
    d0 = dt.Frame({"A": [1, 2, 3]})
    d1 = dt.Frame({"B": [None, "doo"]})[0, :]
    dt_compute_stats(d0, d1)
    assert d1.shape == (1, 1)
    assert d1.stypes == (stype.str32, )
    d0.cbind(d1)
    dr = dt.Frame({"A": [1, 2, 3, 4], "B": [None, None, None, "f"]})[:-1, :]
    assert_equals(d0, dr)


def test_cbind_method():
    d0 = dt.Frame({"A": [1, 2, 3]})
    d1 = dt.Frame({"B": list('abc')})
    d2 = dt.Frame({"C": [5.6, 7.1, -3.3]})
    dr = dt.cbind(d0, d1, d2)
    assert dr.names == ("A", "B", "C")
    res = dt.Frame([[1, 2, 3], ["a", "b", "c"], [5.6, 7.1, -3.3]],
                   names=("A", "B", "C"))
    assert_equals(dr, res)


def test_cbind_array():
    d0 = dt.Frame(A=range(5))
    with pytest.warns(DatatableWarning):
        d0.cbind([d0] * 10)
    frame_integrity_check(d0)
    assert d0.shape == (5, 11)
    assert d0.names == ("A",) + tuple("A.%d" % i for i in range(10))
    assert d0.to_list() == [[0, 1, 2, 3, 4]] * 11


def test_cbind_bad_things():
    d0 = dt.Frame(A=range(5))
    with pytest.raises(TypeError) as e:
        d0.cbind(12)
    assert ("Frame.cbind() expects a list or sequence of Frames, but got an "
            "argument of type <class 'int'>" == str(e.value))
    with pytest.raises(TypeError) as e:
        d0.cbind([d0, d0, 0])
    assert ("Frame.cbind() expects a list or sequence of Frames, but got an "
            "argument of type <class 'int'>" == str(e.value))


def test_cbind_correct_stypes():
    # Issue 1287: check that if stypes / ltypes were queried before doing cbind,
    # then they are updated correctly after the cbind.
    f0 = dt.Frame(range(5))
    s1 = f0.stypes   # cause stypes to be computed and cached
    l1 = f0.ltypes
    with pytest.warns(DatatableWarning):
        f0.cbind(f0, f0)
    s2 = f0.stypes
    l2 = f0.ltypes
    assert s2 == s1 * 3
    assert l2 == l1 * 3


def test_cbind_0rows_1():
    """Issue #1604."""
    res = dt.cbind(dt.Frame(A=[]), dt.Frame(B=[]))
    assert res.names == ("A", "B")
    assert res.shape == (0, 2)


def test_cbind_0rows_2():
    DT = dt.Frame(A=[])
    DT.cbind([dt.Frame(B=[]), dt.Frame(T=[])])
    assert DT.names == ("A", "B", "T")
    assert DT.shape == (0, 3)


def test_cbind_0rows_3():
    DT0 = dt.Frame(A=[], B=[], C=[])
    RES1 = dt.cbind(dt.Frame(), DT0)
    RES2 = dt.cbind(DT0, dt.Frame())
    assert_equals(RES1, DT0)
    assert_equals(RES2, DT0)


def test_cbind_error_1():
    DT = dt.Frame(A=[1, 5])
    with pytest.raises(ValueError) as e:
        DT.cbind(dt.Frame(B=[]))
    assert ("Cannot cbind frame with 0 rows to a frame with 2 rows"
            in str(e.value))


def test_cbind_error_2():
    DT = dt.Frame(A=[])
    with pytest.raises(ValueError) as e:
        DT.cbind(dt.Frame(B=[1, 5]))
    assert ("Cannot cbind frame with 2 rows to a frame with 0 rows"
            in str(e.value))


def test_cbind_error_3():
    DT = dt.Frame()
    DT.nrows = 5
    assert DT.shape == (5, 0)
    with pytest.raises(ValueError) as e:
        DT.cbind(dt.Frame(B=[]))
    assert ("Cannot cbind frame with 0 rows to a frame with 5 rows"
            in str(e.value))


def test_issue1905():
    # cbind() is passed a generator, where each generated Frame is a
    # temporary. In this case cbind() should take care to keep the
    # references to all frames while working, lest they get gc-d by
    # the end of the generator loop.
    DT = dt.cbind(dt.Frame(range(50), names=[str(i)])
                  for i in range(30))
    assert DT.shape == (50, 30)


def test_cbind_nones():
    DT = dt.cbind(None, dt.Frame(A=range(5)), None, dt.Frame(B=[0]*5))
    assert_equals(DT, dt.Frame(A=range(5), B=[0]*5))


def test_cbind_expanded_frame():
    DT = dt.Frame(A=[1, 2], B=['a', "E"], C=[7, 1000], D=[-3.14, 159265])
    RES = dt.cbind(*DT)
    assert_equals(DT, RES)


def test_cbind_issue2024():
    DT = dt.Frame([[]] * 2, names=["A.1", "A.5"])
    with pytest.warns(DatatableWarning):
        RZ = dt.cbind(DT, DT)
        assert RZ.names == ("A.1", "A.5", "A.2", "A.6")
        RZ = dt.cbind(DT, DT, DT)
        assert RZ.names == ("A.1", "A.5", "A.2", "A.6", "A.3", "A.7")
        RZ = dt.cbind(DT, DT, DT, DT)
        assert RZ.names == ("A.1", "A.5", "A.2", "A.6", "A.3", "A.7", "A.4",
                            "A.8")
        RZ = dt.cbind(DT, DT, DT, DT, DT)
        assert RZ.names == ("A.1", "A.5", "A.2", "A.6", "A.3", "A.7", "A.4",
                            "A.8", "A.9", "A.10")


def test_cbind_issue2028():
    def join(names1, names2):
        with pytest.warns(DatatableWarning):
            return dt.cbind(dt.Frame(names=names1),
                            dt.Frame(names=names2)).names

    assert join(["A", "A.1"], ["A", "A.1"]) == ("A", "A.1", "A.0", "A.2")
    assert join(["A", "A.1"], ["A.1", "A"]) == ("A", "A.1", "A.2", "A.0")
    assert join(["B"], ["B.0", "B.00", "B"]) == ("B", "B.0", "B.00", "B.1")
    with dt.options.frame.context(names_auto_index=17):
        assert join(["A", "A.1"], ["A", "A.1"]) == ("A", "A.1", "A.17", "A.2")
