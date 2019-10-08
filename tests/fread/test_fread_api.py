#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
# Tests in this file are specifically aimed at checking the API of `fread`
# function / class. This includes: presence of various parameters, checks that
# the parameters do what they are supposed to do.
#-------------------------------------------------------------------------------
import pytest
import datatable as dt
import os
from datatable import ltype, stype, DatatableWarning, FreadWarning
from datatable.internal import frame_integrity_check



#-------------------------------------------------------------------------------
# Tests for fread "source" arguments:
# the unnamed first argument, as well as `file`, `text`, `cmd`, `url`.
#-------------------------------------------------------------------------------

def test_fread_from_file1(tempfile):
    with open(tempfile, "w") as o:
        o.write("A,B\n1,2")
    d0 = dt.fread(tempfile)
    frame_integrity_check(d0)
    assert d0.names == ("A", "B")
    assert d0.to_list() == [[1], [2]]


def test_fread_from_file2():
    with pytest.raises(ValueError):
        dt.fread(file="a,b\n1,2")


def test_fread_from_text1():
    d0 = dt.fread(text="A")
    frame_integrity_check(d0)
    assert d0.names == ("A",)
    assert d0.shape == (0, 1)


def test_fread_from_cmd1():
    d0 = dt.fread(cmd="ls -l")
    frame_integrity_check(d0)
    # It is difficult to assert anything about the contents or structure
    # of the resulting dataset...


def test_fread_from_cmd2():
    d0 = dt.fread(cmd="ls", header=False)
    frame_integrity_check(d0)
    assert d0.ncols == 1
    assert d0.nrows >= 12
    d1 = dt.fread(cmd="cat LICENSE", sep="\n")
    frame_integrity_check(d1)
    assert d1.nrows == 372


def test_fread_from_cmd3(tempfile):
    with open(tempfile, "w") as o:
        o.write("A,B\n1,2\n3,4\n5,6\n7,3\n8,9\n")
    assert dt.fread(cmd="grep -v 3 %s" % tempfile) \
             .to_list() == [[1, 5, 8], [2, 6, 9]]


def test_fread_from_url1():
    with pytest.raises(ValueError) as e:
        dt.fread(url="A")
    assert "unknown url type" in str(e.value)


def test_fread_from_url2():
    path = os.path.abspath("LICENSE")
    d0 = dt.fread("file://" + path, sep="\n")
    frame_integrity_check(d0)
    assert d0.shape == (372, 1)


def test_fread_from_anysource_as_text1(capsys):
    src = "A\n" + "\n".join(str(i) for i in range(100))
    assert len(src) < 4096
    d0 = dt.fread(src, verbose=True)
    out, err = capsys.readouterr()
    frame_integrity_check(d0)
    assert not err
    assert "Input contains '\\x0A', treating it as raw text" in out


def test_fread_from_anysource_as_text2(capsys):
    src = "A\n" + "\n".join(str(i) for i in range(1000, 2000))
    assert len(src) > 4096
    d0 = dt.fread(src, verbose=True)
    out, err = capsys.readouterr()
    frame_integrity_check(d0)
    assert not err
    assert ("Input is a string of length %d, treating it as raw text"
            % len(src)) in out


def test_fread_from_anysource_as_text3(capsys):
    src = b"A,B,C\n1,2,3\n5,4,3\n"
    d0 = dt.fread(src, verbose=True)
    out, err = capsys.readouterr()
    frame_integrity_check(d0)
    assert not err
    assert d0.to_list() == [[1, 5], [2, 4], [3, 3]]
    assert "Input contains '\\x0A', treating it as raw text" in out


def test_fread_from_anysource_as_file1(tempfile, capsys):
    assert isinstance(tempfile, str)
    with open(tempfile, "w") as o:
        o.write("A,B\n1,2\n")
    d0 = dt.fread(tempfile, verbose=True)
    out, err = capsys.readouterr()
    frame_integrity_check(d0)
    assert not err
    assert "Input is assumed to be a file name" in out


def test_fread_from_anysource_as_file2(tempfile, py36):
    import pathlib
    with open(tempfile, "w") as o:
        o.write("A,B\n1,2\n")
    d0 = dt.fread(tempfile)
    d1 = dt.fread(tempfile.encode())
    d2 = dt.fread(pathlib.Path(tempfile))
    frame_integrity_check(d0)
    frame_integrity_check(d1)
    frame_integrity_check(d2)
    assert d0.to_list() == d1.to_list() == d2.to_list()


def test_fread_from_anysource_filelike():
    class MyFile:
        def __init__(self):
            self.name = b"fake file"

        def fileno(self):
            return NotImplemented

        def read(self):
            return "A,B,C\na,b,c\n"

    d0 = dt.fread(MyFile())
    frame_integrity_check(d0)
    assert d0.names == ("A", "B", "C")
    assert d0.to_list() == [["a"], ["b"], ["c"]]


