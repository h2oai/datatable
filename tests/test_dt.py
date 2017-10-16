#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import math
import pytest
import sys
import datatable as dt
from tests import same_iterables


#-------------------------------------------------------------------------------
# Prepare fixtures & helper functions
#-------------------------------------------------------------------------------

@pytest.fixture()
def dt0():
    return dt.DataTable([
        [2, 7, 0, 0],
        [True, False, False, True],
        [1, 1, 1, 1],
        [0.1, 2, -4, 4.4],
        [None, None, None, None],
        [0, 0, 0, 0],
        ["1", "2", "hello", "world"],
    ], colnames=list("ABCDEFG"))


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
    assert isinstance(dt0, dt.DataTable)
    assert dt0.internal.check()
    assert dt0.nrows == 4
    assert dt0.ncols == 7
    assert dt0.shape == (4, 7)
    assert dt0.names == ("A", "B", "C", "D", "E", "F", "G")
    assert dt0.types == ("int", "bool", "bool", "real", "bool", "bool", "str")
    assert dt0.stypes == ("i1i", "i1b", "i1b", "f8r", "i1b", "i1b", "i4s")
    assert dt0.internal.alloc_size > 500
    assert (sys.getsizeof(dt0) > dt0.internal.alloc_size +
            sys.getsizeof(dt0.names) +
            sum(sys.getsizeof(colname) for colname in dt0.names) +
            sys.getsizeof(dt0._inames) + sys.getsizeof(dt0._types) +
            sys.getsizeof(dt0._stypes))
    # +--------------+------------+    +------+
    # | nrows:     8 | **stats: 8 |--->| NULL |
    # | ncols:     8 |------------+    +------+
    # |--------------|
    # | **columns: 8 |    +----------------------------+----------------------------+----------------------------+----------------------------+----------------------------+----------------------------+----------------------------+------------+
    # |              |--->| Column* (i1i): 8           | Column* (i1b): 8           | Column* (i1b): 8           | Column* (f8r): 8           | Column* (i1b): 8           | Column* (i1b): 8           | Column* (i4s): 8           | Column*: 8 |
    # |--------------+    +----------------------------+----------------------------+----------------------------+----------------------------+----------------------------+----------------------------+----------------------------+------------+
    # | *rowindex: 8 |      |                            |                            |                            |                            |                            |                            |                                 |
    # +--------------+      |  +------+     +------+     |  +------+     +------+     |  +------+     +------+     |  +------+     +------+     |  +------+     +------+     |  +------+     +------+     |  +------+     +------+          v
    #         |             |  | NULL |     | NULL |     |  | NULL |     | NULL |     |  | NULL |     | NULL |     |  | NULL |     | NULL |     |  | NULL |     | NULL |     |  | NULL |     | NULL |     |  | NULL |     | NULL |      +------+
    #         v             |  +------+     +------+     |  +------+     +------+     |  +------+     +------+     |  +------+     +------+     |  +------+     +------+     |  +------+     +------+     |  +------+     +------+      | NULL |
    #     +------+          |     ^             ^        |     ^             ^        |     ^             ^        |     ^             ^        |     ^             ^        |     ^             ^        |     ^             ^         +------+
    #     | NULL |          v     |             |        v     |             |        v     |             |        v     |             |        v     |             |        v     |             |        v     |             |
    #     +------+        +---------------------------++---------------------------++---------------------------++---------------------------++---------------------------++---------------------------++---------------------------+
    #                     | *pybuf:     8 | *stats: 8 || *pybuf:     8 | *stats: 8 || *pybuf:     8 | *stats: 8 || *pybuf:     8 | *stats: 8 || *pybuf:     8 | *stats: 8 || *pybuf:     8 | *stats: 8 || *pybuf:     8 | *stats: 8 |
    #                     |---------------------------||---------------------------||---------------------------||---------------------------||---------------------------||---------------------------||---------------------------|
    #                     | refcount:   4 | mtype:  1 || refcount:   4 | mtype:  1 || refcount:   4 | mtype:  1 || refcount:   4 | mtype:  1 || refcount:   4 | mtype:  1 || refcount:   4 | mtype:  1 || refcount:   4 | mtype:  1 |
    #                     | alloc_size: 8 | stype:  1 || alloc_size: 8 | stype:  1 || alloc_size: 8 | stype:  1 || alloc_size: 8 | stype:  1 || alloc_size: 8 | stype:  1 || alloc_size: 8 | stype:  1 || alloc_size: 8 | stype:  1 |
    #                     | _padding:   2 | nrows:  8 || _padding:   2 | nrows:  8 || _padding:   2 | nrows:  8 || _padding:   2 | nrows:  8 || _padding:   2 | nrows:  8 || _padding:   2 | nrows:  8 || _padding:   2 | nrows:  8 |
    #                     |---------------|-----------||---------------|-----------||---------------|-----------||---------------|-----------||---------------|-----------||---------------|-----------||---------------|-----------|
    #                     | *data:      8 | *meta:  8 || *data:      8 | *meta:  8 || *data:      8 | *meta:  8 || *data:      8 | *meta:  8 || *data:      8 | *meta:  8 || *data:      8 | *meta:  8 || *data:      8 | *meta:  8 |
    #                     +---------------------------++---------------------------++---------------------------++---------------------------++---------------------------++---------------------------++---------------------------+
    #                             |             |              |             |              |             |              |             |              |             |              |             |              |             |
    #                             v             v              v             v              v             v              v             v              v             v              v             v              v             v
    #                           +---+       +------+         +---+       +------+         +---+       +------+         +---+       +------+         +---+       +------+         +---+       +------+         +---+         +---+
    #                           | 1 | 2     | NULL |         | 1 | 1     | NULL |         | 1 | 1     | NULL |         | 8 | 0.1   | NULL |         | 1 | NA_I8 | NULL |         | 1 | 0     | NULL |         | 1 | "1"     | 8 | 12
    #                           |---|       +------+         |---|       +------+         |---|       +------+         |---|       +------+         |---|       +------+         |---|       +------+         |---|         +---+
    #                           | 1 | 7                      | 1 | 0                      | 1 | 1                      | 8 | 2                      | 1 | NA_I8                  | 1 | 0                      | 1 | "2"
    #                           |---|                        |---|                        |---|                        |---|                        |---|                        |---|                        |---|
    #                           | 1 | 0                      | 1 | 0                      | 1 | 1                      | 8 | -4                     | 1 | NA_I8                  | 1 | 0                      | 5 | "hello"
    #                           |---|                        |---|                        |---|                        |---|                        |---|                        |---|                        |---|
    #                           | 1 | 0                      | 1 | 1                      | 1 | 1                      | 8 | 4.4                    | 1 | NA_I8                  | 1 | 0                      | 4 | "world"
    #                           +---+                        +---+                        +---+                        +---+                        +---+                        +---+                        |---|
    #                                                                                                                                                                                                         | 1 | pad
    #                                                                                                                                                                                                         |---|
    #                                                                                                                                                                                                         | 4 | -1
    #                                                                                                                                                                                                         |---|
    #                                                                                                                                                                                                         | 4 | 2
    #                                                                                                                                                                                                         |---|
    #                                                                                                                                                                                                         | 4 | 3
    #                                                                                                                                                                                                         |---|
    #                                                                                                                                                                                                         | 4 | 8
    #                                                                                                                                                                                                         |---|
    #                                                                                                                                                                                                         | 4 | 12
    #                                                                                                                                                                                                         +---+


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



