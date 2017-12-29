#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
#-------------------------------------------------------------------------------
#-------------------------------------------------------------------------------
import datatable as dt
import os
import pytest
import random

root_env_name = "DT_LARGE_TESTS_ROOT"


#-------------------------------------------------------------------------------
alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

def random_string(n):
    """Return random string of `n` characters."""
    return "".join(random.choice(alphabet) for _ in range(n))


#-------------------------------------------------------------------------------

@pytest.mark.parametrize("src", ["", " ", "\n", " \n" * 3,
                                 "\t\n  \n\n        \t  ",
                                 "\uFEFF", "\uFEFF\n", "\uFEFF  \t\n \n\n",
                                 "\n\x1A\x1A", "\0", "\0\0\0\0", "\n\0",
                                 "\uFEFF  \n  \n\t\0\x1A\0\x1A"])
def test_empty(tempfile, src):
    with open(tempfile, "w", encoding="utf-8") as o:
        o.write(src)
    d0 = dt.fread(text=src)
    d1 = dt.fread(file=tempfile)
    assert d0.internal.check()
    assert d0.shape == (0, 0)
    assert d1.internal.check()
    assert d1.shape == (0, 0)


def test_html():
    with pytest.raises(Exception) as e:
        dt.fread("  \n  <!DOCTYPE html>\n"
                 "<html><body>HA!</body></html>", verbose=True)
    assert "<text> is an HTML file" in str(e)


def test_headers_only_input():
    d0 = dt.fread(text="A,B")
    assert d0.internal.check()
    assert d0.shape == (0, 2)
    d1 = dt.fread(text="AB\n")
    assert d1.internal.check()
    assert d1.shape == (0, 1)


# TODO: also test repl=None, which currently gets deserialized into empty
# strings.
@pytest.mark.parametrize("seed", [random.randint(0, 2**31)])
@pytest.mark.parametrize("repl", ["", "?"])
def test_empty_strings(seed, repl):
    random.seed(seed)
    ncols = random.randint(3, 10)
    nrows = int(random.expovariate(1 / 200) + 1)
    p = random.uniform(0.1, 0.5)
    src = []
    for i in range(ncols):
        src.append([(random_string(8) if random.random() < p else repl)
                    for j in range(nrows)])
        if all(t == repl for t in src[i]):
            src[i][0] = "!!!"
    colnames = list(alphabet[:ncols].upper())
    d0 = dt.DataTable(src, names=colnames)
    assert d0.names == tuple(colnames)
    assert d0.ltypes == (dt.ltype.str,) * ncols
    text = d0.to_csv()
    d1 = dt.fread(text=text)
    assert d1.internal.check()
    assert d1.names == d0.names
    assert d1.stypes == d0.stypes
    assert d1.topython() == src


def test_fread1():
    f = dt.fread("hello\n"
                 "1.1\n"
                 "200000\n"
                 "100.3")
    assert f.shape == (3, 1)
    assert f.names == ("hello", )
    assert f.topython() == [[1.1, 200000.0, 100.3]]


def test_fread2():
    f = dt.fread("""
        A,B,C,D
        1,2,3,4
        10,0,,7.2
        ,1,12,3.3333333333
        """)
    assert f.shape == (3, 4)
    assert f.names == ("A", "B", "C", "D")
    assert f.ltypes == (dt.ltype.int, dt.ltype.int, dt.ltype.int, dt.ltype.real)


def test_fread3():
    f = dt.fread("C\n12.345")
    assert f.internal.check()
    assert f.shape == (1, 1)
    assert f.names == ("C", )
    assert f.topython() == [[12.345]]


