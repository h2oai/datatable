#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import pytest
import datatable as dt


#-------------------------------------------------------------------------------
# Prepare fixtures & helper functions
#-------------------------------------------------------------------------------

@pytest.fixture()
def dt0():
    return dt.DataTable({
        "A": [2, 7, 0, 0],
        "B": [True, False, False, True],
        "C": [1, 1, 1, 1],
        "D": [0.1, 2, -4, 4.4],
        "E": [None, None, None, None],
        "F": [0, 0, 0, 0],
        "G": [1, 2, "hello", "world"],
    })

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

def test_dt_properties(dt0):
    assert isinstance(dt0, dt.DataTable)
    assert dt0.nrows == 4
    assert dt0.ncols == 7
    assert dt0.shape == (4, 7)
    assert dt0.names == ("A", "B", "C", "D", "E", "F", "G")
    assert dt0.types == ("int", "bool", "bool", "real", "bool", "bool", "str")
    assert dt0.stypes == ("i1i", "i1b", "i1b", "f8r", "i1b", "i1b", "i4s")
    assert dt0.internal.verify_integrity() is None


def test_dt_call(dt0, capsys):
    dt1 = dt0(timeit=True)
    assert dt1.shape == dt0.shape
    assert dt1.internal.isview and dt1.internal.rowmapping_type == "slice"
    out, err = capsys.readouterr()
    assert err == ""
    assert "Time taken:" in out


def test_dt_view(dt0, monkeypatch, capsys):
    def send1(refresh_rate=1):
        yield None
    monkeypatch.setattr("datatable.utils.terminal.wait_for_keypresses", send1)
    dt0.view()
    out, err = capsys.readouterr()
    assert ("     A   B   C     D   E   F  G    \n"
            "--  --  --  --  ----  --  --  -----\n"
            " 0   2   1   1   0.1       0  1    \n"
            " 1   7   0   1   2         0  2    \n"
            " 2   0   0   1  -4         0  hello\n"
            " 3   0   1   1   4.4       0  world\n"
            "\n"
            "[4 rows x 7 columns]\n"
            "Press q to quit  ↑←↓→ to move  wasd to page  t to toggle types"
            in out)


def test_dt_colindex(dt0):
    for i, ch in enumerate("ABCDEFG"):
        assert dt0.colindex(ch) == i
    with pytest.raises(ValueError) as e:
        dt0.colindex("a")
    assert "Column 'a' does not exist" in str(e.value)


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
    assert d0.internal.verify_integrity() is None
    assert d0.shape == (1, 15)
    assert d0.names == tuple("BCDEFGHIJKLMNOP")
    d0 = smalldt()
    del d0["B"]
    assert d0.internal.verify_integrity() is None
    assert d0.shape == (1, 15)
    assert d0.names == tuple("ACDEFGHIJKLMNOP")
    d0 = smalldt()
    del d0[-1]
    assert d0.internal.verify_integrity() is None
    assert d0.shape == (1, 15)
    assert d0.names == tuple("ABCDEFGHIJKLMNO")
    del d0["E":"J"]
    assert d0.internal.verify_integrity() is None
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
    assert d0.internal.verify_integrity() is None
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



def test__hex(dt0, monkeypatch, capsys):
    def send1(refresh_rate=1):
        yield None
    monkeypatch.setattr("datatable.utils.terminal.wait_for_keypresses", send1)
    dt.utils.terminal.term.override_width = 100

    dt0._hex(-1)
    out, err = capsys.readouterr()
    print(out)
    assert ("Column 6, Name: 'G'\n"
            "Ltype: str, Stype: i4s, Mtype: data\n"
            "Bytes: 36\n"
            "Meta: offoff=20\n"
            "Refcnt: 1\n"
            "    00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F                  \n"
            "--  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  ----------------\n"
            " 0  31  32  68  65  6C  6C  6F  77  6F  72  6C  64  FF  FF  FF  FF  12helloworldÿÿÿÿ\n"
            " 1  FF  FF  FF  FF  02  00  00  00  03  00  00  00  08  00  00  00  ÿÿÿÿ............\n"
            " 2  0D  00  00  00                                                  ....            \n"
            in out)

    with pytest.raises(ValueError) as e:
        dt0._hex(10)
    assert "Invalid column index" in str(e.value)

    dt0[::2]._hex(1)
    out, err = capsys.readouterr()
    print(out)
    assert ("Column 1, Name: 'C'\n"
            "Ltype: bool, Stype: i1b, Mtype: data\n"
            "Bytes: 4\n"
            "Meta: None\n"
            "Refcnt: 2\n"
            in out)
