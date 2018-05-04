#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import pytest
import sys
import datatable as dt
from datatable import stype, ltype, f, isna
from tests import same_iterables, list_equals


#-------------------------------------------------------------------------------
# Prepare fixtures & helper functions
#-------------------------------------------------------------------------------

@pytest.fixture()
def dt0():
    return dt.Frame([
        [2, 7, 0, 0],
        [True, False, False, True],
        [1, 1, 1, 1],
        [0.1, 2, -4, 4.4],
        [None, None, None, None],
        [0, 0, 0, 0],
        ["1", "2", "hello", "world"],
    ], names=list("ABCDEFG"))


@pytest.fixture()
def patched_terminal(monkeypatch):
    def send1(refresh_rate=1):
        yield None
    monkeypatch.setattr("datatable.utils.terminal.wait_for_keypresses", send1)
    dt.utils.terminal.term.override_width = 100
    dt.utils.terminal.term.disable_styling()


def assert_valueerror(datatable, rows, error_message):
    with pytest.raises(ValueError) as e:
        datatable(rows=rows)
    assert str(e.type) == "<class 'dt.ValueError'>"
    assert error_message in str(e.value)

def as_list(datatable):
    nrows, ncols = datatable.shape
    return datatable.internal.window(0, nrows, 0, ncols).data



#-------------------------------------------------------------------------------
# Run the tests
#-------------------------------------------------------------------------------

@pytest.mark.run(order=1)
def test_dt_properties(dt0):
    assert isinstance(dt0, dt.Frame)
    assert dt0.internal.check()
    assert dt0.nrows == 4
    assert dt0.ncols == 7
    assert dt0.shape == (4, 7)
    assert dt0.names == ("A", "B", "C", "D", "E", "F", "G")
    assert dt0.ltypes == (ltype.int, ltype.bool, ltype.bool, ltype.real,
                          ltype.bool, ltype.bool, ltype.str)
    assert dt0.stypes == (stype.int8, stype.bool8, stype.bool8, stype.float64,
                          stype.bool8, stype.bool8, stype.str32)
    assert dt0.internal.alloc_size > 500
    assert (sys.getsizeof(dt0) > dt0.internal.alloc_size +
            sys.getsizeof(dt0.names) +
            sum(sys.getsizeof(colname) for colname in dt0.names) +
            sys.getsizeof(dt0._inames) + sys.getsizeof(dt0._ltypes) +
            sys.getsizeof(dt0._stypes))


@pytest.mark.run(order=2)
def test_dt_call(dt0, capsys):
    # assert dt0.internal.column(0).refcount == 1
    dt1 = dt0(timeit=True)
    assert dt1.shape == dt0.shape
    assert not dt1.internal.isview
    # assert dt0.internal.column(0).refcount == 2
    out, err = capsys.readouterr()
    assert err == ""
    assert "Time taken:" in out


@pytest.mark.run(order=3)
def test_dt_view(dt0, patched_terminal, capsys):
    dt0.view()
    out, err = capsys.readouterr()
    assert ("      A   B   C     D   E   F  G    \n"
            "---  --  --  --  ----  --  --  -----\n"
            " 0    2   1   1   0.1       0  1    \n"
            " 1    7   0   1   2         0  2    \n"
            " 2    0   0   1  -4         0  hello\n"
            " 3    0   1   1   4.4       0  world\n"
            "\n"
            "[4 rows x 7 columns]\n"
            "Press q to quit  ↑←↓→ to move  wasd to page  t to toggle types"
            in out)


@pytest.mark.run(order=4)
def test_dt_colindex(dt0):
    assert dt0.colindex(0) == 0
    assert dt0.colindex(1) == 1
    assert dt0.colindex(-1) == 6
    for i, ch in enumerate("ABCDEFG"):
        assert dt0.colindex(ch) == i
    with pytest.raises(ValueError) as e:
        dt0.colindex("a")
    assert "Column `a` does not exist" in str(e.value)
    with pytest.raises(ValueError) as e:
        dt0.colindex(7)
    assert "Column index `7` is invalid for a datatable with" in str(e.value)
    with pytest.raises(ValueError) as e:
        dt0.colindex(-8)
    assert "Column index `-8` is invalid for a datatable with" in str(e.value)