@pytest.mark.run(order=6)
def test_dt_delitem():
    """
    Test deleting columns from a datatable.
    Note: don't use dt0 here, because this test will modify it, potentially
    invalidating other tests.
    """
    def smalldt():
        return dt.DataTable([[i] for i in range(16)],
                            colnames="ABCDEFGHIJKLMNOP")

    d0 = smalldt()
    del d0["A"]
    assert d0.internal.check()
    assert d0.shape == (1, 15)
    assert d0.names == tuple("BCDEFGHIJKLMNOP")
    d0 = smalldt()
    del d0["B"]
    assert d0.internal.check()
    assert d0.shape == (1, 15)
    assert d0.names == tuple("ACDEFGHIJKLMNOP")
    d0 = smalldt()
    del d0[-1]
    assert d0.internal.check()
    assert d0.shape == (1, 15)
    assert d0.names == tuple("ABCDEFGHIJKLMNO")
    del d0["E":"J"]
    assert d0.internal.check()
    assert d0.shape == (1, 9)
    assert d0.names == tuple("ABCDKLMNO")
    assert as_list(d0) == [[0], [1], [2], [3], [10], [11], [12], [13], [14]]
    del d0[::2]
    assert d0.names == tuple("BDLN")
    assert as_list(d0) == [[1], [3], [11], [13]]
    del d0[0]
    assert d0.names == tuple("DLN")
    assert as_list(d0) == [[3], [11], [13]]
    del d0[:]
    assert d0.names == tuple()
    assert d0.shape == (1, 0)
    d0 = smalldt()
    del d0[[0, 3, 0, 5, 0, 9]]
    assert d0.internal.check()
    assert d0.names == tuple("BCEGHIKLMNOP")
    assert d0.shape == (1, 12)
    with pytest.raises(TypeError) as e:
        del d0[0.5]
    assert "Cannot delete 0.5 from the datatable" in str(e.value)
    with pytest.raises(TypeError):
        del d0[d0]
    with pytest.raises(TypeError):
        del d0[[1, 2, 1, 0.7]]
    with pytest.raises(TypeError):
        del d0[[slice(10), 2, 0]]