def test_fread_from_anysource_as_url(tempfile, capsys):
    assert isinstance(tempfile, str)
    with open(tempfile, "w") as o:
        o.write("A,B\n1,2\n")
    d0 = dt.fread("file://" + os.path.abspath(tempfile), verbose=True)
    out, err = capsys.readouterr()
    frame_integrity_check(d0)
    assert not err
    assert "Input is a URL" in out


def test_fread_from_stringbuf():
    from io import StringIO
    s = StringIO("A,B,C\n1,2,3\n4,5,6")
    d0 = dt.fread(s)
    frame_integrity_check(d0)
    assert d0.names == ("A", "B", "C")
    assert d0.to_list() == [[1, 4], [2, 5], [3, 6]]


def test_fread_from_fileobj(tempfile):
    with open(tempfile, "w") as f:
        f.write("A,B,C\nfoo,bar,baz\n")

    with open(tempfile, "r") as f:
        d0 = dt.fread(f)
        frame_integrity_check(d0)
        assert d0.names == ("A", "B", "C")
        assert d0.to_list() == [["foo"], ["bar"], ["baz"]]


def test_fread_file_not_exists():
    name = "qerubvwpif8rAIB9845gb1_"
    path = os.path.abspath(".")
    with pytest.raises(ValueError) as e:
        dt.fread(name)
    assert ("File %s`/%s` does not exist" % (path, name)) in str(e.value)


def test_fread_file_is_directory():
    path = os.path.abspath(".")
    with pytest.raises(ValueError) as e:
        dt.fread(path)
    assert ("Path `%s` is not a file" % path) in str(e.value)


def test_fread_xz_file(tempfile, capsys):
    import lzma
    xzfile = tempfile + ".xz"
    with lzma.open(xzfile, "wb") as f:
        f.write(b"A\n1\n2\n3\n")
    d0 = dt.fread(xzfile, verbose=True)
    out, err = capsys.readouterr()
    frame_integrity_check(d0)
    assert d0.to_list() == [[1, 2, 3]]
    assert not err
    assert ("Extracting %s into memory" % xzfile) in out
    os.unlink(xzfile)


def test_fread_gz_file(tempfile, capsys):
    import gzip
    gzfile = tempfile + ".gz"
    with gzip.open(gzfile, "wb") as f:
        f.write(b"A\n10\n20\n30\n")
    d0 = dt.fread(gzfile, verbose=True)
    out, err = capsys.readouterr()
    frame_integrity_check(d0)
    assert d0.to_list() == [[10, 20, 30]]
    assert not err
    assert ("Extracting %s into memory" % gzfile) in out
    os.unlink(gzfile)


def test_fread_bz2_file(tempfile, capsys):
    import bz2
    bzfile = tempfile + ".bz2"
    with bz2.open(bzfile, "wb") as f:
        f.write(b"A\n11\n22\n33\n")
    try:
        d0 = dt.fread(bzfile, verbose=True)
        out, err = capsys.readouterr()
        frame_integrity_check(d0)
        assert d0.to_list() == [[11, 22, 33]]
        assert not err
        assert ("Extracting %s into memory" % bzfile) in out
    finally:
        os.remove(bzfile)


