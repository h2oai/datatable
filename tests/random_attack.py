#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import sys; sys.path = ['.', '..'] + sys.path
import copy
import datatable as dt
import itertools
import numpy as np
import os
import pytest
import random
import re
import textwrap
import time
import warnings
from datatable import f, join
from datatable.internal import frame_integrity_check
from datatable.utils.terminal import term
from datatable.utils.misc import plural_form as plural
from tests import assert_equals

exhaustive_checks = False
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

    def __init__(self, seed=None):
        if seed is None:
            seed = random.getrandbits(64)
        self._seed = seed
        random.seed(seed)
        print("Seed: %r\n" % seed)

    def attack(self, frame=None, rounds=None):
        t0 = time.time()
        if rounds is None:
            rounds = int(random.expovariate(0.05) + 2)
        assert isinstance(rounds, int)
        if frame is None:
            frame = Frame0()
        print("Launching an attack for %d rounds" % rounds)
        for _ in range(rounds):
            self.attack_frame(frame)
            if exhaustive_checks:
                frame.check()
            if time.time() - t0 > 60:
                print(">>> Stopped early, taking too long <<<")
                break
        print("\nAttack ended, checking the outcome... ", end='')
        frame.check()
        print(term.color("bright_green", "PASSED"))
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

        if python_output:
            if res:
                python_output.write("DT.nrows = %d\n" % new_nrows)
            else:
                python_output.write("with pytest.raises(ValueError, "
                                    "match='Cannot increase the number of rows "
                                    "in a keyed frame'):\n"
                                    "    DT.nrows = %d\n\n"
                                    % new_nrows)

    def slice_rows(self, frame):
        s = self.random_slice(frame.nrows)
        new_ncols = len(range(*s.indices(frame.nrows)))
        print("[02] Applying row slice (%r, %r, %r) -> nrows = %d"
              % (s.start, s.stop, s.step, new_ncols))
        if python_output:
            python_output.write("DT = DT[%s, :]\n" % repr_slice(s))
        frame.slice_rows(s)

    def slice_cols(self, frame):
        s = self.random_slice(frame.ncols)
        new_ncols = len(range(*s.indices(frame.ncols)))
        print("[03] Applying column slice (%r, %r, %r) -> ncols = %d"
              % (s.start, s.stop, s.step, new_ncols))
        if python_output:
            python_output.write("DT = DT[:, %s]\n" % repr_slice(s))
        frame.slice_cols(s)

    def cbind_self(self, frame):
        if frame.ncols > 1000:
            return self.slice_cols(frame)
        t = random.randint(1, min(5, 100 // (1 + frame.ncols)) + 1)
        print("[04] Cbinding frame with itself %d times -> ncols = %d"
              % (t, frame.ncols * (t + 1)))
        if python_output:
            python_output.write("DT.cbind([DT] * %d)\n" % t)
        frame.cbind([frame] * t)

    def rbind_self(self, frame):
        t = random.randint(1, min(5, 1000 // (1 + frame.nrows)) + 1)
        res = frame.rbind([frame] * t)
        if python_output:
            if res:
                python_output.write("DT.rbind([DT] * %d)\n" % t)
            else:
                python_output.write("with pytest.raises(ValueError, "
                                    "match='Cannot rbind to a keyed frame'):\n"
                                    "    DT.rbind([DT] * %d)\n" % t)
        print("[05] Rbinding frame with itself %d times -> nrows = %d"
              % (t, frame.nrows))

    def select_rows_array(self, frame):
        if frame.nrows == 0:
            return
        s = self.random_array(frame.nrows)
        print("[06] Selecting a row list %r -> nrows = %d" % (s, len(s)))
        if python_output:
            python_output.write("DT = DT[%r, :]\n" % (s,))
        frame.slice_rows(s)

    def delete_rows_array(self, frame):
        if frame.nrows == 0:
            return
        s = self.random_array(frame.nrows, positive=True)
        s = sorted(set(s))
        print("[07] Removing rows %r -> nrows = %d"
              % (s, frame.nrows - len(s)))
        if python_output:
            python_output.write("del DT[%r, :]\n" % (s,))
        frame.delete_rows(s)

    def select_rows_with_boolean_column(self, frame):
        bool_columns = [i for i in range(frame.ncols) if frame.types[i] is bool]
        if frame.ncols <= 1 or not bool_columns:
            return
        filter_column = random.choice(bool_columns)
        print("[08] Selecting rows where column %d (%r) is True"
              % (filter_column, frame.names[filter_column]))
        if python_output:
            python_output.write("DT = DT[f[%d], :]\n" % filter_column)
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
            replacement_value = Frame0.random_name()
        print("[09] Replacing NA values in column %d with %r"
              % (icol, replacement_value))
        res = frame.replace_nas_in_column(icol, replacement_value)

        if python_output:
            if res:
                python_output.write("DT[f[%d] == None, f[%d]] = %r\n"
                                    % (icol, icol, replacement_value))
            else:
                msg = 'Cannot change values in a key column `%s`' % frame.names[icol]
                msg = re.escape(msg)
                python_output.write("with pytest.raises(ValueError, match='%s'):\n"
                                    "    DT[f[%d] == None, f[%d]] = %r\n"
                                    % (msg, icol, icol, replacement_value))

    def sort_columns(self, frame):
        if frame.ncols == 0:
            return
        ncols_sort = min(int(random.expovariate(1.0)) + 1, frame.ncols)
        a = random.sample(range(0, frame.ncols), ncols_sort)

        print("[10] Sorting %s in ascending order: %r"
              % (plural(len(a), "column"), a))
        if python_output:
            python_output.write("DT = DT.sort(%r)\n" % a)
        frame.sort_columns(a)

    def cbind_numpy_column(self, frame):
        print("[11] Cbinding frame with a numpy column -> ncols = %d"
              % (frame.ncols + 1))

        frame.cbind_numpy_column()
        if python_output:
            python_output.write("DT.cbind(DTNP)\n")

    def add_range_column(self, frame):
        start = int(random.expovariate(0.05) - 5)
        step = 0
        while step == 0:
            step = int(1 + random.random() * 3)
        stop = start + step * frame.nrows
        rangeobj = range(start, stop, step)
        print("[12] Adding a range column %r -> ncols = %d"
              % (rangeobj, frame.ncols + 1))
        name = Frame0.random_name()
        if python_output:
            python_output.write("DT.cbind(dt.Frame(%s=%r))\n"
                                % (name, rangeobj))
        frame.add_range_column(name, rangeobj)

    def set_key_columns(self, frame):
        if frame.ncols == 0:
            return

        nkeys = min(int(random.expovariate(1.0)) + 1, frame.ncols)
        keys = random.sample(range(0, frame.ncols), nkeys)
        names = [frame.names[i] for i in keys]

        print("[13] Setting %s: %r"
              % (plural(nkeys, "key column"), keys))

        res = frame.set_key_columns(keys, names)
        if python_output:
            if res:
                python_output.write("DT.key = %r\n" % names)
            else:
                python_output.write("with pytest.raises(ValueError, "
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
        if python_output:
            if res:
                python_output.write("del DT[:, %r]\n" % s)
            else:
                python_output.write("with pytest.raises(ValueError, "
                                    "match='Cannot delete a column that is "
                                    "a part of a multi-column key'):\n"
                                    "    del DT[:, %r]\n\n"
                                    % s)




    def join_self(self, frame):
        if frame.ncols > 1000:
            return self.slice_cols(frame)

        res = frame.join_self()
        print("[15] Joining frame with itself -> ncols = %d" % frame.ncols)
        if python_output:
            if res:
                python_output.write("DT = DT[:, :, join(DT)]\n")
            else:
                python_output.write("with pytest.raises(ValueError, "
                                    "match='The join frame is not keyed'):\n"
                                    "    DT = DT[:, :, join(DT)]\n\n")

    def shallow_copy(self, frame):
        print("[16] Creating a shallow copy of a frame")
        if python_output:
            python_output.write("DT_shallow_copy = DT.copy()\n")
            python_output.write("DT_deep_copy = copy.deepcopy(DT)\n")
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




#-------------------------------------------------------------------------------
# Frame0
#-------------------------------------------------------------------------------

class Frame0:

    def __init__(self, ncols=None, nrows=None, types=None, names=None,
                 missing_fraction=None):
        if ncols is None:
            if types:
                ncols = len(types)
            elif names:
                ncols = len(names)
            else:
                ncols = int(random.expovariate(0.2)) + 1
        if nrows is None:
            nrows = int(random.expovariate(0.01)) + 1
        if not types:
            types = [self.random_type() for _ in range(ncols)]
        if not names:
            names = self.random_names(ncols)
        if missing_fraction is None:
            missing_fraction = random.random()**10
            if missing_fraction < 0.05 or nrows == 1:
                missing_fraction = 0.0
        assert isinstance(ncols, int)
        assert isinstance(nrows, int)
        assert isinstance(types, list) and len(types) == ncols
        assert isinstance(names, list) and len(names) == ncols
        assert isinstance(missing_fraction, float)
        tt = {bool: 0, int: 0, float: 0, str: 0}
        for t in types:
            tt[t] += 1
        assert len(tt) == 4
        print("Making a frame with nrows=%d, ncols=%d" % (nrows, ncols))
        print("  types: bool=%d, int=%d, float=%d, str=%d"
              % (tt[bool], tt[int], tt[float], tt[str]))
        print("  missing values: %.3f" % missing_fraction)
        data = [self.random_column(nrows, types[i], missing_fraction)[0]
                for i in range(ncols)]
        self.data = data
        self.names = names
        self.types = types
        self.nkeys = 0
        self.np_data = []
        self.np_data_deepcopy = []
        self.df = dt.Frame(data, names=names, stypes=types)
        self.df_shallow_copy = self.df_deep_copy = None
        if python_output:
            python_output.write("DT = dt.Frame(%s,\n"
                                "              names=%r,\n"
                                "              stypes=%s)\n"
                                % (repr_data(data, 14), names, repr_types(types)))
            python_output.write("assert DT.shape == (%d, %d)\n" % (nrows, ncols))
            python_output.write("DT_shallow_copy = DT_deep_copy = None\n")

    def random_type(self):
        return random.choice([bool, int, float, str])


    def random_names(self, ncols):
        t = random.random()
        if t < 0.8:
            alphabet = "abcdefghijklmnopqrstuvwxyz"
            if t < 0.5:
                alphabet = alphabet * 2 + "0123456789"
            elif t < 0.6:
                alphabet += "".join(chr(i) for i in range(0x20, 0x7F))
            else:
                alphabet = (alphabet * 40 + "0123456789" * 10 +
                            "".join(chr(x) for x in range(192, 448)))
            while True:
                # Ensure uniqueness of the returned names
                names = [self.random_name(alphabet) for _ in range(ncols)]
                if len(set(names)) == ncols:
                    return names
        else:
            c = chr(ord('A') + random.randint(0, 25))
            return ["%s%d" % (c, i) for i in range(ncols)]


    @staticmethod
    def random_name(alphabet="abcdefghijklmnopqrstuvwxyz"):
        n = int(random.expovariate(0.2) + 1.5)
        while True:
            name = "".join(random.choice(alphabet) for _ in range(n))
            if not(name.isdigit() or name.isspace()):
                return name


    def random_column(self, nrows, ttype, missing_fraction, missing_nones=True):
        missing_mask = [False] * nrows
        if ttype == bool:
            data = self.random_bool_column(nrows)
        elif ttype == int:
            data = self.random_int_column(nrows)
        elif ttype == float:
            data = self.random_float_column(nrows)
        else:
            data = self.random_str_column(nrows)
        if missing_fraction:
            for i in range(nrows):
                if random.random() < missing_fraction:
                    missing_mask[i] = True

        if missing_nones:
            for i in range(nrows):
                if missing_mask[i]: data[i] = None

        return data, missing_mask


    def random_bool_column(self, nrows):
        t = random.random()  # fraction of 1s
        return [random.random() < t
                for _ in range(nrows)]


    def random_int_column(self, nrows):
        t = random.random()
        s = t * 10 - int(t * 10)   # 0...1
        q = t * 1000 - int(t * 1000)
        if t < 0.1:
            r0, r1 = -int(10 + s * 20), int(60 + s * 100)
        elif t < 0.3:
            r0, r1 = 0, int(60 + s * 100)
        elif t < 0.6:
            r0, r1 = 0, int(t * 1000000 + 500000)
        elif t < 0.8:
            r1 = int(t * 100000 + 30000)
            r0 = -r1
        elif t < 0.9:
            r1 = (1 << 63) - 1
            r0 = -r1
        else:
            r0, r1 = 0, (1 << 63) - 1
        data = [random.randint(r0, r1) for _ in range(nrows)]
        if q < 0.2:
            fraction = 0.5 + q * 2
            for i in range(nrows):
                if random.random() < fraction:
                    data[i] = 0
        return data


    def random_float_column(self, nrows):
        scale = random.expovariate(0.3) + 1
        return [random.random() * scale
                for _ in range(nrows)]


    def random_str_column(self, nrows):
        t = random.random()
        if t < 0.2:
            words = ["cat", "dog", "mouse", "squirrel", "whale",
                     "ox", "ant", "badger", "eagle", "snail"][:int(2 + t * 50)]
            return [random.choice(words) for _ in range(nrows)]
        else:
            return [self.random_name() for _ in range(nrows)]


    #---------------------------------------------------------------------------
    # Properties
    #---------------------------------------------------------------------------

    @property
    def nrows(self):
        nrows = len(self.data[0]) if self.data else 0
        assert self.df.nrows == nrows
        return nrows

    @property
    def ncols(self):
        ncols = len(self.data)
        assert self.df.ncols == ncols
        return ncols


    #---------------------------------------------------------------------------
    # Validity checks
    #---------------------------------------------------------------------------

    def check(self):
        frame_integrity_check(self.df)
        if self.df_shallow_copy:
            frame_integrity_check(self.df_shallow_copy)
            frame_integrity_check(self.df_deep_copy)
            assert_equals(self.df_shallow_copy, self.df_deep_copy)
        self.check_shape()
        self.check_types()
        self.check_keys()
        if self.df.names != tuple(self.names):
            print("ERROR: df.names=%r ≠\n"
                  "       py.names=%r" % (self.df.names, tuple(self.names)))
            sys.exit(1)
        self.check_data()
        self.check_np_data()

    def check_shape(self):
        df_nrows = self.df.nrows
        py_nrows = len(self.data[0]) if self.data else 0
        if df_nrows != py_nrows:
            print("ERROR: df.nrows=%r != py.nrows=%r" % (df_nrows, py_nrows))
            sys.exit(1)
        df_ncols = self.df.ncols
        py_ncols = len(self.data)
        if df_ncols != py_ncols:
            print("ERROR: df.ncols=%r != py.ncols=%r" % (df_ncols, py_ncols))
            sys.exit(1)

    def check_keys(self):
        dt_nkeys = len(self.df.key)
        py_nkeys = self.nkeys
        if dt_nkeys != py_nkeys:
            print("ERROR: number of key columns doesn't match")
            print("  dt keys: %r" % dt_nkeys)
            print("  py keys: %r" % py_nkeys)
            sys.exit(1)

    def check_types(self):
        df_ltypes = self.df.ltypes
        py_ltypes = tuple(dt.ltype(x) for x in self.types)
        if df_ltypes != py_ltypes:
            print("ERROR: types mismatch")
            print("  dt types: %r" % (df_ltypes, ))
            print("  py types: %r" % (py_ltypes, ))
            sys.exit(1)

    def check_data(self):
        df_data = self.df.to_list()
        py_data = self.data
        if df_data != py_data:
            assert len(df_data) == len(py_data), "Shape check failed..."
            for i, dfcol in enumerate(df_data):
                pycol = py_data[i]
                if dfcol == pycol:
                    continue
                print("ERROR: data mismatch in column %d (%r)"
                      % (i, self.df.names[i]))
                for j, dfval in enumerate(dfcol):
                    pyval = pycol[j]
                    if dfval == pyval:
                        continue
                    print("  first difference: dt[%d]=%r != py[%d]=%r"
                          % (j, dfval, j, pyval))
                    print("  dt data: %s" % repr_row(dfcol, j))
                    print("  py data: %s" % repr_row(pycol, j))
                    sys.exit(1)
            assert False, "Data check failed..."


    def check_np_data(self):
        np_data1 = self.np_data
        np_data2 = self.np_data_deepcopy

        assert len(np_data1) == len(np_data2), "Numpy data shape check failed..."
        for i, np_col1 in enumerate(np_data1):
            np_col2 = np_data2[i]
            assert len(np_col1) == len(np_col2), "Numpy column shape check failed..."
            if np.array_equal(np_col1, np_col2):
                continue
            print("ERROR: numpy data mismatch at column %d" % i)
            for j, np_val1 in enumerate(np_col1):
                np_val2 = np_col2[j]
                if np_val1 == np_val2:
                    continue
                print("  first difference: np_col1[%d]=%r != np_col2[%d]=%r"
                      % (j, np_val1, j, np_val2))
                print("  np_col1 data: %s" % repr_row(list(np_col1), j))
                print("  np_col2 data: %s" % repr_row(list(np_col2), j))
                sys.exit(1)
            assert False, "Numpy data check failed..."

    #---------------------------------------------------------------------------
    # Operations
    #---------------------------------------------------------------------------

    def resize_rows(self, nrows):
        curr_nrows = self.nrows
        if self.nkeys and nrows > curr_nrows:
            with pytest.raises(ValueError, match="Cannot increase the number "
                               "of rows in a keyed frame"):
                self.df.nrows = nrows
            return False
        else:
            self.df.nrows = nrows

        if curr_nrows < nrows:
            append = [None] * (nrows - curr_nrows)
            for i, elem in enumerate(self.data):
                self.data[i] = elem + append
        elif curr_nrows > nrows:
            for i, elem in enumerate(self.data):
                self.data[i] = elem[:nrows]
        return True

    def slice_rows(self, s):
        self.df = self.df[s, :]
        if isinstance(s, slice):
            for i in range(self.ncols):
                self.data[i] = self.data[i][s]
        else:
            for i in range(self.ncols):
                col = self.data[i]
                self.data[i] = [col[j] for j in s]
        self.nkeys = 0

    def delete_rows(self, s):
        assert isinstance(s, slice) or isinstance(s, list)
        nrows = self.nrows
        del self.df[s, :]
        if isinstance(s, slice):
            s = list(range(nrows))[s]
        index = sorted(set(range(nrows)) - set(s))
        for i in range(self.ncols):
            col = self.data[i]
            self.data[i] = [col[j] for j in index]

    def delete_columns(self, s):
        assert isinstance(s, slice) or isinstance(s, list)
        if isinstance(s, slice):
            s = list(range(ncols))[s]
        set_keys = set(range(self.nkeys))
        set_delcols = set(s)
        nkeys_remove = len(set_keys.intersection(set_delcols))

        if (nkeys_remove > 0 and nkeys_remove < self.nkeys and self.nrows > 0):
            with pytest.raises(ValueError, match="Cannot delete a column that "
                               "is a part of a multi-column key"):
                del self.df[:, s]
            return False
        else:
            ncols = self.ncols
            del self.df[:, s]
            new_column_ids = sorted(set(range(ncols)) - set(s))
            self.data = [self.data[i] for i in new_column_ids]
            self.names = [self.names[i] for i in new_column_ids]
            self.types = [self.types[i] for i in new_column_ids]
            self.nkeys -= nkeys_remove
            return True


    def slice_cols(self, s):
        self.df = self.df[:, s]
        self.data = self.data[s]
        self.names = self.names[s]
        self.types = self.types[s]
        self.nkeys = 0

    def cbind(self, frames):
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", dt.DatatableWarning)
            self.df.cbind(*[iframe.df for iframe in frames])
        newdata = copy.deepcopy(self.data)
        newnames = self.names.copy()
        newtypes = self.types.copy()
        for iframe in frames:
            newdata += copy.deepcopy(iframe.data)
            newnames += iframe.names
            newtypes += iframe.types
        self.data = newdata
        self.names = newnames
        self.types = newtypes
        self.dedup_names()

    def rbind(self, frames):
        if (self.nkeys > 0) and (self.nrows > 0):
            with pytest.raises(ValueError, match="Cannot rbind to a keyed frame"):
                self.df.rbind(*[iframe.df for iframe in frames])
            return False
        else:
            self.df.rbind(*[iframe.df for iframe in frames])

        newdata = [col.copy() for col in self.data]
        for iframe in frames:
            assert iframe.ncols == self.ncols
            for j in range(self.ncols):
                assert self.types[j] == iframe.types[j]
                assert self.names[j] == iframe.names[j]
                newdata[j] += iframe.data[j]
        self.data = newdata
        return True

    def filter_on_bool_column(self, icol):
        assert self.types[icol] is bool
        filter_col = self.data[icol]
        self.data = [[value for i, value in enumerate(column) if filter_col[i]]
                     for column in self.data]
        self.df = self.df[f[icol], :]
        self.nkeys = 0

    def replace_nas_in_column(self, icol, replacement_value):
        assert 0 <= icol < self.ncols
        assert isinstance(replacement_value, self.types[icol])

        if icol < self.nkeys:
            msg = 'Cannot change values in a key column `%s`' % self.names[icol]
            msg = re.escape(msg)
            with pytest.raises(ValueError, match = msg):
                self.df[f[icol] == None, f[icol]] = replacement_value
            return False
        else:
            self.df[f[icol] == None, f[icol]] = replacement_value
            column = self.data[icol]
            for i, value in enumerate(column):
                if value is None:
                    column[i] = replacement_value
            return True

    def sort_columns(self, a):
        self.df = self.df.sort(a)
        if self.nrows:
            data = list(zip(*self.data))
            data.sort(key=lambda x: [(x[i] is not None, x[i]) for i in a])
            self.data = list(map(list, zip(*data)))
        self.nkeys = 0

    def cbind_numpy_column(self):
        coltype = self.random_type()
        mfraction = random.random()
        data, mmask = self.random_column(self.nrows, coltype, mfraction, False)
        np_data = np.ma.array(data, mask=mmask, dtype=np.dtype(coltype))

        # Save random numpy arrays to make sure they don't change with
        # munging. Arrays that are not saved here will be eventually deleted
        # by Python, in such a case we also test datatable behaviour.
        if random.random() > 0.5:
            self.np_data += [np_data]
            self.np_data_deepcopy += [copy.deepcopy(np_data)]

        names = self.random_names(1)
        df = dt.Frame(np_data.T, names=names)
        if python_output:
            python_output.write("DTNP = dt.Frame(np.ma.array(%s,\n"
                                "                mask=%s,\n"
                                "                dtype=np.dtype(%s)).T,\n"
                                "                names=%r)\n"
                                % (repr_data([data], 14),
                                   repr_data([mmask], 14),
                                   coltype.__name__,
                                   names))
            python_output.write("assert DTNP.shape == (%d, %d)\n"
                                % (self.nrows, 1))

        for i in range(self.nrows):
            if mmask[i]: data[i] = None

        self.df.cbind(df)
        self.data += [data]
        self.types += [coltype]
        self.names += names
        self.dedup_names()

    def add_range_column(self, name, rangeobj):
        self.data += [list(rangeobj)]
        self.names += [name]
        self.types += [int]
        self.dedup_names()
        self.df.cbind(dt.Frame(rangeobj, names=[name]))

    def set_key_columns(self, keys, names):
        key_data = [self.data[i] for i in keys]
        unique_rows = set(zip(*key_data))
        if len(unique_rows) == self.nrows:
            self.df.key = names
        else:
            with pytest.raises(ValueError, match="Cannot set a key: the values "
                               "are not unique"):
                self.df.key = names
            return False

        nonkeys = sorted(set(range(self.ncols)) - set(keys))
        new_column_order = keys + nonkeys

        self.types = [self.types[i] for i in new_column_order]
        self.names = [self.names[i] for i in new_column_order]
        self.nkeys = len(keys)

        if self.nrows:
            data = list(zip(*self.data))
            data.sort(key=lambda x: [(x[i] is not None, x[i]) for i in keys])
            self.data = list(map(list, zip(*data)))
            self.data = [self.data[i] for i in new_column_order]
        return True

    def join_self(self):
        ncols = self.ncols
        if self.nkeys:
            self.df = self.df[:, :, join(self.df)]
        else:
            with pytest.raises(ValueError, match="The join frame is not keyed"):
                self.df = self.df[:, :, join(self.df)]
            return False

        s = slice(self.nkeys, ncols)
        join_data = copy.deepcopy(self.data[s])
        join_types = self.types[s].copy()
        join_names = self.names[s].copy()

        self.data += join_data
        self.types += join_types
        self.names += join_names
        self.nkeys = 0
        self.dedup_names()
        return True

    # This is a noop for the python data
    def shallow_copy(self):
        self.df_shallow_copy = self.df.copy()
        self.df_deep_copy = copy.deepcopy(self.df)


    #---------------------------------------------------------------------------
    # Helpers
    #---------------------------------------------------------------------------

    def dedup_names(self):
        seen_names = set()
        for i, name in enumerate(self.names):
            if name in seen_names:
                base = name
                while base[-1].isdigit():
                    base = base[:-1]
                if base == name:
                    if base[-1] != '.':
                        base += "."
                    num = -1  # will be incremented to 0 in the while-loop
                else:
                    num = int(name[len(base):])
                while name in seen_names:
                    num += 1
                    name = base + str(num)
                self.names[i] = name
            seen_names.add(name)





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
    exhaustive_checks = args.exhaustive
    if args.python:
        ra_dir = os.path.join(os.path.dirname(__file__), "random_attack_logs")
        os.makedirs(ra_dir, exist_ok=True)
        outfile = os.path.join(ra_dir, str(args.seed) + ".py")
        python_output = open(outfile, "wt", encoding="UTF-8")
        python_output.write("#!/usr/bin/env python\n")
        python_output.write("#seed: %s\n" % args.seed)
        python_output.write("import sys; sys.path = ['.', '..'] + sys.path\n")
        python_output.write("import copy\n")
        python_output.write("import datatable as dt\n")
        python_output.write("import numpy as np\n")
        python_output.write("import pytest\n")
        python_output.write("from datatable import f, join\n")
        python_output.write("from datatable.internal import frame_integrity_check\n")
        python_output.write("from tests import assert_equals\n\n")

    try:
        ra = Attacker(args.seed)
        ra.attack()
    finally:
        if python_output:
            python_output.write("frame_integrity_check(DT)\n")
            python_output.write("if DT_shallow_copy:\n")
            python_output.write("    frame_integrity_check(DT_shallow_copy)\n")
            python_output.write("    frame_integrity_check(DT_deep_copy)\n")
            python_output.write("    assert_equals(DT_shallow_copy, DT_deep_copy)\n")
            python_output.close()