@pytest.mark.run(order=7)
def test_column_hexview(dt0, patched_terminal, capsys):
    assert dt0.internal.column(0).data_pointer
    dt0.internal.column(-1).hexview()
    out, err = capsys.readouterr()
    print(out)
    assert ("Column 6\n"
            "Ltype: str, Stype: i4s, Mtype: data\n"
            "Bytes: 32\n"
            "Meta: offoff=16\n"
            "Refcnt: 1\n"
            "     00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F                  \n"
            "---  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  ----------------\n"
            "000  31  32  68  65  6C  6C  6F  77  6F  72  6C  64  FF  FF  FF  FF  12helloworldÿÿÿÿ\n"
            "010  02  00  00  00  03  00  00  00  08  00  00  00  0D  00  00  00  ................\n"
            in out)

    with pytest.raises(ValueError) as e:
        dt0.internal.column(10).hexview()
    assert "Invalid column index" in str(e.value)

    dt0[::2].internal.column(1).hexview()
    out, err = capsys.readouterr()
    print(out)
    assert ("Column 1\n"
            "Ltype: bool, Stype: i1b, Mtype: data\n"
            "Bytes: 4\n"
            "Meta: None\n"
            "Refcnt: 2\n"
            in out)


@pytest.mark.run(order=8)
def test_rename():
    d0 = dt.DataTable([[1], [2], ["hello"]])
    assert d0.names == ("C1", "C2", "C3")
    d0.rename({0: "x", "C2": "y", -1: "z"})
    assert d0.names == ("x", "y", "z")
    with pytest.raises(TypeError):
        d0.rename(["a", "b"])
    with pytest.raises(ValueError) as e:
        d0.rename({"xxx": "yyy"})
    assert "Column `xxx` does not exist" in str(e.value)


@pytest.mark.run(order=9)
def test_internal_rowindex():
    d0 = dt.DataTable(list(range(100)))
    d1 = d0[:20, :]
    assert d0.internal.rowindex is None
    assert repr(d1.internal.rowindex) == "_RowIndex(0:20:1)"



#-------------------------------------------------------------------------------
# Test conversions into Pandas / Numpy
#-------------------------------------------------------------------------------

@pytest.mark.run(order=10)
@pytest.mark.usefixture("pandas")
def test_topandas():
    d0 = dt.DataTable({"A": [1, 5], "B": ["hello", "you"], "C": [True, False]})
    p0 = d0.topandas()
    assert p0.shape == (2, 3)
    assert same_iterables(p0.columns.tolist(), ["A", "B", "C"])
    assert p0["A"].values.tolist() == [1, 5]
    assert p0["B"].values.tolist() == ["hello", "you"]
    assert p0["C"].values.tolist() == [True, False]


@pytest.mark.run(order=11)
@pytest.mark.usefixture("pandas")
def test_topandas_view():
    d0 = dt.DataTable([[1, 5, 2, 0, 199, -12],
                       ["alpha", "beta", None, "delta", "epsilon", "zeta"],
                       [.3, 1e2, -.5, 1.9, 2.2, 7.9]], colnames=["A", "b", "c"])
    d1 = d0[::-2, :]
    p1 = d1.topandas()
    assert p1.shape == d1.shape
    assert p1.columns.tolist() == ["A", "b", "c"]
    assert p1.values.T.tolist() == d1.topython()