def test_utf16(tempfile):
    names = ("alpha", "beta", "gamma")
    col1 = [1.5, 12.999, -4e-6, 2.718281828]
    col2 = ["я", "не", "нездужаю", "нівроку"]
    col3 = [444, 5, -7, 0]
    srctxt = ("\uFEFF" +  # BOM
              ",".join(names) + "\n" +
              "\n".join('%r,"%s",%d' % row for row in zip(col1, col2, col3)))
    for encoding in ["utf-16le", "utf-16be"]:
        with open(tempfile, "wb") as o:
            srcbytes = srctxt.encode(encoding)
            assert srcbytes[0] + srcbytes[1] == 0xFE + 0xFF
            o.write(srcbytes)
        d0 = dt.fread(tempfile)
        assert d0.internal.check()
        assert d0.names == names
        assert d0.topython() == [col1, col2, col3]


def test_issue641():
    f = dt.fread("A,B,C\n"
                 "5,,\n"
                 "6,foo\rbar,z\n"
                 "7,bah,")
    assert f.internal.check()
    assert f.names == ("A", "B", "C")
    assert f.ltypes == (dt.ltype.int, dt.ltype.str, dt.ltype.str)
    assert f.topython() == [[5, 6, 7], ["", "foo\rbar", "bah"], ["", "z", ""]]


def test_issue643():
    d0 = dt.fread("A B\n1 2\n3 4 \n5 6\n6   7   ")
    assert d0.internal.check()
    assert d0.names == ("A", "B")
    assert d0.ltypes == (dt.ltype.int, dt.ltype.int)
    assert d0.topython() == [[1, 3, 5, 6], [2, 4, 6, 7]]


def test_space_separated():
    # Loosely based on #1113 in R
    txt = ("ITER    THETA1    THETA2   MCMC\n"
           "        -11000 -2.50000E+00  2.30000E+00    345678.20255 \n"
           "        -10999 -2.49853E+01  3.79270E+02    -195780.43911\n"
           "        -10998 1.95957E-01  4.16522E+00    7937.13048")
    d0 = dt.fread(txt)
    assert d0.internal.check()
    assert d0.names == ("ITER", "THETA1", "THETA2", "MCMC")
    assert d0.ltypes == (dt.ltype.int, dt.ltype.real, dt.ltype.real,
                         dt.ltype.real)
    assert d0.topython() == [[-11000, -10999, -10998],
                             [-2.5, -24.9853, 0.195957],
                             [2.3, 379.270, 4.16522],
                             [345678.20255, -195780.43911, 7937.13048]]


def test_quoted_comma():
    # Similar to #2839 in R
    inp = [["Abc", "def", '"gh,kl"', "mnopqrst"]] * 1000
    inp[111] = ["ain't", "this", "a", "surprise!"]
    txt = "A,B,C,D\n" + "\n".join(",".join(row) for row in inp)
    d0 = dt.fread(txt)
    assert d0.internal.check()
    assert d0.names == ("A", "B", "C", "D")
    assert d0.shape == (1000, 4)
    inp[111][2] = '"a"'
    assert d0.topython() == [[row[0] for row in inp],
                             [row[1] for row in inp],
                             [row[2][1:-1] for row in inp],  # unescape
                             [row[3] for row in inp]]


def test_1x1_na():
    d0 = dt.fread("A\n\n")
    assert d0.internal.check()
    assert d0.shape == (1, 1)
    assert d0.names == ("A", )
    assert d0.ltypes == (dt.ltype.bool, )
    assert d0.topython() == [[None]]


def test_last_quoted_field():
    d0 = dt.fread('A,B,C\n1,5,17\n3,9,"1000"')
    assert d0.internal.check()
    assert d0.shape == (2, 3)
    assert d0.ltypes == (dt.ltype.int, dt.ltype.int, dt.ltype.int)
    assert d0.topython() == [[1, 3], [5, 9], [17, 1000]]


def test_numbers_with_quotes1():
    d0 = dt.fread('B,C\n"12"  ,15\n"13"  ,18\n"14"  ,3')
    assert d0.internal.check()
    assert d0.shape == (3, 2)
    assert d0.ltypes == (dt.ltype.int, dt.ltype.int)
    assert d0.names == ("B", "C")
    assert d0.topython() == [[12, 13, 14], [15, 18, 3]]


