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
import sys
import itertools
import os
import random
import re
import textwrap
import time
from datatable.lib import core
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from tests_random.metaframe import MetaFrame


python_output = None

def repr_row(row, j):
    assert 0 <= j < len(row)
    if len(row) <= 20 or j <= 12:
        out = str(row[:j])[:-1]
        if j > 0:
            out += ",   " + str(row[j])
        if len(row) > 20:
            out += ",   " + str(row[j+1:20])[1:-1] + ", ...]"
        elif j < len(row) - 1:
            out += ",   " + str(row[j+1:])[1:]
        else:
            out += "]"
    else:
        out = str(row[:10])[:-1] + ", ..., " + str(row[j]) + ", ...]"
    return out

def repr_slice(s):
    if s == slice(None):
        return ":"
    if s.start is None:
        if s.stop is None:
            return "::" +  str(s.step)
        elif s.step is None:
            return ":" + str(s.stop)
        else:
            return ":" + str(s.stop) + ":" + str(s.step)
    else:
        if s.stop is None:
            return str(s.start) + "::" +  str(s.step)
        elif s.step is None:
            return str(s.start) + ":" + str(s.stop)
        else:
            return str(s.start) + ":" + str(s.stop) + ":" + str(s.step)

def repr_data(data, indent):
    out = "["
    for i, arr in enumerate(data):
        if i > 0:
            out += " " * (indent + 1)
        out += "["
        out += textwrap.fill(", ".join(repr(e) for e in arr),
                             width = 80, subsequent_indent=" "*(indent+2))
        out += "]"
        if i != len(data) - 1:
            out += ",\n"
    out += "]"
    return out

def repr_type(t):
    if t is int:
        return "int"
    if t is bool:
        return "bool"
    if t is float:
        return "float"
    if t is str:
        return "str"
    raise ValueError("Unknown type %r" % t)

def repr_types(types):
    assert isinstance(types, (list, tuple))
    return "[" + ", ".join(repr_type(t) for t in types) + "]"



#-------------------------------------------------------------------------------
# Attacker
#-------------------------------------------------------------------------------

