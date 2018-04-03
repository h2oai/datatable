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
from datatable import ltype



#-------------------------------------------------------------------------------
# Tests for fread "source" arguments:
# the unnamed first argument, as well as `file`, `text`, `cmd`, `url`.
#-------------------------------------------------------------------------------

def test_fread_from_file1(tempfile):
    with open(tempfile, "w") as o:
        o.write("A,B\n1,2")
    d0 = dt.fread(tempfile)
    assert d0.internal.check()
    assert d0.names == ("A", "B")
    assert d0.topython() == [[1], [2]]


def test_fread_from_file2():
    with pytest.raises(ValueError):
        dt.fread(file="a,b\n1,2")


def test_fread_from_text1():
    d0 = dt.fread(text="A")
    assert d0.internal.check()
    assert d0.names == ("A",)
    assert d0.shape == (0, 1)


def test_fread_from_cmd1():
    d0 = dt.fread(cmd="ls -l")
    assert d0.internal.check()
    # It is difficult to assert anything about the contents or structure
    # of the resulting dataset...


def test_fread_from_cmd2():
    d0 = dt.fread(cmd="ls", header=False)
    assert d0.internal.check()
    assert d0.ncols == 1
    assert d0.nrows >= 15
    d1 = dt.fread(cmd="cat LICENSE", sep="\n")
    assert d1.internal.check()
    assert d1.nrows == 372


def test_fread_from_url1():
    with pytest.raises(ValueError) as e:
        dt.fread(url="A")
    assert "unknown url type" in str(e)


def test_fread_from_url2():
    path = os.path.abspath("LICENSE")
    d0 = dt.fread("file://" + path, sep="\n")
    assert d0.internal.check()
    assert d0.shape == (372, 1)


def test_fread_from_anysource_as_text1(capsys):
    src = "A\n" + "\n".join(str(i) for i in range(100))
    assert len(src) < 4096
    d0 = dt.fread(src, verbose=True)
    out, err = capsys.readouterr()
    assert d0.internal.check()
    assert ("Character 1 in the input is '\\n', treating input as raw text"
            in out)


def test_fread_from_anysource_as_text2(capsys):
    src = "A\n" + "\n".join(str(i) for i in range(1000, 2000))
    assert len(src) > 4096
    d0 = dt.fread(src, verbose=True)
    out, err = capsys.readouterr()
    assert d0.internal.check()
    assert ("Source has length %d characters, and will be treated as "
            "raw text" % len(src)) in out


def test_fread_from_anysource_as_text3(capsys):
    src = b"A,B,C\n1,2,3\n5,4,3\n"
    d0 = dt.fread(src, verbose=True)
    out, err = capsys.readouterr()
    assert d0.internal.check()
    assert d0.topython() == [[1, 5], [2, 4], [3, 3]]
    assert ("Character 5 in the input is '\\n', treating input as raw text"
            in out)


def test_fread_from_anysource_as_file1(tempfile, capsys):
    assert isinstance(tempfile, str)
    with open(tempfile, "w") as o:
        o.write("A,B\n1,2\n")
    d0 = dt.fread(tempfile, verbose=True)
    out, err = capsys.readouterr()
    assert d0.internal.check()
    assert "Input is assumed to be a file name" in out


def test_fread_from_anysource_as_file2(tempfile, py36):
    import pathlib
    with open(tempfile, "w") as o:
        o.write("A,B\n1,2\n")
    d0 = dt.fread(tempfile)
    d1 = dt.fread(tempfile.encode())
    d2 = dt.fread(pathlib.Path(tempfile))
    assert d0.internal.check()
    assert d1.internal.check()
    assert d2.internal.check()
    assert d0.topython() == d1.topython() == d2.topython()


def test_fread_from_anysource_filelike():
    class MyFile:
        def __init__(self):
            self.name = b"fake file"

        def fileno(self):
            return NotImplemented

        def read(self):
            return "A,B,C\na,b,c\n"

    d0 = dt.fread(MyFile())
    assert d0.internal.check()
    assert d0.names == ("A", "B", "C")
    assert d0.topython() == [["a"], ["b"], ["c"]]


