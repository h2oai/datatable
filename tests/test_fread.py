#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import pytest
import datatable as dt
import os
import random
import math
from datatable import stype, ltype



#-------------------------------------------------------------------------------
# Tests for fread "source" arguments
#-------------------------------------------------------------------------------

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


def test_fread_from_anysource_as_file2(tempfile):
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
# Tests for the `columns` argument
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
    assert d0.ltypes == (dt.ltype.real, dt.ltype.str)
    assert d0.topython() == [[1.0], ["2"]]


def test_fread_columns_fn1():
    text = ("A1,A2,A3,A4,A5,x16,b333\n"
            "5,4,3,2,1,0,0\n")
    d0 = dt.fread(text=text, columns=lambda col: int(col[1:]) % 2 == 0)
    assert d0.internal.check()
    assert d0.names == ("A2", "A4", "x16")
    assert d0.topython() == [[4], [2], [0]]


def test_select_some_columns():
    # * Last field of last line contains separator
    # * The file doesn't end with \n
    # * Only subset of columns is requested
    f = dt.fread('A,B,C\n1,2,"a,b"', columns={'A', 'B'})
    assert f.internal.check()
    assert f.names == ("A", "B")
    assert f.topython() == [[1], [2]]



#-------------------------------------------------------------------------------
# Other parameters
#-------------------------------------------------------------------------------

def test_fread_skip_blank_lines():
    inp = ("A,B\n"
           "1,2\n"
           "\n"
           "3,4\n")
    d0 = dt.fread(text=inp, skip_blank_lines=True)
    assert d0.internal.check()
    assert d0.shape == (2, 2)
    assert d0.ltypes == (dt.ltype.int, dt.ltype.int)
    assert d0.topython() == [[1, 3], [2, 4]]

    with pytest.warns(RuntimeWarning) as ws:
        d1 = dt.fread(text=inp, skip_blank_lines=False)
        assert d1.internal.check()
        assert d1.shape == (1, 2)
        assert d1.ltypes == (dt.ltype.int, dt.ltype.int)
        assert d1.topython() == [[1], [2]]
    assert len(ws) == 1
    assert ("Found the last consistent line but text exists afterwards"
            in ws[0].message.args[0])


def test_fread_strip_white():
    inp = ("A,B\n"
           "1,  c  \n"
           "3, d\n")
    d0 = dt.fread(text=inp, strip_white=True)
    assert d0.internal.check()
    assert d0.shape == (2, 2)
    assert d0.ltypes == (dt.ltype.int, dt.ltype.str)
    assert d0.topython() == [[1, 3], ["c", "d"]]
    d1 = dt.fread(text=inp, strip_white=False)
    assert d1.internal.check()
    assert d1.shape == (2, 2)
    assert d1.ltypes == (dt.ltype.int, dt.ltype.str)
    assert d1.topython() == [[1, 3], ["  c  ", " d"]]


def test_fread_quotechar():
    inp = "A,B\n'foo',1\n\"bar\",2\n`baz`,3\n"
    d0 = dt.fread(inp)  # default is quotechar='"'
    assert d0.internal.check()
    assert d0.topython() == [["'foo'", "bar", "`baz`"], [1, 2, 3]]
    d1 = dt.fread(inp, quotechar="'")
    assert d1.internal.check()
    assert d1.topython() == [["foo", '"bar"', "`baz`"], [1, 2, 3]]
    d1 = dt.fread(inp, quotechar="`")
    assert d1.internal.check()
    assert d1.topython() == [["'foo'", '"bar"', "baz"], [1, 2, 3]]
    d1 = dt.fread(inp, quotechar=None)
    assert d1.internal.check()
    assert d1.topython() == [["'foo'", '"bar"', "`baz`"], [1, 2, 3]]


def test_fread_quotechar_bad():
    for c in "~!@#$%abcd*()-_+=^&:;{}[]\\|,.></?0123456789":
        with pytest.raises(ValueError) as e:
            dt.fread("A,B\n1,2", quotechar=c)
        assert "quotechar should be one of [\"'`] or None" in str(e.value)
    # Multi-character raises as well
    with pytest.raises(ValueError):
        dt.fread("A,B\n1,2", quotechar="''")
    with pytest.raises(ValueError):
        dt.fread("A,B\n1,2", quotechar="")


def test_fread_dec():
    inp = 'A,B\n1.000,"1,000"\n2.345,"5,432e+10"\n'
    d0 = dt.fread(inp)  # default is dec='.'
    assert d0.internal.check()
    assert d0.ltypes == (dt.ltype.real, dt.ltype.str)
    assert d0.topython() == [[1.0, 2.345], ["1,000", "5,432e+10"]]
    d1 = dt.fread(inp, dec=",")
    assert d1.internal.check()
    assert d1.ltypes == (dt.ltype.str, dt.ltype.real)
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
# Misc
#-------------------------------------------------------------------------------

def test_fread_hex():
    rnd = random.random
    arr = [rnd() * 10**(10**rnd()) for i in range(20)]
    inp = "A\n%s\n" % "\n".join(x.hex() for x in arr)
    d0 = dt.fread(text=inp)
    assert d0.internal.check()
    assert d0.ltypes == (ltype.real, )
    assert d0.topython() == [arr]


def test_fread_hex0():
    d0 = dt.fread(text="A\n0x0.0p+0\n0x0p0\n-0x0p-0\n")
    assert d0.topython() == [[0.0] * 3]
    assert math.copysign(1.0, d0.topython()[0][2]) == -1.0