@pytest.mark.run(order=5)
def test_dt_getitem(dt0):
    dt1 = dt0[0]
    assert dt1.shape == (4, 1)
    assert dt1.names == ("A", )
    dt1 = dt0[(4,)]
    assert dt1.shape == (4, 1)
    assert dt1.names == ("E", )
    dt2 = dt0[0, 1]
    assert dt2.shape == (1, 1)
    assert dt2.names == ("B", )
    with pytest.raises(ValueError) as e:
        dt0[0, 1, 2, 3]
    assert "Selector (0, 1, 2, 3) is not supported" in str(e.value)



#-------------------------------------------------------------------------------
# Delete columns
#-------------------------------------------------------------------------------

# Not a fixture: create a new datatable each time this function is called
def smalldt():
    return dt.Frame([[i] for i in range(16)], names="ABCDEFGHIJKLMNOP")

def test_del_0cols():
    d0 = smalldt()
    del d0[[]]
    assert d0.internal.check()
    assert d0.shape == (1, 16)
    assert d0.topython() == smalldt().topython()

def test_del_1col_str_1():
    d0 = smalldt()
    del d0["A"]
    assert d0.internal.check()
    assert d0.shape == (1, 15)
    assert d0.topython() == [[i] for i in range(1, 16)]
    assert d0.names == tuple("BCDEFGHIJKLMNOP")

def test_del_1col_str_2():
    d0 = smalldt()
    del d0["B"]
    assert d0.internal.check()
    assert d0.shape == (1, 15)
    assert d0.topython() == [[i] for i in range(16) if i != 1]
    assert d0.names == tuple("ACDEFGHIJKLMNOP")

def test_del_1col_str_3():
    d0 = smalldt()
    del d0["P"]
    assert d0.internal.check()
    assert d0.shape == (1, 15)
    assert d0.topython() == [[i] for i in range(16) if i != 15]
    assert d0.names == tuple("ABCDEFGHIJKLMNO")

def test_del_1col_int():
    d0 = smalldt()
    del d0[-1]
    assert d0.internal.check()
    assert d0.shape == (1, 15)
    assert d0.names == tuple("ABCDEFGHIJKLMNO")

def test_del_cols_strslice():
    d0 = smalldt()
    del d0["E":"K"]
    assert d0.internal.check()
    assert d0.shape == (1, 9)
    assert d0.names == tuple("ABCDLMNOP")
    assert d0.topython() == [[0], [1], [2], [3], [11], [12], [13], [14], [15]]

def test_del_cols_intslice1():
    d0 = smalldt()
    del d0[::2]
    assert d0.internal.check()
    assert d0.shape == (1, 8)
    assert d0.names == tuple("BDFHJLNP")
    assert as_list(d0) == [[i] for i in range(1, 16, 2)]

def test_del_cols_intslice2():
    d0 = smalldt()
    del d0[::-2]
    assert d0.internal.check()
    assert d0.shape == (1, 8)
    assert d0.names == tuple("ACEGIKMO")
    assert as_list(d0) == [[i] for i in range(0, 16, 2)]

def test_del_cols_all():
    d0 = smalldt()
    del d0[:]
    assert d0.internal.check()
    assert d0.names == tuple()
    assert d0.shape == (0, 0)

def test_del_cols_list():
    d0 = smalldt()
    del d0[[0, 3, 0, 5, 0, 9]]
    assert d0.internal.check()
    assert d0.names == tuple("BCEGHIKLMNOP")
    assert d0.shape == (1, 12)

def test_del_cols_multislice():
    d0 = smalldt()
    del d0[[slice(10), 12, -1]]
    assert d0.internal.check()
    assert d0.names == tuple("KLNO")
    assert d0.shape == (1, 4)

def test_del_cols_generator():
    d0 = smalldt()
    del d0[(i**2 for i in range(4))]
    assert d0.internal.check()
    assert d0.names == tuple("CDFGHIKLMNOP")

def test_del_cols_with_allrows():
    d0 = smalldt()
    del d0[:, ["A", "B", "C"]]
    assert d0.internal.check()
    assert d0.shape == (1, 13)
    assert d0.names == tuple("DEFGHIJKLMNOP")