def test_numbers_with_quotes2():
    d0 = dt.fread('A,B\n83  ,"23948"\n55  ,"20487203497"')
    assert d0.internal.check()
    assert d0.shape == (2, 2)
    assert d0.ltypes == (dt.ltype.int, dt.ltype.int)
    assert d0.names == ("A", "B")
    assert d0.topython() == [[83, 55], [23948, 20487203497]]


def test_unquoting1():
    d0 = dt.fread('"""A"""\n"hello, ""world"""\n""""\n"goodbye"\n')
    assert d0.internal.check()
    assert d0.names == ('"A"',)
    assert d0.topython() == [['hello, "world"', '"', 'goodbye']]


def test_unquoting2():
    d0 = dt.fread("B\nmorning\n'''night'''\n'day'\n'even''ing'\n",
                  quotechar="'")
    assert d0.internal.check()
    assert d0.names == ("B",)
    assert d0.topython() == [["morning", "'night'", "day", "even'ing"]]


def test_unescaping1():
    d0 = dt.fread('"C\\\\D"\n'
                  'AB\\x20CD\\n\n'
                  '"\\"one\\", \\\'two\\\', three"\n'
                  '"\\r\\t\\v\\a\\b\\071\\uABCD"\n')
    assert d0.internal.check()
    assert d0.names == ("C\\D",)
    assert d0.topython() == [["AB CD\n",
                              "\"one\", 'two', three",
                              "\r\t\v\a\b\071\uABCD"]]



#-------------------------------------------------------------------------------
# Omnibus Test
#-------------------------------------------------------------------------------

def make_seeds():
    # If you want to test a specific seed, uncomment the following line:
    # return [398276719]
    n = 25
    if os.environ.get(root_env_name, "") != "":
        n = 500
    return [random.randint(0, 2**31) for _ in range(n)]


@pytest.mark.parametrize("seed", make_seeds())
def test_fread_omnibus(seed):
    """
    Extensive method that attempts to test all possible scenarios for fread.
    """
    random.seed(seed)
    params = {}
    allparams = {}
    while True:
        ncols = random.choice([1, 1, 1, 2, 2, 3, 3, 3, 4, 5, 6, 7, 8, 128,
                               int(random.expovariate(1e-2))])
        nrows = random.choice([0, 1, 1, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 64,
                               128, 999, int(random.expovariate(1e-4))])
        if ncols * nrows < 100000:
            allparams["ncols"] = ncols
            allparams["nrows"] = nrows
            break
    quote = '"' if random.random() < 0.8 else "'"
    allparams["quote"] = quote
    if quote == "'":
        params["quotechar"] = quote
    prepared_data = [None] * ncols
    colnames = [None] * ncols
    coltypes = [None] * ncols
    for i in range(ncols):
        coltype = random.choice([dt.ltype.int, dt.ltype.str])
        if coltype == dt.ltype.int:
            coldata = generate_int_column(allparams)
        elif coltype == dt.ltype.str:
            coldata = generate_str_column(allparams)
        else:
            assert False, "Unknown coltype: %r" % coltype
        assert len(coldata) == nrows
        if coltype != dt.ltype.int and all(is_intlike(x) for x in coldata):
            coltype = dt.ltype.int
        # Check 'bool' last, since ['0', '1'] is both int-like and bool-like
        if coltype != dt.ltype.bool and all_boollike(coldata):
            coltype = dt.ltype.bool
        prepared_data[i] = coldata
        colnames[i] = "x%d" % i
        coltypes[i] = coltype
    sep = random.choice(",,,,,,,;;||\t\t:\x01\x02\x03\x04\x07")
    if sep in ":\x01\x02\x03\x04\x07" or random.random() < 0.2:
        params["sep"] = sep
    out = [sep.join(colnames)]
    for j in range(nrows):
        out.append(sep.join(prepared_data[i][j] for i in range(ncols)))
    if random.random() < 0.5 or (ncols == 1 and out[-1] == ""):
        # Determine whether trailing newline should be appended. Note that for
        # a single-column dataset with last element NA, the trailing newline
        # must be added (otherwise the last NA will not be recognized as data).
        out.append("")  # Ensures trailing newline
    nl = random.choice(["\n", "\n", "\r", "\r\n", "\n\r", "\r\r\n"])
    text = nl.join(out)
    if ncols == 0:
        # A datatable with 0 cols and N rows will serialize into N newlines,
        # which are read as an empty datatable (nrows = 0).
        nrows = 0

    try:
        params["text"] = text
        d0 = dt.fread(**params)
        assert d0.internal.check()
        assert d0.shape == (nrows, ncols)
        assert d0.names == tuple(colnames)
        if nrows:
            assert d0.ltypes == tuple(coltypes)
    except:
        with open("omnibus.csv", "w") as o:
            o.write(text)
        raise