def test_fread_zip_file_1(tempfile, capsys):
    import zipfile
    zfname = tempfile + ".zip"
    with zipfile.ZipFile(zfname, "x", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("data1.csv", "a,b,c\n10,20,30\n5,7,12\n")
    d0 = dt.fread(zfname, verbose=True)
    out, err = capsys.readouterr()
    frame_integrity_check(d0)
    assert d0.names == ("a", "b", "c")
    assert d0.to_list() == [[10, 5], [20, 7], [30, 12]]
    assert not err
    assert ("Extracting %s to temporary directory" % zfname) in out
    os.unlink(zfname)


def test_fread_zip_file_multi(tempfile):
    import zipfile
    zfname = tempfile + ".zip"
    with zipfile.ZipFile(zfname, "x", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("data1.csv", "a,b,c\nfoo,bar,baz\ngee,jou,sha\n")
        zf.writestr("data2.csv", "A,B,C\n3,4,5\n6,7,8\n")
        zf.writestr("data3.csv", "Aa,Bb,Cc\ntrue,1.5,\nfalse,1e+20,yay\n")
    with pytest.warns(FreadWarning) as ws:
        d0 = dt.fread(zfname)
        d1 = dt.fread(zfname + "/data2.csv")
        d2 = dt.fread(zfname + "/data3.csv")
    frame_integrity_check(d0)
    frame_integrity_check(d1)
    frame_integrity_check(d2)
    assert d0.names == ("a", "b", "c")
    assert d1.names == ("A", "B", "C")
    assert d2.names == ("Aa", "Bb", "Cc")
    assert d0.to_list() == [["foo", "gee"], ["bar", "jou"], ["baz", "sha"]]
    assert d1.to_list() == [[3, 6], [4, 7], [5, 8]]
    assert d2.to_list() == [[True, False], [1.5, 1e20], ["", "yay"]]
    assert len(ws) == 1
    assert ("Zip file %s contains multiple compressed files"
            % zfname) in ws[0].message.args[0]
    os.unlink(zfname)


def test_fread_zip_file_bad1(tempfile):
    import zipfile
    zfname = tempfile + ".zip"
    with zipfile.ZipFile(zfname, "x"):
        pass
    with pytest.raises(ValueError) as e:
        dt.fread(zfname)
    assert ("Zip file %s is empty" % zfname) in str(e.value)
    os.unlink(zfname)


def test_fread_zip_file_bad2(tempfile):
    import zipfile
    zfname = tempfile + ".zip"
    with zipfile.ZipFile(zfname, "x") as zf:
        zf.writestr("data1.csv", "Egeustimentis")
    with pytest.raises(ValueError) as e:
        dt.fread(zfname + "/out.csv")
    assert "File `out.csv` does not exist in archive" in str(e.value)
    os.unlink(zfname)


def test_fread_bad_source_none():
    with pytest.raises(ValueError) as e:
        dt.fread()
    assert "No input source" in str(e.value)


def test_fread_bad_source_any_and_source():
    with pytest.raises(ValueError) as e:
        dt.fread("a", text="b")
    assert "When an unnamed argument is passed, it is invalid to also " \
           "provide the `text` parameter" in str(e.value)


def test_fread_bad_source_2sources():
    with pytest.raises(ValueError) as e:
        dt.fread(file="a", text="b")
    assert "Both parameters `file` and `text` cannot be passed to fread " \
           "simultaneously" in str(e.value)


def test_fread_bad_source_anysource():
    with pytest.raises(TypeError) as e:
        dt.fread(12345)
    assert "Unknown type for the first argument in fread" in str(e.value)


def test_fread_bad_source_text():
    with pytest.raises(TypeError) as e:
        dt.fread(text=["a", "b", "c"])
    assert ("Invalid parameter `text` in fread: expected str or bytes"
            in str(e.value))


def test_fread_bad_source_file():
    with pytest.raises(TypeError) as e:
        dt.fread(file=TypeError)
    assert ("Invalid parameter `file` in fread: expected a str/bytes/PathLike"
            in str(e.value))


def test_fread_bad_source_cmd():
    with pytest.raises(TypeError) as e:
        dt.fread(cmd=["ls", "-l", ".."])
    assert "Invalid parameter `cmd` in fread: expected str" in str(e.value)


def test_fread_from_glob(tempfile):
    base, ext = os.path.splitext(tempfile)
    if not ext:
        ext = ".csv"
    pattern = base + "*" + ext
    tempfiles = ["".join([base, str(i), ext]) for i in range(10)]
    try:
        for j in range(10):
            with open(tempfiles[j], "w") as f:
                f.write("A,B,C\n0,0,0\n%d,%d,%d\n"
                        % (j, j * 2 + 1, (j + 3) * 17 % 23))
        res = dt.fread(pattern)
        assert len(res) == 10
        assert set(res.keys()) == set(tempfiles)
        for f in res.values():
            assert isinstance(f, dt.Frame)
            frame_integrity_check(f)
            assert f.names == ("A", "B", "C")
            assert f.shape == (2, 3)
        df = dt.rbind(*[res[f] for f in tempfiles])
        frame_integrity_check(df)
        assert df.names == ("A", "B", "C")
        assert df.shape == (20, 3)
        assert df.to_list() == [
            [0, 0, 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7, 0, 8, 0, 9],
            [0, 1, 0, 3, 0, 5, 0, 7, 0, 9, 0, 11, 0, 13, 0, 15, 0, 17, 0, 19],
            [0, 5, 0, 22, 0, 16, 0, 10, 0, 4, 0, 21, 0, 15, 0, 9, 0, 3, 0, 20]
        ]
    finally:
        for f in tempfiles:
            os.remove(f)




#-------------------------------------------------------------------------------
# `columns`
#-------------------------------------------------------------------------------

def test_fread_columns_slice():
    d0 = dt.fread(text="A,B,C,D,E\n1,2,3,4,5", columns=slice(None, None, 2))
    frame_integrity_check(d0)
    assert d0.names == ("A", "C", "E")
    assert d0.to_list() == [[1], [3], [5]]


def test_fread_columns_range():
    d0 = dt.fread(text="A,B,C,D,E\n1,2,3,4,5", columns=range(3))
    frame_integrity_check(d0)
    assert d0.names == ("A", "B", "C")
    assert d0.to_list() == [[1], [2], [3]]


def test_fread_columns_range_bad1():
    with pytest.raises(ValueError) as e:
        dt.fread(text="A,B,C,D,E\n1,2,3,4,5", columns=range(3, 0, -1))
    assert "Cannot use slice/range with negative step" in str(e.value)


def test_fread_columns_range_bad2():
    with pytest.raises(ValueError) as e:
        dt.fread(text="A,B,C,D,E\n1,2,3,4,5", columns=range(13))
    assert "Invalid range iterator" in str(e.value)


def test_fread_columns_list1():
    d0 = dt.fread(text="A,B,C\n1,2,3", columns=["foo", "bar", "baz"])
    frame_integrity_check(d0)
    assert d0.names == ("foo", "bar", "baz")
    assert d0.to_list() == [[1], [2], [3]]


def test_fread_columns_list2():
    d0 = dt.fread(text="A,B,C\n1,2,3", columns=["foo", None, "baz"])
    frame_integrity_check(d0)
    assert d0.names == ("foo", "baz")
    assert d0.to_list() == [[1], [3]]


def test_fread_columns_list3():
    d0 = dt.fread(text="A,B,C\n1,2,3", columns=[("foo", str), None, None])
    frame_integrity_check(d0)
    assert d0.names == ("foo", )
    assert d0.to_list() == [["1"]]


def test_fread_columns_list_of_types():
    d0 = dt.fread(text="A,B,C\n1,2,3",
                  columns=(stype.int32, stype.float64, stype.str32))
    frame_integrity_check(d0)
    assert d0.names == ("A", "B", "C")
    assert d0.stypes == (stype.int32, stype.float64, stype.str32)
    assert d0.to_list() == [[1], [2.0], ["3"]]


def test_fread_columns_list_bad1():
    with pytest.raises(ValueError) as e:
        dt.fread(text="C1,C2\n1,2\n3,4\n", columns=["C2"])
    assert ("Input contains 2 columns, whereas `columns` parameter "
            "specifies only 1 column" in str(e.value))


def test_fread_columns_list_bad2():
    with pytest.raises(TypeError):
        dt.fread(text="C1,C2\n1,2\n3,4\n", columns=["C1", 2])


def test_fread_columns_list_bad3():
    with pytest.raises(ValueError) as e:
        dt.fread(text="C1,C2\n1,2", columns=["C1", ("C2", bytes)])
    assert "Unknown type <class 'bytes'> used as an override for column 'C2'" \
           in str(e.value)


def test_fread_columns_list_bad4():
    src = 'A,B,C\n01,foo,3.140\n002,bar,6.28000\n'
    d0 = dt.fread(src, columns=[stype.str32, None, stype.float64])
    assert d0.names == ("A", "C")
    assert d0.to_list() == [["01", "002"], [3.14, 6.28]]
    with pytest.raises(RuntimeError) as e:
        dt.fread(src, columns=[str, float, float])
    assert "Attempt to override column 2 \"B\" with detected type 'Str32' " \
           in str(e.value)
    with pytest.raises(ValueError) as e:
        dt.fread(src, columns=[str, str])
    assert ("Input contains 3 columns, whereas `columns` parameter "
            "specifies only 2 columns" in str(e.value))


def test_fread_columns_list_bad5():
    src = 'A,B,C\n01,foo,3.140\n002,bar,6.28000\n'
    with pytest.raises(TypeError) as e:
        dt.fread(src, columns=list(range(3)))
    assert "Entry `columns[0]` has invalid type 'int'" in str(e.value)


def test_fread_columns_set1():
    text = ("C1,C2,C3,C4\n"
            "1,3.3,7,\"Alice\"\n"
            "2,,,\"Bob\"")
    d0 = dt.fread(text=text, columns={"C1", "C3"})
    frame_integrity_check(d0)
    assert d0.names == ("C1", "C3")
    assert d0.to_list() == [[1, 2], [7, None]]


def test_fread_columns_set2():
    with pytest.warns(DatatableWarning) as ws:
        d0 = dt.fread(text="A,B,A\n1,2,3\n", columns={"A"})
    assert d0.names == ("A", "A.0")
    assert d0.to_list() == [[1], [3]]
    assert len(ws) == 1
    assert ("Duplicate column name found, and was assigned a unique name: "
            "'A' -> 'A.0'" in ws[0].message.args[0])


def test_fread_columns_set_bad():
    with pytest.warns(FreadWarning) as ws:
        dt.fread(text="A,B,C\n1,2,3", columns={"A", "foo"})
    assert len(ws) == 1
    assert "Column(s) ['foo'] not found in the input" in ws[0].message.args[0]


def test_fread_columns_dict1():
    d0 = dt.fread(text="C1,C2,C3\n1,2,3\n4,5,6", columns={"C3": "A", "C1": "B"})
    assert d0.names == ("B", "C2", "A")
    assert d0.to_list() == [[1, 4], [2, 5], [3, 6]]


def test_fread_columns_dict2():
    d0 = dt.fread(text="A,B,C,D\n1,2,3,4", columns={"A": "a", ...: None})
    assert d0.names == ("a", )
    assert d0.to_list() == [[1]]


def test_fread_columns_dict3():
    d0 = dt.fread(text="A,B,C\n1,2,3",
                  columns={"A": ("foo", float), "B": str, "C": None})
    assert d0.names == ("foo", "B")
    assert d0.ltypes == (ltype.real, ltype.str)
    assert d0.to_list() == [[1.0], ["2"]]


def test_fread_columns_dict4():
    src = 'A,B,C\n01,foo,3.140\n002,bar,6.28000\n'
    d0 = dt.fread(src, columns={"C": str})
    assert d0.names == ("A", "B", "C")
    assert d0.ltypes == (ltype.int, ltype.str, ltype.str)
    assert d0.to_list() == [[1, 2], ["foo", "bar"], ["3.140", "6.28000"]]
    d1 = dt.fread(src, columns={"C": str, "A": float})
    assert d1.ltypes == (ltype.real, ltype.str, ltype.str)
    assert d1.to_list() == [[1, 2], ["foo", "bar"], ["3.140", "6.28000"]]
    d2 = dt.fread(src, columns={"C": str, "A": ltype.real})
    assert d2.ltypes == (ltype.real, ltype.str, ltype.str)
    assert d2.to_list() == [[1, 2], ["foo", "bar"], ["3.140", "6.28000"]]


def test_fread_columns_dict_reverse():
    src = 'A,B,C\n01,foo,3.140\n002,bar,6.28000\n'
    d0 = dt.fread(src, columns={str: "C", ltype.real: ["A"]})
    assert d0.ltypes == (ltype.real, ltype.str, ltype.str)
    assert d0.to_list() == [[1, 2], ["foo", "bar"], ["3.140", "6.28000"]]
    d1 = dt.fread(src, columns={str: slice(2, None), ltype.real: ["A"]})
    assert d1.ltypes == (ltype.real, ltype.str, ltype.str)
    assert d1.to_list() == [[1, 2], ["foo", "bar"], ["3.140", "6.28000"]]
    d2 = dt.fread(src, columns={str: slice(None)})
    assert d2.ltypes == (ltype.str, ltype.str, ltype.str)
    assert d2.to_list() == [["01", "002"], ["foo", "bar"],
                            ["3.140", "6.28000"]]


def test_fread_columns_type():
    src = 'A,B,C\n01,foo,3.140\n002,bar,6.28000\n'
    d0 = dt.fread(src, columns=str)
    assert d0.ltypes == (ltype.str, ltype.str, ltype.str)
    assert d0.to_list() == [["01", "002"], ["foo", "bar"],
                            ["3.140", "6.28000"]]


def test_fread_columns_fn1():
    text = ("A1,A2,A3,A4,A5,x16,b333\n"
            "5,4,3,2,1,0,0\n")
    d0 = dt.fread(text=text, columns=lambda cols: [int(col.name[1:]) % 2 == 0
                                                   for col in cols])
    frame_integrity_check(d0)
    assert d0.names == ("A2", "A4", "x16")
    assert d0.to_list() == [[4], [2], [0]]


def test_fread_columns_fn3():
    d0 = dt.fread('A,B\n"1","2"', columns=lambda cols: [int] * len(cols))
    d1 = dt.fread('A,B\n"1","2"', columns=lambda cols: [float] * len(cols))
    d2 = dt.fread('A,B\n"1","2"', columns=lambda cols: [str] * len(cols))
    frame_integrity_check(d0)
    frame_integrity_check(d1)
    frame_integrity_check(d2)
    assert d0.ltypes == (ltype.int, ltype.int)
    assert d1.ltypes == (ltype.real, ltype.real)
    assert d2.ltypes == (ltype.str, ltype.str)
    assert d0.to_list() == [[1], [2]]
    assert d1.to_list() == [[1.0], [2.0]]
    assert d2.to_list() == [["1"], ["2"]]


@pytest.mark.parametrize("columns", [None, dict()])
def test_fread_columns_empty(columns):
    # `None` column selector should select all columns
    d0 = dt.fread("A,B,C\n1,2,3", columns=columns)
    assert d0.shape == (1, 3)
    assert d0.names == ("A", "B", "C")
    assert d0.to_list() == [[1], [2], [3]]




#-------------------------------------------------------------------------------
# `sep`
#-------------------------------------------------------------------------------

def test_sep_comma():
    d0 = dt.fread("A,B,C\n1,2,3\n", sep=",")
    d1 = dt.fread("A,B,C\n1,2,3\n", sep=";")
    frame_integrity_check(d0)
    frame_integrity_check(d1)
    assert d0.shape == (1, 3)
    assert d1.shape == (1, 1)


def test_sep_newline():
    d0 = dt.fread("A,B,C\n1,2;3 ,5\n", sep="\n")
    d1 = dt.fread("A,B,C\n1,2;3 ,5\n", sep="")
    frame_integrity_check(d0)
    frame_integrity_check(d1)
    assert d0.shape == d1.shape == (1, 1)
    assert d0.names == d1.names == ("A,B,C",)
    assert d0.to_list() == d1.to_list() == [["1,2;3 ,5"]]


@pytest.mark.parametrize("sep", [None, ";", ",", "|", "\n", "*"])
def test_sep_selection(sep):
    src = "A;B;C|D,E\n1;3;4|5,6\n2;4;6|8,10\n"
    d0 = dt.fread(src, sep=sep)
    # It is not clear whether ';' should be the winning sep here. Comma (',') is
    # also a viable choice, since it has higher preference over ';'
    if sep is None:
        sep = ";"
    frame_integrity_check(d0)
    assert d0.names == tuple("A;B;C|D,E".split(sep))


def test_sep_invalid1():
    with pytest.raises(TypeError) as e:
        dt.fread("A,,B\n", sep=12)
    assert ("Parameter `sep` should be a string, instead got <class 'int'>"
            in str(e.value))


def test_sep_invalid2():
    with pytest.raises(Exception) as e:
        dt.fread("A,,B\n", sep=",,")
    assert ("Multi-character or unicode separators are not supported: ',,'"
            in str(e.value))


def test_sep_invalid3():
    with pytest.raises(Exception) as e:
        dt.fread("A,,B\n", sep="⌘")
    assert ("Multi-character or unicode separators are not supported: '⌘'"
            in str(e.value))


@pytest.mark.parametrize('c', list('019\'`"aAzZoQ'))
def test_sep_invalid4(c):
    with pytest.raises(Exception) as e:
        dt.fread("A,,B\n", sep=c)
    assert "Separator `%s` is not allowed" % c == str(e.value)



#-------------------------------------------------------------------------------
# `skip_blank_lines`
#-------------------------------------------------------------------------------

def test_fread_skip_blank_lines_true():
    inp = ("A,B\n"
           "1,2\n"
           "\n"
           "  \t \n"
           "3,4\n")
    d0 = dt.fread(text=inp, skip_blank_lines=True)
    frame_integrity_check(d0)
    assert d0.shape == (2, 2)
    assert d0.ltypes == (ltype.int, ltype.int)
    assert d0.to_list() == [[1, 3], [2, 4]]


@pytest.mark.skip("Issue #838")
def test_fread_skip_blank_lines_false():
    inp = "A,B\n1,2\n  \n\n3,4\n"
    with pytest.warns(DatatableWarning) as ws:
        d1 = dt.fread(text=inp, skip_blank_lines=False)
        frame_integrity_check(d1)
        assert d1.shape == (1, 2)
        assert d1.ltypes == (ltype.bool, ltype.int)
        assert d1.to_list() == [[True], [2]]
    assert len(ws) == 1
    assert ("Found the last consistent line but text exists afterwards"
            in ws[0].message.args[0])



#-------------------------------------------------------------------------------
# `strip_whitespace`
#-------------------------------------------------------------------------------

def test_fread_strip_whitespace():
    inp = ("A,B\n"
           "1,  c  \n"
           "3, d\n")
    d0 = dt.fread(text=inp, strip_whitespace=True)
    frame_integrity_check(d0)
    assert d0.shape == (2, 2)
    assert d0.ltypes == (ltype.int, ltype.str)
    assert d0.to_list() == [[1, 3], ["c", "d"]]
    d1 = dt.fread(text=inp, strip_whitespace=False)
    frame_integrity_check(d1)
    assert d1.shape == (2, 2)
    assert d1.ltypes == (ltype.int, ltype.str)
    assert d1.to_list() == [[1, 3], ["  c  ", " d"]]



#-------------------------------------------------------------------------------
# `quotechar`
#-------------------------------------------------------------------------------

def test_fread_quotechar():
    inp = "A,B\n'foo',1\n\"bar\",2\n`baz`,3\n"
    d0 = dt.fread(inp)  # default is quotechar='"'
    frame_integrity_check(d0)
    assert d0.to_list() == [["'foo'", "bar", "`baz`"], [1, 2, 3]]
    d1 = dt.fread(inp, quotechar="'")
    frame_integrity_check(d1)
    assert d1.to_list() == [["foo", '"bar"', "`baz`"], [1, 2, 3]]
    d2 = dt.fread(inp, quotechar="`")
    frame_integrity_check(d2)
    assert d2.to_list() == [["'foo'", '"bar"', "baz"], [1, 2, 3]]
    d3 = dt.fread(inp, quotechar="")
    frame_integrity_check(d3)
    assert d3.to_list() == [["'foo'", '"bar"', "`baz`"], [1, 2, 3]]
    d4 = dt.fread(inp, quotechar=None)
    frame_integrity_check(d4)
    assert d4.to_list() == [["'foo'", "bar", "`baz`"], [1, 2, 3]]


def test_fread_quotechar_bad():
    for c in "~!@#$%abcd*()-_+=^&:;{}[]\\|,.></?0123456789":
        with pytest.raises(ValueError) as e:
            dt.fread("A,B\n1,2", quotechar=c)
        assert "quotechar = (%s) is not allowed" % c in str(e.value)
    # Multi-character raises as well
    with pytest.raises(ValueError):
        dt.fread("A,B\n1,2", quotechar="''")
    with pytest.raises(ValueError):
        dt.fread("A,B\n1,2", quotechar="\0")



#-------------------------------------------------------------------------------
# `dec`
#-------------------------------------------------------------------------------

def test_fread_dec():
    inp = 'A,B\n1.000,"1,000"\n2.345,"5,432e+10"\n'
    d0 = dt.fread(inp)  # default is dec='.'
    frame_integrity_check(d0)
    assert d0.ltypes == (ltype.real, ltype.str)
    assert d0.to_list() == [[1.0, 2.345], ["1,000", "5,432e+10"]]
    d1 = dt.fread(inp, dec=",")
    frame_integrity_check(d1)
    assert d1.ltypes == (ltype.str, ltype.real)
    assert d1.to_list() == [["1.000", "2.345"], [1.0, 5.432e+10]]


def test_fread_dec_bad():
    for c in "~!@#$%abcdqerlkzABCZXE*()-_+=^&:;{}[]\\|></?0123456789'\"`":
        with pytest.raises(ValueError) as e:
            dt.fread("A,B\n1,2", dec=c)
        assert "Only dec='.' or ',' are allowed" in str(e.value)
    # Multi-character raises as well
    with pytest.raises(ValueError):
        dt.fread("A,B\n1,2", dec="..")
    with pytest.raises(ValueError):
        dt.fread("A,B\n1,2", dec="")



#-------------------------------------------------------------------------------
# `header`
#-------------------------------------------------------------------------------

def test_fread_header():
    """Check the effect of 'header' parameter."""
    inp = 'A,B\n1,2'
    d0 = dt.fread(inp, header=True)
    d1 = dt.fread(inp, header=False)
    frame_integrity_check(d0)
    frame_integrity_check(d1)
    assert d0.to_list() == [[1], [2]]
    assert d1.to_list() == [["A", "1"], ["B", "2"]]



#-------------------------------------------------------------------------------
# `skip_to_line/string`
#-------------------------------------------------------------------------------

def test_fread_skip_to_line():
    d0 = dt.fread("a,z\nv,1\nC,D\n1,2\n", skip_to_line=3)
    frame_integrity_check(d0)
    assert d0.names == ("C", "D")
    assert d0.to_list() == [[1], [2]]


def test_fread_skip_to_line_large():
    # Note: exception is not thrown, instead an empty Frame is returned
    d0 = dt.fread("a,b\n1,2\n3,4\n5,6\n", skip_to_line=1000)
    frame_integrity_check(d0)
    assert d0.shape == (0, 0)


def test_fread_skip_to_string():
    d0 = dt.fread("I, the Greatest King of All Times, do hereby proclaim\n"
                  "that, truly, I am infallible\n\n"
                  "A,B,C\n"
                  "1,2,3\n", skip_to_string=",B,")
    frame_integrity_check(d0)
    assert d0.names == ("A", "B", "C")
    assert d0.to_list() == [[1], [2], [3]]


def test_skip_to_string_bad():
    with pytest.raises(Exception) as e:
        dt.fread("A,B\n1,2", skip_to_string="bazinga!")
    assert ('skip_to_string = "bazinga!" was not found in the input'
            in str(e.value))



#-------------------------------------------------------------------------------
# `max_nrows`
#-------------------------------------------------------------------------------

def test_fread_max_nrows(capsys):
    d0 = dt.fread("A,B,C\n"
                  "1,foo,1\n"
                  "3,bar,0\n"
                  "5,baz,0\n"
                  "7,meh,1\n", max_nrows=2, verbose=True)
    out, err = capsys.readouterr()
    frame_integrity_check(d0)
    assert d0.names == ("A", "B", "C")
    assert d0.to_list() == [[1, 3], ["foo", "bar"], [True, False]]
    assert "Allocating 3 column slots with 2 rows" in out
    assert "Too few rows allocated" not in out
    assert "converting input string into bytes" in out


def test_fread_max_nrows_0rows():
    d0 = dt.fread("A\n", max_nrows=0)
    frame_integrity_check(d0)
    assert d0.names == ("A", )
    assert d0.shape == (0, 1)


@pytest.mark.xfail()
def test_fread_max_nrows_correct_types():
    d0 = dt.fread("A,B\n"
                  "1,2\n"
                  "3,4\n"
                  "5,6\n"
                  "foo,bar\n"
                  "zzzzz\n", max_nrows=3)
    assert d0.names == ("A", "B")
    assert d0.shape == (3, 2)
    assert d0.ltypes == (ltype.int, ltype.int)
    assert d0.to_list() == [[1, 3, 5], [2, 4, 6]]



#-------------------------------------------------------------------------------
# `logger`
#-------------------------------------------------------------------------------

def test_fread_logger():
    class MyLogger:
        def __init__(self):
            self.count = 0
            self.msg = ""

        def debug(self, msg):
            self.msg += msg + "\n"
            self.count += 1

    lg = MyLogger()
    dt.fread(text="A\n1\n2\n3", logger=lg)
    assert lg.count > 10
    assert "\n\n" not in lg.msg



#-------------------------------------------------------------------------------
# `nthreads`
#-------------------------------------------------------------------------------

def test_fread_nthreads(capsys):
    """Check that the `nthreads` parameter is respected"""
    dt.fread(text="A\n1\n2\n3", verbose=True, nthreads=1)
    out, err = capsys.readouterr()
    assert "Using 1 thread" in out
    assert not err



#-------------------------------------------------------------------------------
# `fill`
#-------------------------------------------------------------------------------

def test_fillna0():
    d0 = dt.fread("A,B,C\n"
                  "1,foo,bar\n"
                  "2,baz\n"
                  "3", fill=True)
    frame_integrity_check(d0)
    assert d0.to_list() == [[1, 2, 3],
                            ['foo', 'baz', None],
                            ['bar', None, None]]


def test_fillna1():
    src = ("Row,bool8,int32,int64,float32x,float64,float64+,float64x,str\n"
           "1,True,1234,1234567890987654321,0x1.123p-03,2.3,-inf,"
           "0x1.123456789abp+100,the end\n"
           "2\n"
           "3\n"
           "4\n"
           "5\n")
    d = dt.fread(text=src, fill=True)
    frame_integrity_check(d)
    p = d[1:, 1:].to_list()
    assert p == [[None] * 4] * 8


def test_fillna_and_skipblanklines():
    d0 = dt.fread("A,B\n"
                  "foo,2\n"
                  "\n"
                  "baz\n"
                  "bar,3\n", fill=True, skip_blank_lines=True)
    frame_integrity_check(d0)
    assert d0.to_list() == [["foo", "baz", "bar"], [2, None, 3]]



#-------------------------------------------------------------------------------
# `na_strings`
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("na", [" ", " NA", "A\t", "1\r"])
def test_nastrings_invalid(na):
    with pytest.raises(Exception) as e:
        dt.fread("A\n1", na_strings=[na])
    assert "has whitespace or control characters" in str(e.value)



#-------------------------------------------------------------------------------
# `anonymize`
#-------------------------------------------------------------------------------

def test_anonymize1(capsys):
    try:
        src = ("Якби ви знали, паничі,\n"
               "Де люде плачуть живучи,\n"
               "То ви б елегій не творили\n"
               "Та марне бога б не хвалили,\n"
               "На наші сльози сміючись.\n")
        d0 = dt.fread(src, sep="", verbose=True)
        assert d0
        out, err = capsys.readouterr()
        assert not err
        assert "На наші сльози сміючись." in out
        dt.options.fread.anonymize = True
        d1 = dt.fread(src, sep="", verbose=True)
        out, err = capsys.readouterr()
        assert not err
        assert "На наші сльози сміючись." not in out
        assert "UU UUUU UUUUUU UUUUUUUU." in out
        assert d1.shape == (4, 1)
    finally:
        dt.options.fread.anonymize = False


def test_anonymize2(capsys):
    try:
        lines = ["secret code"] + [str(j) for j in range(10000)]
        lines[111] = "boo"
        src = "\n".join(lines)
        d0 = dt.fread(src, verbose=True)
        assert d0
        out, err = capsys.readouterr()
        assert not err
        assert "secret code" in out
        assert "Column 1 (secret code)" in out
        dt.options.fread.anonymize = True
        d0 = dt.fread(src, verbose=True)
        out, err = capsys.readouterr()
        assert not err
        assert "Column 1 (secret code)" not in out
        assert "Column 1 (aaaaaa aaaa)" in out
    finally:
        dt.options.fread.anonymize = False