def test_delitem_invalid_selectors():
    """
    Test deleting columns from a datatable.
    Note: don't use dt0 here, because this test will modify it, potentially
    invalidating other tests.
    """
    d0 = smalldt()
    with pytest.raises(ValueError) as e:
        del d0[0.5]
    assert "Unknown `select` argument: 0.5" in str(e.value)
    with pytest.raises(ValueError):
        del d0[d0]
    with pytest.raises(TypeError):
        del d0[[1, 2, 1, 0.7]]



#-------------------------------------------------------------------------------
# Delete rows
#-------------------------------------------------------------------------------

def test_del_rows_single():
    d0 = dt.Frame(range(10))
    del d0[3, :]
    assert d0.internal.check()
    assert d0.topython() == [[0, 1, 2, 4, 5, 6, 7, 8, 9]]


def test_del_rows_slice0():
    d0 = dt.Frame(range(10))
    del d0[:3, :]
    assert d0.internal.check()
    assert d0.shape == (7, 1)
    assert d0.topython() == [list(range(3, 10))]


def test_del_rows_slice1():
    d0 = dt.Frame(range(10))
    del d0[-4:, :]
    assert d0.internal.check()
    assert d0.shape == (6, 1)
    assert d0.topython() == [list(range(10 - 4))]


def test_del_rows_slice2():
    d0 = dt.Frame(range(10))
    del d0[2:6, :]
    assert d0.internal.check()
    assert d0.shape == (6, 1)
    assert d0.topython() == [[0, 1, 6, 7, 8, 9]]


def test_del_rows_slice_empty():
    d0 = dt.Frame(range(10))
    del d0[4:4, :]
    assert d0.internal.check()
    assert d0.shape == (10, 1)
    assert d0.topython() == [list(range(10))]


def test_del_rows_slice_reverse():
    d0 = dt.Frame(range(10))
    s0 = list(range(10))
    del d0[:4:-1, :]
    del s0[:4:-1]
    assert d0.internal.check()
    assert d0.topython() == [s0]


def test_del_rows_slice_all():
    d0 = dt.Frame(range(10))
    del d0[::-1, :]
    assert d0.internal.check()
    assert d0.shape == (0, 1)


def test_del_all():
    d0 = dt.Frame(range(10))
    del d0[:, :]
    assert d0.internal.check()
    assert d0.shape == (0, 0)


def test_del_rows_slice_step():
    d0 = dt.Frame(range(10))
    del d0[::3, :]
    assert d0.internal.check()
    assert d0.topython() == [[1, 2, 4, 5, 7, 8]]


def test_del_rows_array():
    d0 = dt.Frame(range(10))
    del d0[[0, 7, 8], :]
    assert d0.internal.check()
    assert d0.topython() == [[1, 2, 3, 4, 5, 6, 9]]


@pytest.mark.skip("Issue #805")
def test_del_rows_array_unordered():
    d0 = dt.Frame(range(10))
    del d0[[3, 1, 5, 2, 2, 0, -1], :]
    assert d0.internal.check()
    assert d0.topython() == [[4, 6, 7, 8]]


def test_del_rows_filter():
    d0 = dt.Frame(range(10), names=["A"], stype="int32")
    del d0[f.A < 4, :]
    assert d0.internal.check()
    assert d0.topython() == [[4, 5, 6, 7, 8, 9]]


def test_del_rows_nas():
    d0 = dt.Frame({"A": [1, 5, None, 12, 7, None, -3]})
    del d0[isna(f.A), :]
    assert d0.internal.check()
    assert d0.topython() == [[1, 5, 12, 7, -3]]


def test_del_rows_from_view1():
    d0 = dt.Frame(range(10))
    d1 = d0[::2, :]  # 0 2 4 6 8
    del d1[3, :]
    assert d1.topython() == [[0, 2, 4, 8]]
    del d1[[0, 3], :]
    assert d1.topython() == [[2, 4]]


def test_del_rows_from_view2():
    f0 = dt.Frame([1, 3, None, 4, 5, None, None, 2, None, None, None])
    f1 = f0[5:, :]
    del f1[isna(f[0]), :]
    assert f1.topython() == [[2]]



#-------------------------------------------------------------------------------
# ...
#-------------------------------------------------------------------------------