def all_boollike(coldata):
    for x in coldata:
        x = x.strip()
        if len(x) >= 2 and x[0] == x[-1] and x[0] in "'\"`":
            x = x[1:-1]
        if len(x) > 5 or x.lower() not in {"true", "false", "0", "1", ""}:
            return False
    return True

def is_intlike(x):
    x = x.strip()
    return x.isdigit() or (len(x) > 2 and x[0] == x[-1] and x[0] in "'\"" and
                           x[1:-1].isdigit())


def generate_int_column(allparams):
    """
    Generate and return a column of random integers. The data will be drawn
    from one of the 5 possible ranges (tiny, small, medium, large or huge),
    and will usually have a certain amount of NAs. The integers may or may not
    be quoted, and also occastionally surrounded with whitespace.
    """
    quote = allparams["quote"]
    make_quoted = random.random() < 0.2
    rr = (lambda x: quote + str(x) + quote) if make_quoted else str
    if random.random() < 0.2:
        rr = (lambda x: quote + str(x) + quote + "  ") if make_quoted else \
             (lambda x: str(x) + "   ")

    # Determine the distribution of the data to generate
    weights = [0, 0, 0, 0, 0, 0]  # NA, tiny, small, medium, large, huge
    if random.random() < 0.8:
        weights[0] = random.random() ** 3
    if random.random() < 0.8:
        tt = random.random()
        weights[int(tt * 5) + 1] = 1
    else:
        for i in range(1, 6):
            weights[i] = 1
    thresholds = [sum(weights[:i]) / sum(weights) for i in range(1, 7)]

    nrows = allparams["nrows"]
    coldata = [None] * nrows
    for j in range(nrows):
        t = random.random()
        if t < thresholds[0]:    # NA
            coldata[j] = ""
        elif t < thresholds[1]:  # tiny
            coldata[j] = rr(random.randint(-127, 127))
        elif t < thresholds[2]:  # small
            coldata[j] = rr(random.randint(0, 65535))
        elif t < thresholds[3]:  # medium
            coldata[j] = rr(random.randint(-10000, 1000000))
        elif t < thresholds[4]:  # large
            coldata[j] = rr(random.randint(-2147483647, 2147483647))
        else:                    # huge
            coldata[j] = rr(random.randint(-9223372036854775807,
                                           9223372036854775807))
    return coldata


def generate_str_column(allparams):
    """
    Generate and return a column with random string data. This is the most
    versatile generator, and includes multiple different "modes" of generation.
    """
    nrows = allparams["nrows"]
    quote = allparams["quote"]
    always_quote = random.random() < 0.2
    rr = (lambda x: x)
    if always_quote:
        rr = (lambda x: quote + x + quote)
    rmode = random.random()
    if rmode < 0:
        pass
    else:
        # Generate simple alphanumeric strings
        return [rr(random_string(int(random.expovariate(0.01))))
                for _ in range(nrows)]