@pytest.mark.run(order=12)
def test_topython():
    src = [[-1, 0, 1, 3],
           ["cat", "dog", "mouse", "elephant"],
           [False, True, None, True]]
    d0 = dt.DataTable(src, colnames=["A", "B", "C"])
    assert d0.types == ("int", "str", "bool")
    a0 = d0.topython()
    assert len(a0) == 3
    assert len(a0[0]) == 4
    assert a0 == src
    # Special case for booleans (because Booleans compare equal to 1/0)
    assert all(b is None or isinstance(b, bool) for b in a0[2])


@pytest.mark.run(order=13)
def test_topython2():
    src = [[1.0, None, float("nan"), 3.3]]
    d0 = dt.DataTable(src)
    assert d0.types == ("real", )
    a0 = d0.topython()[0]
    assert a0 == [1.0, None, None, 3.3]


@pytest.mark.run(order=14)
def test_tonumpy0(numpy):
    d0 = dt.DataTable([1, 3, 5, 7, 9])
    a0 = d0.tonumpy()
    assert a0.shape == d0.shape
    assert a0.dtype == numpy.dtype("int8")
    assert a0.tolist() == [[1], [3], [5], [7], [9]]
    a1 = numpy.array(d0)
    assert (a0 == a1).all()


@pytest.mark.run(order=15)
def test_tonumpy1(numpy):
    d0 = dt.DataTable({"A": [1, 5], "B": ["helo", "you"],
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
    d0 = dt.DataTable(tbl)
    assert d0.shape == (5, 3)
    assert d0.stypes == ("i1i", "i1i", "i1i")
    assert d0.topython() == tbl
    n0 = numpy.array(d0)
    assert n0.shape == d0.shape
    assert n0.dtype == numpy.dtype("int8")
    assert n0.T.tolist() == tbl


@pytest.mark.run(order=17)
def test_numpy_constructor_empty(numpy):
    d0 = dt.DataTable()
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
    d0 = dt.DataTable(tbl)
    assert d0.stypes == ("i1i", "i1b", "i4i", "f8r")
    n0 = numpy.array(d0)
    assert n0.dtype == numpy.dtype("float64")
    assert n0.T.tolist() == [[1.0, 5.0, 10.0],
                             [1.0, 0, 0],
                             [30498, 1349810, -134308],
                             [1.454, 4.9e-23, 1e7]]
    assert (d0.tonumpy() == n0).all()


@pytest.mark.run(order=19)
def test_numpy_constructor_view(numpy):
    d0 = dt.DataTable([list(range(100)), list(range(0, 1000000, 10000))])
    d1 = d0[::-2, :]
    assert d1.internal.isview
    n1 = numpy.array(d1)
    assert n1.dtype == numpy.dtype("int32")
    assert n1.T.tolist() == [list(range(99, 0, -2)),
                             list(range(990000, 0, -20000))]
    assert (d1.tonumpy() == n1).all()


@pytest.mark.run(order=20)
def test_numpy_constructor_single_col(numpy):
    d0 = dt.DataTable([1, 1, 3, 5, 8, 13, 21, 34, 55])
    assert d0.stypes == ("i1i", )
    n0 = numpy.array(d0)
    assert n0.shape == d0.shape
    assert n0.dtype == numpy.dtype("int8")
    assert (n0 == d0.tonumpy()).all()


@pytest.mark.run(order=21)
def test_numpy_constructor_single_string_col(numpy):
    d = dt.DataTable(["adf", "dfkn", "qoperhi"])
    assert d.shape == (3, 1)
    assert d.stypes == ("i4s", )
    a = numpy.array(d)
    assert a.shape == d.shape
    assert a.dtype == numpy.dtype("object")
    assert a.T.tolist() == d.topython()
    assert (a == d.tonumpy()).all()


@pytest.mark.run(order=22)
def test_numpy_constructor_view_1col(numpy):
    d0 = dt.DataTable({"A": [1, 2, 3, 4], "B": [True, False, True, False]})
    d2 = d0[::2, "B"]
    a = d2.tonumpy()
    assert a.T.tolist() == [[True, True]]
    assert (a == numpy.array(d2)).all()