@pytest.mark.run(order=7)
def test_column_hexview(dt0, patched_terminal, capsys):
    assert dt0.internal.column(0).data_pointer
    dt.options.display.interactive_hint = False
    dt0.internal.column(-1).hexview()
    out, err = capsys.readouterr()
    assert (
        "Column 6\n"
        "Ltype: str, Stype: str32, Mtype: data\n"
        "Bytes: 20\n"
        "Meta: None\n"
        "Refcnt: 1\n"
        "     00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F  "
        "                \n"
        "---  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  "
        "----------------\n"
        "000  FF  FF  FF  FF  02  00  00  00  03  00  00  00  08  00  00  00  "
        "ÿÿÿÿ............\n"
        "010  0D  00  00  00                                                  "
        "....            \n"
        in out)

    with pytest.raises(ValueError) as e:
        dt0.internal.column(10).hexview()
    assert "Invalid column index" in str(e.value)

    dt0[::2].internal.column(1).hexview()
    out, err = capsys.readouterr()
    assert ("Column 1\n"
            "Ltype: bool, Stype: bool8, Mtype: data\n"
            "Bytes: 4\n"
            "Meta: None\n"
            "Refcnt: 2\n"
            in out)


@pytest.mark.run(order=8)
def test_rename():
    d0 = dt.Frame([[1], [2], ["hello"]])
    assert d0.names == ("C0", "C1", "C2")
    d0.rename({0: "x", -1: "z"})
    d0.rename({"C1": "y"})
    assert d0.names == ("x", "y", "z")
    assert d0.colindex("x") == 0
    assert d0.colindex("y") == 1
    assert d0.colindex("z") == 2


@pytest.mark.run(order=8.1)
def test_rename_bad():
    d0 = dt.Frame([[1], [2], ["hello"]], names=("a", "b", "c"))
    with pytest.raises(TypeError):
        d0.rename({"a", "b"})
    with pytest.raises(ValueError) as e:
        d0.rename({"xxx": "yyy"})
    assert "Column `xxx` does not exist" in str(e.value)
    with pytest.raises(ValueError) as e:
        d0.names = ['1', '2', '3', '5']
    assert ("Cannot rename columns to ['1', '2', '3', '5']: "
            "expected 3 names" in str(e.value))
    with pytest.raises(ValueError) as e:
        d0.names = ('k', )
    assert ("Cannot rename columns to ('k',): "
            "expected 3 names" in str(e.value))


@pytest.mark.run(order=8.2)
def test_rename_setter():
    d0 = dt.Frame([[7], [2], [5]])
    d0.names = ("A", "B", "E")
    d0.names = ["a", "c", "e"]
    assert d0.internal.check()
    assert d0.names == ("a", "c", "e")
    assert d0.colindex("a") == 0
    assert d0.colindex("c") == 1
    assert d0.colindex("e") == 2


@pytest.mark.run(order=9)
def test_internal_rowindex():
    d0 = dt.Frame(range(100))
    d1 = d0[:20, :]
    assert d0.internal.rowindex is None
    assert repr(d1.internal.rowindex) == "_RowIndex(0/20/1)"


#-------------------------------------------------------------------------------
# Test conversions into Pandas / Numpy
#-------------------------------------------------------------------------------

@pytest.mark.run(order=10)
@pytest.mark.usefixtures("pandas")
def test_topandas():
    d0 = dt.Frame({"A": [1, 5], "B": ["hello", "you"], "C": [True, False]})
    p0 = d0.topandas()
    assert p0.shape == (2, 3)
    assert same_iterables(p0.columns.tolist(), ["A", "B", "C"])
    assert p0["A"].values.tolist() == [1, 5]
    assert p0["B"].values.tolist() == ["hello", "you"]
    assert p0["C"].values.tolist() == [True, False]


@pytest.mark.run(order=11)
@pytest.mark.usefixtures("pandas")
def test_topandas_view():
    d0 = dt.Frame([[1, 5, 2, 0, 199, -12],
                   ["alpha", "beta", None, "delta", "epsilon", "zeta"],
                   [.3, 1e2, -.5, 1.9, 2.2, 7.9]], names=["A", "b", "c"])
    d1 = d0[::-2, :]
    p1 = d1.topandas()
    assert p1.shape == d1.shape
    assert p1.columns.tolist() == ["A", "b", "c"]
    assert p1.values.T.tolist() == d1.topython()