def test_fread_float():
    inp = "A\n0x0p0\n0x1.5p0\n0x1.5p-1\n0x1.2AAAAAp+22"
    d0 = dt.fread(text=inp)
    assert d0.internal.check()
    assert d0.stypes == (stype.float32, )
    assert d0.topython() == [[0, 1.3125, 0.65625, 4893354.5]]


def test_fread_issue527():
    """
    Test handling of invalid UTF-8 characters: right now they are decoded
    using Windows-1252 code page (this is better than throwing an exception).
    """
    inp = b"A,B\xFF,C\n1,2,3\xAA\n"
    d0 = dt.fread(text=inp)
    assert d0.internal.check()
    assert d0.names == ("A", "Bÿ", "C")
    assert d0.topython() == [[1], [2], ["3ª"]]


def test_fread_issue594():
    """
    Test handling of characters that are both invalid UTF-8 and invalid
    Win-1252, when they appear as a column header.
    """
    bad = (b'\x7f'
           b'\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f'
           b'\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f'
           b'\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf'
           b'\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf'
           b'\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf'
           b'\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf'
           b'\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef'
           b'\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff')
    inp = (b'A,"' + bad + b'"\n2,foo\n')
    d0 = dt.fread(text=inp)
    assert d0.internal.check()
    assert d0.shape == (1, 2)
    assert d0.names == ("A", bad.decode("windows-1252", "replace"))


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


def test_fread_nthreads(capsys):
    """Check that the `nthreads` parameter is respected"""
    dt.fread(text="A\n1\n2\n3", verbose=True, nthreads=1)
    out, err = capsys.readouterr()
    assert "Using 1 thread" in out


def test_fread_fillna():
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


def test_fread_CtrlZ():
    """Check that Ctrl+Z characters at the end of the file are removed"""
    src = b"A,B,C\n1,2,3\x1A\x1A"
    d0 = dt.fread(text=src)
    assert d0.internal.check()
    assert d0.ltypes == (dt.ltype.int, dt.ltype.int, dt.ltype.int)
    assert d0.topython() == [[1], [2], [3]]


def test_fread_NUL():
    """Check that NUL characters at the end of the file are removed"""
    d0 = dt.fread(text=b"A,B\n2.3,5\0\0\0\0\0\0\0\0\0\0")
    assert d0.internal.check()
    assert d0.ltypes == (dt.ltype.real, dt.ltype.int)
    assert d0.topython() == [[2.3], [5]]


def test_fread_1col():
    """Check that it is possible to read 1-column file witn NAs."""
    d0 = dt.fread(text="A\n1\n2\n\n4\n\n5\n\n")
    assert d0.internal.check()
    assert d0.names == ("A",)
    assert d0.topython() == [[1, 2, None, 4, None, 5, None]]
    d1 = dt.fread("QUOTE\n"
                  "If you think\n"
                  "you can do it,\n\n"
                  "you can.\n\n", sep="\n")
    assert d1.internal.check()
    assert d1.names == ("QUOTE",)
    assert d1.topython() == [["If you think", "you can do it,", "",
                              "you can.", ""]]


def test_fread_line_endings():
    entries = ["A", "", "1", "2", "3"]
    for eol in ["\n", "\r", "\n\r", "\r\n", "\r\r\n"]:
        text = eol.join(entries)
        d0 = dt.fread(text=text)
        assert d0.internal.check()
        assert d0.names == ("A",)
        assert d0.topython() == [[None, 1, 2, 3]]


def test_fread_with_whitespace():
    """
    Check that whitespace at the ends of lines is ignored, even if sep=' ' is
    detected. See issue #606
    """
    d0 = dt.fread(text="A\n23     ")
    assert d0.internal.check()
    assert d0.names == ("A",)
    assert d0.topython() == [[23]]
    d1 = dt.fread("A B C \n10 11 12 \n")
    assert d1.internal.check()
    assert d1.names == ("A", "B", "C")
    assert d1.topython() == [[10], [11], [12]]
    d2 = dt.fread("a  b  c\nfoo  bar  baz\noof  bam  \nnah  aye  l8r")
    assert d2.internal.check()
    assert d2.names == ("a", "b", "c")
    assert d2.topython() == [["foo", "oof", "nah"],
                             ["bar", "bam", "aye"],
                             ["baz", None,  "l8r"]]  # should this be ""?


def test_fread_cmd():
    d0 = dt.fread(cmd="ls", header=False)
    assert d0.internal.check()
    assert d0.ncols == 1
    assert d0.nrows >= 15
    d1 = dt.fread(cmd="cat LICENSE", sep="\n")
    assert d1.internal.check()
    assert d1.nrows == 200


def test_fread_url():
    path = os.path.abspath("LICENSE")
    d0 = dt.fread("file://" + path, sep="\n")
    assert d0.internal.check()
    assert d0.shape == (200, 1)


def test_fread_issue615():
    d0 = dt.fread("A,B,C,D,E,F,G,H,I\n"
                  "NaNaNa,Infinity-3,nanny,0x1.5p+12@boo,23ba,2.5e-4q,"
                  "Truely,Falsely,1\n")
    assert d0.internal.check()
    assert d0.topython() == [["NaNaNa"], ["Infinity-3"], ["nanny"],
                             ["0x1.5p+12@boo"], ["23ba"], ["2.5e-4q"],
                             ["Truely"], ["Falsely"], [1]]


def test_fread_issue628():
    """Similar to #594 but read in verbose mode."""
    d0 = dt.fread(b"a,\x80\n11,2\n", verbose=True)
    assert d0.internal.check()
    assert d0.topython() == [[11], [2]]
    # The interpretation of byte \x80 as symbol € is not set in stone: we may
    # alter it in the future, or make it platform-dependent?
    assert d0.names == ("a", "€")
