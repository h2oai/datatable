#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
# The "Omnibus" fread test. Here we attempt to randomly generate input files of
# varying structure/complexity and then read them with fread. The idea is to
# attempt all possible parameters combinations, and check whether they can work
# together.
#
# This is still work-in-progress and tests only a small subset of all possible
# cases, however it already uncovered multiple non-trivial bugs in fread.
#-------------------------------------------------------------------------------
import datatable as dt
import os
import pytest
import random
from datatable.internal import frame_integrity_check
from tests import random_string

root_env_name = "DT_LARGE_TESTS_ROOT"
env_coverage = "DTCOVERAGE"



#-------------------------------------------------------------------------------
# The test
#-------------------------------------------------------------------------------

def make_seeds():
    # If you want to test a specific seed, uncomment the following line:
    # return [398276719]
    n = 25
    if (os.environ.get(root_env_name, "") != "" and
            os.environ.get(env_coverage, "") == ""):
        n = 100
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
        if (coltype not in [dt.ltype.real, dt.ltype.int] and
                all(is_reallike(x) for x in coldata)):
            coltype = dt.ltype.real
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
    # Append extra newlines which should be ignored
    if ncols > 1 and random.random() < 0.4:
        x = 0
        while x < 0.8:
            x = random.random()
            out.append("" if x < 0.5 else " " * int((x - 0.5) * 100))
    nl = random.choice(["\n", "\n", "\r", "\r\n", "\n\r", "\r\r\n"])
    text = nl.join(out)
    # A datatable with 0 cols and N rows will serialize into N newlines,
    # which are read as an empty datatable (nrows = 0).
    if ncols == 0:
        nrows = 0

    d0 = None
    try:
        params["text"] = text
        d0 = dt.fread(**params)
        frame_integrity_check(d0)
        assert d0.shape == (nrows, ncols)
        assert d0.names == tuple(colnames)
        if nrows:
            assert d0.ltypes == tuple(coltypes)
    except:
        with open("omnibus.csv", "w") as o:
            o.write(text)
        with open("omnibus.params", "w") as o:
            del params["text"]
            o.write("params = %r\n\n"
                    "exp_shape = (%d, %d)\n"
                    "exp_names = %r\n"
                    "exp_types = %r\n"
                    % (params, nrows, ncols, tuple(colnames), tuple(coltypes)))
            if d0:
                o.write("act_shape = %r\n"
                        "act_names = %r\n"
                        "act_types = %r\n"
                        % (d0.shape, d0.names, d0.ltypes))
        raise



#-------------------------------------------------------------------------------
# Helper functions
#-------------------------------------------------------------------------------

def all_boollike(coldata):
    for x in coldata:
        x = x.strip()
        if len(x) >= 2 and x[0] == x[-1] and x[0] in "'\"`":
            x = x[1:-1]
        if len(x) > 5 or x.lower() not in {"true", "false", "0", "1", ""}:
            return False
    return True


def is_intlike(x):
    x = x.strip().strip('"\'')
    return (x.isdigit() or
            (len(x) > 2 and x[0] == x[-1] and x[0] in "'\"" and
                               x[1:-1].isdigit()) or
            x == "" or
            x == '""' or
            x == "''")

def is_reallike(x):
    x = x.strip().strip('"\'')
    try:
        float(x)
        return True
    except (ValueError, TypeError):
        return False


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
        # Generate simple alphanumeric strings and make sure
        # the resulting column is not fully populated with numeric values.
        is_numeric = nrows > 0
        col = []
        while is_numeric:
            col = [rr(random_string(int(random.expovariate(0.01))))
                    for _ in range(nrows)]
            for row in col:
                try:
                    if row:
                        float(row)
                except:
                    is_numeric = False
                    break
        return col