@pytest.mark.run(order=11.1)
@pytest.mark.usefixtures("pandas")
def test_topandas_nas():
    d0 = dt.Frame([[True, None, None, None, False],
                   [1, 5, None, -16, 100],
                   [249, None, 30000, 1, None],
                   [4587074, None, 109348, 1394, -343],
                   [None, None, None, None, 134918374091834]])
    assert d0.internal.check()
    assert d0.stypes == (dt.stype.bool8, dt.stype.int8, dt.stype.int16,
                         dt.stype.int32, dt.stype.int64)
    p0 = d0.topandas()
    # Check that each column in Pandas DataFrame has the correct number of NAs
    assert p0.count().tolist() == [2, 4, 3, 4, 1]


@pytest.mark.run(order=12)
def test_topython():
    src = [[-1, 0, 1, 3],
           ["cat", "dog", "mouse", "elephant"],
           [False, True, None, True]]
    d0 = dt.Frame(src, names=["A", "B", "C"])
    assert d0.ltypes == (ltype.int, ltype.str, ltype.bool)
    a0 = d0.topython()
    assert len(a0) == 3
    assert len(a0[0]) == 4
    assert a0 == src
    # Special case for booleans (because Booleans compare equal to 1/0)
    assert all(b is None or isinstance(b, bool) for b in a0[2])


@pytest.mark.run(order=13)
def test_topython2():
    src = [[1.0, None, float("nan"), 3.3]]
    d0 = dt.Frame(src)
    assert d0.ltypes == (ltype.real, )
    a0 = d0.topython()[0]
    assert a0 == [1.0, None, None, 3.3]


@pytest.mark.run(order=14)
def test_tonumpy0(numpy):
    d0 = dt.Frame([1, 3, 5, 7, 9])
    assert d0.stypes == (stype.int8, )
    a0 = d0.tonumpy()
    assert a0.shape == d0.shape
    assert a0.dtype == numpy.dtype("int8")
    assert a0.tolist() == [[1], [3], [5], [7], [9]]
    a1 = numpy.array(d0)
    assert (a0 == a1).all()


@pytest.mark.run(order=15)
def test_tonumpy1(numpy):
    d0 = dt.Frame({"A": [1, 5], "B": ["helo", "you"],
                   "C": [True, False], "D": [3.4, None]})
    a0 = d0.tonumpy()
    assert a0.shape == d0.shape
    assert a0.dtype == numpy.dtype("object")
    assert same_iterables(a0.T.tolist(), d0.topython())
    a1 = numpy.array(d0)
    assert (a0 == a1).all()


@pytest.mark.run(order=16)
def test_numpy_constructor_simple(numpy):
    tbl = [[1, 4, 27, 9, 22], [-35, 5, 11, 2, 13], [0, -1, 6, 100, 20]]
    d0 = dt.Frame(tbl)
    assert d0.shape == (5, 3)
    assert d0.stypes == (stype.int8, stype.int8, stype.int8)
    assert d0.topython() == tbl
    n0 = numpy.array(d0)
    assert n0.shape == d0.shape
    assert n0.dtype == numpy.dtype("int8")
    assert n0.T.tolist() == tbl


@pytest.mark.run(order=17)
def test_numpy_constructor_empty(numpy):
    d0 = dt.Frame()
    assert d0.shape == (0, 0)
    n0 = numpy.array(d0)
    assert n0.shape == (0, 0)
    assert n0.tolist() == []


@pytest.mark.run(order=18)
def test_numpy_constructor_multi_types(numpy):
    # Test that multi-types datatable will be promoted into a common type
    tbl = [[1, 5, 10],
           [True, False, False],
           [30498, 1349810, -134308],
           [1.454, 4.9e-23, 10000000]]
    d0 = dt.Frame(tbl)
    assert d0.stypes == (stype.int8, stype.bool8, stype.int32, stype.float64)
    n0 = numpy.array(d0)
    assert n0.dtype == numpy.dtype("float64")
    assert n0.T.tolist() == [[1.0, 5.0, 10.0],
                             [1.0, 0, 0],
                             [30498, 1349810, -134308],
                             [1.454, 4.9e-23, 1e7]]
    assert (d0.tonumpy() == n0).all()