def test_fread_from_anysource_as_url(tempfile, capsys):
    assert isinstance(tempfile, str)
    with open(tempfile, "w") as o:
        o.write("A,B\n1,2\n")
    d0 = dt.fread("file://" + os.path.abspath(tempfile), verbose=True)
    out, err = capsys.readouterr()
    assert d0.internal.check()
    assert "Input is a URL" in out


def test_fread_from_stringbuf():
    from io import StringIO
    s = StringIO("A,B,C\n1,2,3\n4,5,6")
    d0 = dt.fread(s)
    assert d0.internal.check()
    assert d0.names == ("A", "B", "C")
    assert d0.topython() == [[1, 4], [2, 5], [3, 6]]


def test_fread_from_fileobj(tempfile):
    with open(tempfile, "w") as f:
        f.write("A,B,C\nfoo,bar,baz\n")

    with open(tempfile, "r") as f:
        d0 = dt.fread(f)
        assert d0.internal.check()
        assert d0.names == ("A", "B", "C")
        assert d0.topython() == [["foo"], ["bar"], ["baz"]]


def test_fread_file_not_exists():
    name = "qerubvwpif8rAIB9845gb1_"
    path = os.path.abspath(".")
    with pytest.raises(ValueError) as e:
        dt.fread(name)
    assert ("File %s`/%s` does not exist" % (path, name)) in str(e)


def test_fread_file_is_directory():
    path = os.path.abspath(".")
    with pytest.raises(ValueError) as e:
        dt.fread(path)
    assert ("Path `%s` is not a file" % path) in str(e)


def test_fread_xz_file(tempfile, capsys):
    import lzma
    xzfile = tempfile + ".xz"
    with lzma.open(xzfile, "wb") as f:
        f.write(b"A\n1\n2\n3\n")
    d0 = dt.fread(xzfile, verbose=True)
    out, err = capsys.readouterr()
    assert d0.internal.check()
    assert d0.topython() == [[1, 2, 3]]
    assert ("Extracting %s into memory" % xzfile) in out
    os.unlink(xzfile)


def test_fread_gz_file(tempfile, capsys):
    import gzip
    gzfile = tempfile + ".gz"
    with gzip.open(gzfile, "wb") as f:
        f.write(b"A\n10\n20\n30\n")
    d0 = dt.fread(gzfile, verbose=True)
    out, err = capsys.readouterr()
    assert d0.internal.check()
    assert d0.topython() == [[10, 20, 30]]
    assert ("Extracting %s into memory" % gzfile) in out
    os.unlink(gzfile)