class Attacker:

    def __init__(self, seed=None, exhaustive_checks=False, pyout=None):
        if seed is None:
            seed = random.getrandbits(64)
        self._seed = seed
        self._exhaustive_checks = exhaustive_checks
        self._out = pyout
        random.seed(seed)
        print("Seed: %r\n" % seed)

    def attack(self, frame=None, rounds=None):
        t0 = time.time()
        if rounds is None:
            rounds = int(random.expovariate(0.05) + 2)
        assert isinstance(rounds, int)
        if frame is None:
            frame = MetaFrame()
            frame.write_to_file(self._out)
        print("Launching an attack for %d rounds" % rounds)
        for _ in range(rounds):
            self.attack_frame(frame)
            if self._exhaustive_checks:
                frame.check()
            if time.time() - t0 > 60:
                print(">>> Stopped early, taking too long <<<")
                break
        print("\nAttack ended, checking the outcome... ", end='')
        frame.check()
        print(core.apply_color("bright_green", "PASSED"))
        t1 = time.time()
        print("Time taken = %.3fs" % (t1 - t0))

    def attack_frame(self, frame):
        action = random.choices(population=self.ATTACK_METHODS,
                                cum_weights=self.ATTACK_WEIGHTS, k=1)[0]
        action(self, frame)


    #---------------------------------------------------------------------------
    # Individual attack methods
    #---------------------------------------------------------------------------

    def resize_rows(self, frame):
        t = random.random()
        curr_nrows = frame.nrows
        new_nrows = int(curr_nrows * 10 / (19 * t + 1) + 1)
        print("[01] Setting nrows to %d" % (new_nrows, ))

        res = frame.resize_rows(new_nrows)

        if self._out:
            if res:
                self._out.write("DT.nrows = %d\n" % new_nrows)
            else:
                self._out.write("with pytest.raises(ValueError, "
                                    "match='Cannot increase the number of rows "
                                    "in a keyed frame'):\n"
                                    "    DT.nrows = %d\n\n"
                                    % new_nrows)

    def slice_rows(self, frame):
        s = self.random_slice(frame.nrows)
        new_ncols = len(range(*s.indices(frame.nrows)))
        print("[02] Applying row slice (%r, %r, %r) -> nrows = %d"
              % (s.start, s.stop, s.step, new_ncols))
        if self._out:
            self._out.write("DT = DT[%s, :]\n" % repr_slice(s))
        frame.slice_rows(s)

    def slice_cols(self, frame):
        s = self.random_slice(frame.ncols)
        new_ncols = len(range(*s.indices(frame.ncols)))
        print("[03] Applying column slice (%r, %r, %r) -> ncols = %d"
              % (s.start, s.stop, s.step, new_ncols))
        if self._out:
            self._out.write("DT = DT[:, %s]\n" % repr_slice(s))
        frame.slice_cols(s)

    def cbind_self(self, frame):
        if frame.ncols > 1000:
            return self.slice_cols(frame)
        t = random.randint(1, min(5, 100 // (1 + frame.ncols)) + 1)
        print("[04] Cbinding frame with itself %d times -> ncols = %d"
              % (t, frame.ncols * (t + 1)))
        if self._out:
            self._out.write("DT.cbind([DT] * %d)\n" % t)
        frame.cbind([frame] * t)

    def rbind_self(self, frame):
        t = random.randint(1, min(5, 1000 // (1 + frame.nrows)) + 1)
        res = frame.rbind([frame] * t)
        if self._out:
            if res:
                self._out.write("DT.rbind([DT] * %d)\n" % t)
            else:
                self._out.write("with pytest.raises(ValueError, "
                                    "match='Cannot rbind to a keyed frame'):\n"
                                    "    DT.rbind([DT] * %d)\n" % t)
        print("[05] Rbinding frame with itself %d times -> nrows = %d"
              % (t, frame.nrows))

    def select_rows_array(self, frame):
        if frame.nrows == 0:
            return
        s = self.random_array(frame.nrows)
        print("[06] Selecting a row list %r -> nrows = %d" % (s, len(s)))
        if self._out:
            self._out.write("DT = DT[%r, :]\n" % (s,))
        frame.slice_rows(s)

    def delete_rows_array(self, frame):
        if frame.nrows == 0:
            return
        s = self.random_array(frame.nrows, positive=True)
        s = sorted(set(s))
        print("[07] Removing rows %r -> nrows = %d"
              % (s, frame.nrows - len(s)))
        if self._out:
            self._out.write("del DT[%r, :]\n" % (s,))
        frame.delete_rows(s)

    def select_rows_with_boolean_column(self, frame):
        bool_columns = [i for i in range(frame.ncols) if frame.types[i] is bool]
        if frame.ncols <= 1 or not bool_columns:
            return
        filter_column = random.choice(bool_columns)
        print("[08] Selecting rows where column %d (%r) is True"
              % (filter_column, frame.names[filter_column]))
        if self._out:
            self._out.write("DT = DT[f[%d], :]\n" % filter_column)
        frame.filter_on_bool_column(filter_column)

    def replace_nas_in_column(self, frame):
        icol = random.randint(0, frame.ncols - 1)
        if frame.types[icol] is bool:
            replacement_value = random.choice([True, False])
        elif frame.types[icol] is int:
            replacement_value = random.randint(-100, 100)
        elif frame.types[icol] is float:
            replacement_value = random.random() * 1000
        elif frame.types[icol] is str:
            replacement_value = MetaFrame.random_name()
        print("[09] Replacing NA values in column %d with %r"
              % (icol, replacement_value))
        res = frame.replace_nas_in_column(icol, replacement_value)

        if self._out:
            if res:
                self._out.write("DT[f[%d] == None, f[%d]] = %r\n"
                                    % (icol, icol, replacement_value))
            else:
                msg = 'Cannot change values in a key column %s' % frame.names[icol]
                msg = re.escape(msg)
                self._out.write("with pytest.raises(ValueError, match='%s'):\n"
                                    "    DT[f[%d] == None, f[%d]] = %r\n"
                                    % (msg, icol, icol, replacement_value))

    def sort_columns(self, frame):
        if frame.ncols == 0:
            return
        ncols_sort = min(int(random.expovariate(1.0)) + 1, frame.ncols)
        a = random.sample(range(0, frame.ncols), ncols_sort)

        print("[10] Sorting %d columns in ascending order: %r" % (len(a), a))
        if self._out:
            self._out.write("DT = DT.sort(%r)\n" % a)
        frame.sort_columns(a)

    def cbind_numpy_column(self, frame):
        print("[11] Cbinding frame with a numpy column -> ncols = %d"
              % (frame.ncols + 1))

        frame.cbind_numpy_column(self._out)
        if self._out:
            self._out.write("DT.cbind(DTNP)\n")

    def add_range_column(self, frame):
        start = int(random.expovariate(0.05) - 5)
        step = 0
        while step == 0:
            step = int(1 + random.random() * 3)
        stop = start + step * frame.nrows
        rangeobj = range(start, stop, step)
        print("[12] Adding a range column %r -> ncols = %d"
              % (rangeobj, frame.ncols + 1))
        name = MetaFrame.random_name()
        if self._out:
            self._out.write("DT.cbind(dt.Frame(%s=%r))\n"
                                % (name, rangeobj))
        frame.add_range_column(name, rangeobj)

    def set_key_columns(self, frame):
        if frame.ncols == 0:
            return

        nkeys = min(int(random.expovariate(1.0)) + 1, frame.ncols)
        keys = random.sample(range(0, frame.ncols), nkeys)
        names = [frame.names[i] for i in keys]

        print("[13] Setting %d key column(s): %r" % (nkeys, keys))

        res = frame.set_key_columns(keys, names)
        if self._out:
            if res:
                self._out.write("DT.key = %r\n" % names)
            else:
                self._out.write("with pytest.raises(ValueError, "
                                    "match='Cannot set a key: the values are "
                                    "not unique'):\n"
                                    "    DT.key = %r\n\n"
                                    % names)

    def delete_columns_array(self, frame):
        # datatable supports a shape of n x 0 for non-zero n's, while
        # python doesn't, so we never remove all the columns from a Frame.
        if frame.ncols < 2:
            return
        s = self.random_array(frame.ncols - 1, positive=True)
        res = frame.delete_columns(s)
        print("[14] Removing columns %r -> ncols = %d" % (s, frame.ncols))
        if self._out:
            if res:
                self._out.write("del DT[:, %r]\n" % s)
            else:
                self._out.write("with pytest.raises(ValueError, "
                                    "match='Cannot delete a column that is "
                                    "a part of a multi-column key'):\n"
                                    "    del DT[:, %r]\n\n"
                                    % s)




    def join_self(self, frame):
        if frame.ncols > 1000:
            return self.slice_cols(frame)

        res = frame.join_self()
        print("[15] Joining frame with itself -> ncols = %d" % frame.ncols)
        if self._out:
            if res:
                self._out.write("DT = DT[:, :, join(DT)]\n")
            else:
                self._out.write("with pytest.raises(ValueError, "
                                    "match='The join frame is not keyed'):\n"
                                    "    DT = DT[:, :, join(DT)]\n\n")

    def shallow_copy(self, frame):
        print("[16] Creating a shallow copy of a frame")
        if self._out:
            self._out.write("DT_shallow_copy = DT.copy()\n")
            self._out.write("DT_deep_copy = copy.deepcopy(DT)\n")
        frame.shallow_copy()


    #---------------------------------------------------------------------------
    # Helpers
    #---------------------------------------------------------------------------

    def random_slice(self, n):
        while True:
            t = random.random()
            s = random.random()
            i0 = int(n * t * 0.2) if t > 0.5 else None
            i1 = int(n * (s * 0.2 + 0.8)) if s > 0.5 else None
            if i0 is not None and i1 is not None and i1 < i0:
                i0, i1 = i1, i0
            step = random.choice([-2, -1, -1, 1, 1, 2, 2, 2, 3, 3, 4, 5, 7] +
                                 [None] * 3)
            if step is not None and step < 0:
                i0, i1 = i1, i0
            res = slice(i0, i1, step)
            newn = len(range(*res.indices(n)))
            if newn > 0 or n == 0:
                return res

    def random_array(self, n, positive=False):
        assert n > 0
        newn = max(5, random.randint(n // 2, 3 * n // 2))
        lb = 0 if positive else -n
        ub = n - 1
        return [random.randint(lb, ub) for i in range(newn)]


    ATTACK_METHODS = {
        resize_rows: 1,
        slice_rows: 3,
        slice_cols: 0.5,
        cbind_self: 1,
        rbind_self: 1,
        select_rows_array: 1,
        delete_rows_array: 1,
        select_rows_with_boolean_column: 1,
        replace_nas_in_column: 1,
        sort_columns: 1,
        cbind_numpy_column: 1,
        add_range_column: 1,
        set_key_columns: 1,
        delete_columns_array: 1,
        join_self: 1,
        shallow_copy: 1,
    }
    ATTACK_WEIGHTS = list(itertools.accumulate(ATTACK_METHODS.values()))
    ATTACK_METHODS = list(ATTACK_METHODS.keys())





if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="Perform single batch of randomized testing. The SEED "
                    "argument is required. Use this script for troubleshooting "
                    "a particular seed value, otherwise run `random_driver.py`."
    )
    parser.add_argument("seed", type=int, metavar="SEED")
    parser.add_argument("-py", "--python", action="store_true",
                        help="Generate python file corresponding to the code "
                             "being run.")
    parser.add_argument("-x", "--exhaustive", action="store_true",
                        help="Run exhaustive checks, i.e. check for validity "
                             "after every step.")
    args = parser.parse_args()
    if args.python:
        ra_dir = os.path.join(os.path.dirname(__file__), "random_attack_logs")
        os.makedirs(ra_dir, exist_ok=True)
        outfile = os.path.join(ra_dir, str(args.seed) + ".py")
        with open(outfile, "wt", encoding="UTF-8") as out:
            out.write("#!/usr/bin/env python\n")
            out.write("# seed: %s\n" % args.seed)
            out.write("import sys; sys.path = ['.', '..'] + sys.path\n")
            out.write("import copy\n")
            out.write("import datatable as dt\n")
            out.write("import numpy as np\n")
            out.write("import pytest\n")
            out.write("from datatable import f, join\n")
            out.write("from datatable.internal import frame_integrity_check\n")
            out.write("from tests import assert_equals\n\n")

            try:
                ra = Attacker(args.seed, args.exhaustive, out)
                ra.attack()
            finally:
                out.write("frame_integrity_check(DT)\n")
                out.write("if DT_shallow_copy:\n")
                out.write("    frame_integrity_check(DT_shallow_copy)\n")
                out.write("    frame_integrity_check(DT_deep_copy)\n")
                out.write("    assert_equals(DT_shallow_copy, DT_deep_copy)\n")

    else:
        ra = Attacker(args.seed, args.exhaustive)
        ra.attack()