@pytest.mark.run(order=19)
def test_numpy_constructor_view(numpy):
    d0 = dt.Frame([range(100), range(0, 1000000, 10000)])
    d1 = d0[::-2, :]
    assert d1.internal.isview
    n1 = numpy.array(d1)
    assert n1.dtype == numpy.dtype("int32")
    assert n1.T.tolist() == [list(range(99, 0, -2)),
                             list(range(990000, 0, -20000))]
    assert (d1.tonumpy() == n1).all()


@pytest.mark.run(order=20)
def test_numpy_constructor_single_col(numpy):
    d0 = dt.Frame([1, 1, 3, 5, 8, 13, 21, 34, 55])
    assert d0.stypes == (stype.int8, )
    n0 = numpy.array(d0)
    assert n0.shape == d0.shape
    assert n0.dtype == numpy.dtype("int8")
    assert (n0 == d0.tonumpy()).all()


@pytest.mark.run(order=21)
def test_numpy_constructor_single_string_col(numpy):
    d = dt.Frame(["adf", "dfkn", "qoperhi"])
    assert d.shape == (3, 1)
    assert d.stypes == (stype.str32, )
    a = numpy.array(d)
    assert a.shape == d.shape
    assert a.dtype == numpy.dtype("object")
    assert a.T.tolist() == d.topython()
    assert (a == d.tonumpy()).all()


@pytest.mark.run(order=22)
def test_numpy_constructor_view_1col(numpy):
    d0 = dt.Frame({"A": [1, 2, 3, 4], "B": [True, False, True, False]})
    d2 = d0[::2, "B"]
    a = d2.tonumpy()
    assert a.T.tolist() == [[True, True]]
    assert (a == numpy.array(d2)).all()


@pytest.mark.run(order=23)
def test_tonumpy_with_stype(numpy):
    """
    Test using dt.tonumpy() with explicit `stype` argument.
    """
    src = [[1.5, 2.6, 5.4], [3, 7, 10]]
    d0 = dt.Frame(src)
    assert d0.stypes == (stype.float64, stype.int8)
    a1 = d0.tonumpy("float32")
    a2 = d0.tonumpy()
    del d0
    # Check using list_equals(), which allows a tolerance of 1e-6, because
    # conversion to float32 resulted in loss of precision
    assert list_equals(a1.T.tolist(), src)
    assert a2.T.tolist() == src
    assert a1.dtype == numpy.dtype("float32")
    assert a2.dtype == numpy.dtype("float64")



#-------------------------------------------------------------------------------
# .scalar()
#-------------------------------------------------------------------------------

@pytest.mark.run(order=30)
def test_scalar1():
    d0 = dt.Frame([False])
    assert d0.scalar() is False
    d1 = dt.Frame([-32767])
    assert d1.scalar() == -32767
    d2 = dt.Frame([3.14159])
    assert d2.scalar() == 3.14159
    d3 = dt.Frame(["koo!"])
    assert d3.scalar() == "koo!"
    d4 = dt.Frame([None])
    assert d4.scalar() is None


@pytest.mark.run(order=31)
def test_scalar_bad():
    d0 = dt.Frame([100, 200])
    with pytest.raises(ValueError) as e:
        d0.scalar()
    assert (".scalar() method cannot be applied to a Frame with shape "
            "`[2 x 1]`" in str(e.value))


@pytest.mark.run(order=32)
def test_scalar_on_view(dt0):
    assert dt0[1, 0].scalar() == 7
    assert dt0[3, 3].scalar() == 4.4
    assert dt0[2, 6].scalar() == "hello"
    assert dt0[2::5, 3::7].scalar() == -4
    assert dt0[[3], "G"].scalar() == "world"



#-------------------------------------------------------------------------------
# Misc
#-------------------------------------------------------------------------------

@pytest.mark.run(order=33)
def test_issue898():
    """
    Test that reification of obj column views does not lead to any disastrous
    results (previously this was crashing).
    """
    class A: pass
    f0 = dt.Frame([A() for i in range(1111)])
    assert f0.stypes == (dt.stype.obj64, )
    f1 = f0[:-1, :]
    del f0
    f1.materialize()
    res = f1.topython()
    del res