def test_fread_zip_file_1(tempfile, capsys):
    import zipfile
    zfname = tempfile + ".zip"
    with zipfile.ZipFile(zfname, "x", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("data1.csv", "a,b,c\n10,20,30\n5,7,12\n")
    d0 = dt.fread(zfname, verbose=True)
    out, err = capsys.readouterr()
    assert d0.internal.check()
    assert d0.names == ("a", "b", "c")
    assert d0.topython() == [[10, 5], [20, 7], [30, 12]]
    assert ("Extracting %s to temporary directory" % zfname) in out
    os.unlink(zfname)


def test_fread_zip_file_multi(tempfile):
    import zipfile
    zfname = tempfile + ".zip"
    with zipfile.ZipFile(zfname, "x", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("data1.csv", "a,b,c\nfoo,bar,baz\ngee,jou,sha\n")
        zf.writestr("data2.csv", "A,B,C\n3,4,5\n6,7,8\n")
        zf.writestr("data3.csv", "Aa,Bb,Cc\ntrue,1.5,\nfalse,1e+20,yay\n")
    with pytest.warns(UserWarning) as ws:
        d0 = dt.fread(zfname)
        d1 = dt.fread(zfname + "/data2.csv")
        d2 = dt.fread(zfname + "/data3.csv")
    assert d0.internal.check()
    assert d1.internal.check()
    assert d2.internal.check()
    assert d0.names == ("a", "b", "c")
    assert d1.names == ("A", "B", "C")
    assert d2.names == ("Aa", "Bb", "Cc")
    assert d0.topython() == [["foo", "gee"], ["bar", "jou"], ["baz", "sha"]]
    assert d1.topython() == [[3, 6], [4, 7], [5, 8]]
    assert d2.topython() == [[True, False], [1.5, 1e20], ["", "yay"]]
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
    assert ("Zip file %s is empty" % zfname) in str(e)
    os.unlink(zfname)


def test_fread_zip_file_bad2(tempfile):
    import zipfile
    zfname = tempfile + ".zip"
    with zipfile.ZipFile(zfname, "x") as zf:
        zf.writestr("data1.csv", "Egeustimentis")
    with pytest.raises(ValueError) as e:
        dt.fread(zfname + "/out.csv")
    assert "File `out.csv` does not exist in archive" in str(e)
    os.unlink(zfname)


def test_fread_bad_source_none():
    with pytest.raises(ValueError) as e:
        dt.fread()
    assert "No input source" in str(e)


def test_fread_bad_source_any_and_source():
    with pytest.raises(ValueError) as e:
        dt.fread("a", text="b")
    assert "When an unnamed argument is passed, it is invalid to also " \
           "provide the `text` parameter" in str(e)


def test_fread_bad_source_2sources():
    with pytest.raises(ValueError) as e:
        dt.fread(file="a", text="b")
    assert "Both parameters `file` and `text` cannot be passed to fread " \
           "simultaneously" in str(e)


def test_fread_bad_source_anysource():
    with pytest.raises(TypeError) as e:
        dt.fread(12345)
    assert "Unknown type for the first argument in fread" in str(e)


def test_fread_bad_source_text():
    with pytest.raises(TypeError) as e:
        dt.fread(text=["a", "b", "c"])
    assert "Invalid parameter `text` in fread: expected str or bytes" in str(e)


def test_fread_bad_source_file():
    with pytest.raises(TypeError) as e:
        dt.fread(file=TypeError)
    assert ("Invalid parameter `file` in fread: expected a str/bytes/PathLike"
            in str(e))


def test_fread_bad_source_cmd():
    with pytest.raises(TypeError) as e:
        dt.fread(cmd=["ls", "-l", ".."])
    assert "Invalid parameter `cmd` in fread: expected str" in str(e)



#-------------------------------------------------------------------------------
# `columns`
#-------------------------------------------------------------------------------

def test_fread_columns_slice():
    d0 = dt.fread(text="A,B,C,D,E\n1,2,3,4,5", columns=slice(None, None, 2))
    assert d0.internal.check()
    assert d0.names == ("A", "C", "E")
    assert d0.topython() == [[1], [3], [5]]


def test_fread_columns_range():
    d0 = dt.fread(text="A,B,C,D,E\n1,2,3,4,5", columns=range(3))
    assert d0.internal.check()
    assert d0.names == ("A", "B", "C")
    assert d0.topython() == [[1], [2], [3]]


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
    assert d0.internal.check()
    assert d0.names == ("foo", "bar", "baz")
    assert d0.topython() == [[1], [2], [3]]


def test_fread_columns_list2():
    d0 = dt.fread(text="A,B,C\n1,2,3", columns=["foo", None, "baz"])
    assert d0.internal.check()
    assert d0.names == ("foo", "baz")
    assert d0.topython() == [[1], [3]]


def test_fread_columns_list3():
    d0 = dt.fread(text="A,B,C\n1,2,3", columns=[("foo", str), None, None])
    assert d0.internal.check()
    assert d0.names == ("foo", )
    assert d0.topython() == [["1"]]


def test_fread_columns_list_bad1():
    with pytest.raises(ValueError) as e:
        dt.fread(text="C1,C2\n1,2\n3,4\n", columns=["C2"])
    assert ("Input file contains 2 columns, whereas `columns` parameter "
            "specifies only 1 column" in str(e.value))


def test_fread_columns_list_bad2():
    with pytest.raises(TypeError):
        dt.fread(text="C1,C2\n1,2\n3,4\n", columns=["C1", 2])


def test_fread_columns_list_bad3():
    with pytest.raises(ValueError) as e:
        dt.fread(text="C1,C2\n1,2", columns=["C1", ("C2", bytes)])
    assert "Unknown type <class 'bytes'> used as an override for column 'C2'" \
           in str(e)


def test_fread_columns_set1():
    text = ("C1,C2,C3,C4\n"
            "1,3.3,7,\"Alice\"\n"
            "2,,,\"Bob\"")
    d0 = dt.fread(text=text, columns={"C1", "C3"})
    assert d0.internal.check()
    assert d0.names == ("C1", "C3")
    assert d0.topython() == [[1, 2], [7, None]]


def test_fread_columns_set2():
    with pytest.warns(UserWarning) as ws:
        d0 = dt.fread(text="A,B,A\n1,2,3\n", columns={"A"})
    assert d0.names == ("A", "A.1")
    assert d0.topython() == [[1], [3]]
    assert len(ws) == 1
    assert "Duplicate column names found: ['A']" in ws[0].message.args[0]


def test_fread_columns_set_bad():
    with pytest.warns(UserWarning) as ws:
        dt.fread(text="A,B,C\n1,2,3", columns={"A", "foo"})
    assert len(ws) == 1
    assert "Column(s) ['foo'] not found in the input" in ws[0].message.args[0]


def test_fread_columns_dict1():
    d0 = dt.fread(text="C1,C2,C3\n1,2,3\n4,5,6", columns={"C3": "A", "C1": "B"})
    assert d0.names == ("B", "C2", "A")
    assert d0.topython() == [[1, 4], [2, 5], [3, 6]]


def test_fread_columns_dict2():
    d0 = dt.fread(text="A,B,C,D\n1,2,3,4", columns={"A": "a", ...: None})
    assert d0.names == ("a", )
    assert d0.topython() == [[1]]


def test_fread_columns_dict3():
    d0 = dt.fread(text="A,B,C\n1,2,3",
                  columns={"A": ("foo", float), "B": (..., str), "C": None})
    assert d0.names == ("foo", "B")
    assert d0.ltypes == (ltype.real, ltype.str)
    assert d0.topython() == [[1.0], ["2"]]


def test_fread_columns_fn1():
    text = ("A1,A2,A3,A4,A5,x16,b333\n"
            "5,4,3,2,1,0,0\n")
    d0 = dt.fread(text=text, columns=lambda col: int(col[1:]) % 2 == 0)
    assert d0.internal.check()
    assert d0.names == ("A2", "A4", "x16")
    assert d0.topython() == [[4], [2], [0]]


def test_fread_columns_fn3():
    d0 = dt.fread('A,B\n"1","2"', columns=lambda i, name, type: (name, int))
    d1 = dt.fread('A,B\n"1","2"', columns=lambda i, name, type: (name, float))
    d2 = dt.fread('A,B\n"1","2"', columns=lambda i, name, type: (name, str))
    assert d0.internal.check()
    assert d1.internal.check()
    assert d2.internal.check()
    assert d0.ltypes == (ltype.int, ltype.int)
    assert d1.ltypes == (ltype.real, ltype.real)
    assert d2.ltypes == (ltype.str, ltype.str)
    assert d0.topython() == [[1], [2]]
    assert d1.topython() == [[1.0], [2.0]]
    assert d2.topython() == [["1"], ["2"]]


@pytest.mark.parametrize("columns", [None, list(), set(), dict()])
def test_fread_columns_empty(columns):
    # empty column selector should select all columns
    d0 = dt.fread("A,B,C\n1,2,3", columns=columns)
    assert d0.shape == (1, 3)
    assert d0.names == ("A", "B", "C")
    assert d0.topython() == [[1], [2], [3]]



#-------------------------------------------------------------------------------
# `sep`
#-------------------------------------------------------------------------------

def test_sep_comma():
    d0 = dt.fread("A,B,C\n1,2,3\n", sep=",")
    d1 = dt.fread("A,B,C\n1,2,3\n", sep=";")
    assert d0.internal.check()
    assert d1.internal.check()
    assert d0.shape == (1, 3)
    assert d1.shape == (1, 1)


def test_sep_newline():
    d0 = dt.fread("A,B,C\n1,2;3 ,5\n", sep="\n")
    d1 = dt.fread("A,B,C\n1,2;3 ,5\n", sep="")
    assert d0.internal.check()
    assert d1.internal.check()
    assert d0.shape == d1.shape == (1, 1)
    assert d0.names == d1.names == ("A,B,C",)
    assert d0.topython() == d1.topython() == [["1,2;3 ,5"]]


def test_sep_invalid():
    with pytest.raises(TypeError) as e:
        dt.fread("A,,B\n", sep=12)
    assert ("Parameter `sep` of type `Optional[str]` received value 12 "
            "of type int" in str(e))
    with pytest.raises(Exception) as e:
        dt.fread("A,,B\n", sep=",,")
    assert "Multi-character separator ',,' not supported" in str(e)
    with pytest.raises(Exception) as e:
        dt.fread("A,,B\n", sep="⌘")
    assert "The separator should be an ASCII character, got '⌘'" in str(e)



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
    assert d0.internal.check()
    assert d0.shape == (2, 2)
    assert d0.ltypes == (ltype.int, ltype.int)
    assert d0.topython() == [[1, 3], [2, 4]]


def test_fread_skip_blank_lines_false():
    inp = "A,B\n1,2\n  \n\n3,4\n"
    with pytest.warns(UserWarning) as ws:
        d1 = dt.fread(text=inp, skip_blank_lines=False)
        assert d1.internal.check()
        assert d1.shape == (1, 2)
        assert d1.ltypes == (ltype.bool, ltype.int)
        assert d1.topython() == [[True], [2]]
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
    assert d0.internal.check()
    assert d0.shape == (2, 2)
    assert d0.ltypes == (ltype.int, ltype.str)
    assert d0.topython() == [[1, 3], ["c", "d"]]
    d1 = dt.fread(text=inp, strip_whitespace=False)
    assert d1.internal.check()
    assert d1.shape == (2, 2)
    assert d1.ltypes == (ltype.int, ltype.str)
    assert d1.topython() == [[1, 3], ["  c  ", " d"]]



#-------------------------------------------------------------------------------
# `quotechar`
#-------------------------------------------------------------------------------

def test_fread_quotechar():
    inp = "A,B\n'foo',1\n\"bar\",2\n`baz`,3\n"
    d0 = dt.fread(inp)  # default is quotechar='"'
    assert d0.internal.check()
    assert d0.topython() == [["'foo'", "bar", "`baz`"], [1, 2, 3]]
    d1 = dt.fread(inp, quotechar="'")
    assert d1.internal.check()
    assert d1.topython() == [["foo", '"bar"', "`baz`"], [1, 2, 3]]
    d2 = dt.fread(inp, quotechar="`")
    assert d2.internal.check()
    assert d2.topython() == [["'foo'", '"bar"', "baz"], [1, 2, 3]]
    d3 = dt.fread(inp, quotechar="")
    assert d3.internal.check()
    assert d3.topython() == [["'foo'", '"bar"', "`baz`"], [1, 2, 3]]
    d4 = dt.fread(inp, quotechar=None)
    assert d4.internal.check()
    assert d4.topython() == [["'foo'", "bar", "`baz`"], [1, 2, 3]]


def test_fread_quotechar_bad():
    for c in "~!@#$%abcd*()-_+=^&:;{}[]\\|,.></?0123456789":
        with pytest.raises(ValueError) as e:
            dt.fread("A,B\n1,2", quotechar=c)
        assert "quotechar should be one of [\"'`] or '' or None" in str(e.value)
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
    assert d0.internal.check()
    assert d0.ltypes == (ltype.real, ltype.str)
    assert d0.topython() == [[1.0, 2.345], ["1,000", "5,432e+10"]]
    d1 = dt.fread(inp, dec=",")
    assert d1.internal.check()
    assert d1.ltypes == (ltype.str, ltype.real)
    assert d1.topython() == [["1.000", "2.345"], [1.0, 5.432e+10]]


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
    assert d0.internal.check()
    assert d1.internal.check()
    assert d0.topython() == [[1], [2]]
    assert d1.topython() == [["A", "1"], ["B", "2"]]



#-------------------------------------------------------------------------------
# `skip_to_line/string`
#-------------------------------------------------------------------------------

def test_fread_skip_to_line():
    d0 = dt.fread("a,z\nv,1\nC,D\n1,2\n", skip_to_line=3)
    assert d0.internal.check()
    assert d0.names == ("C", "D")
    assert d0.topython() == [[1], [2]]


def test_fread_skip_to_line_large():
    # Note: exception is not thrown, instead an empty Frame is returned
    d0 = dt.fread("a,b\n1,2\n3,4\n5,6\n", skip_to_line=1000)
    assert d0.internal.check()
    assert d0.shape == (0, 0)


def test_fread_skip_to_string():
    d0 = dt.fread("I, the Greatest King of All Times, do hereby proclaim\n"
                  "that, truly, I am infallible\n\n"
                  "A,B,C\n"
                  "1,2,3\n", skip_to_string=",B,")
    assert d0.internal.check()
    assert d0.names == ("A", "B", "C")
    assert d0.topython() == [[1], [2], [3]]


def test_skip_to_string_bad():
    with pytest.raises(Exception) as e:
        dt.fread("A,B\n1,2", skip_to_string="bazinga!")
    assert 'skip_to_string = "bazinga!" was not found in the input' in str(e)



#-------------------------------------------------------------------------------
# `max_nrows`
#-------------------------------------------------------------------------------

def test_fread_max_nrows():
    d0 = dt.fread("A,B,C\n"
                  "1,foo,1\n"
                  "3,bar,0\n"
                  "5,baz,0\n"
                  "7,meh,1\n", max_nrows=2)
    assert d0.internal.check()
    assert d0.names == ("A", "B", "C")
    assert d0.topython() == [[1, 3], ["foo", "bar"], [True, False]]


def test_fread_max_nrows_0rows():
    d0 = dt.fread("A\n", max_nrows=0)
    assert d0.internal.check()
    assert d0.names == ("A", )
    assert d0.shape == (0, 1)



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



#-------------------------------------------------------------------------------
# `fill`
#-------------------------------------------------------------------------------

def test_fillna0():
    d0 = dt.fread("A,B,C\n"
                  "1,foo,bar\n"
                  "2,baz\n"
                  "3", fill=True)
    assert d0.internal.check()
    assert d0.topython() == [[1, 2, 3],
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
    assert d.internal.check()
    p = d[1:, 1:].topython()
    assert p == [[None] * 4] * 8


def test_fillna_and_skipblanklines():
    d0 = dt.fread("A,B\n"
                  "foo,2\n"
                  "\n"
                  "baz\n"
                  "bar,3\n", fill=True, skip_blank_lines=True)
    assert d0.internal.check()
    assert d0.topython() == [["foo", "baz", "bar"], [2, None, 3]]



#-------------------------------------------------------------------------------
# `na_strings`
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("na", [" ", " NA", "A\t", "1\r"])
def test_nastrings_invalid(na):
    with pytest.raises(Exception) as e:
        dt.fread("A\n1", na_strings=[na])
    assert "has whitespace or control characters" in str(e)
