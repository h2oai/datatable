#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import copy
import datatable as dt
import itertools
import random
import sys
import time
import warnings

exhaustive_checks = False

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
        for i in range(rounds):
            print(":", end='', flush=True)
            self.attack_frame(frame)
            if exhaustive_checks:
                frame.check()
            if time.time() - t0 > 60:
                print(">>> Stopped early, taking too long <<<")
                break
        print("\nAttack ended, checking the outcome")
        frame.check()
        print("...ok.")
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
        frame.resize_rows(new_nrows)

    def slice_rows(self, frame):
        s = self.random_slice(frame.nrows)
        new_ncols = len(range(*s.indices(frame.nrows)))
        print("[02] Applying row slice (%r, %r, %r) -> nrows = %d"
              % (s.start, s.stop, s.step, new_ncols))
        frame.slice_rows(s)

    def slice_cols(self, frame):
        s = self.random_slice(frame.ncols)
        new_ncols = len(range(*s.indices(frame.ncols)))
        print("[03] Applying column slice (%r, %r, %r) -> ncols = %d"
              % (s.start, s.stop, s.step, new_ncols))
        frame.slice_cols(s)

    def cbind_self(self, frame):
        if frame.ncols > 1000:
            return self.slice_cols(frame)
        t = random.randint(1, min(5, 100 // (1 + frame.ncols)) + 1)
        print("[04] Cbinding frame with itself %d times -> ncols = %d"
              % (t, frame.ncols * (t + 1)))
        frame.cbind([frame] * t)

    def rbind_self(self, frame):
        t = random.randint(1, min(5, 1000 // (1 + frame.nrows)) + 1)
        print("[05] Rbinding frame with itself %d times -> nrows = %d"
              % (t, frame.nrows * (t + 1)))
        frame.rbind([frame] * t)

    def select_rows_array(self, frame):
        if frame.nrows == 0:
            return
        s = self.random_array(frame.nrows)
        print("[06] Applying a row array -> nrows = %d" % len(s))
        frame.slice_rows(s)

    def delete_rows_array(self, frame):
        if frame.nrows == 0:
            return
        s = self.random_array(frame.nrows, positive=True)
        s = sorted(set(s))
        print("[07] Removing a list of rows -> nrows = %d"
              % (frame.nrows - len(s)))
        frame.delete_rows(s)


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
        data = [self.random_column(nrows, types[i], missing_fraction)
                for i in range(ncols)]
        self.data = data
        self.names = names
        self.types = types
        self.df = dt.Frame(data, names=names, stypes=types)


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


    def random_name(self, alphabet="abcdefghijklmnopqrstuvwxyz"):
        n = int(random.expovariate(0.2) + 1.5)
        while True:
            name = "".join(random.choice(alphabet) for _ in range(n))
            if not(name.isdigit() or name.isspace()):
                return name


    def random_column(self, nrows, ttype, missing):
        if ttype == bool:
            data = self.random_bool_column(nrows)
        elif ttype == int:
            data = self.random_int_column(nrows)
        elif ttype == float:
            data = self.random_float_column(nrows)
        else:
            data = self.random_str_column(nrows)
        if missing:
            for i in range(nrows):
                if random.random() < missing:
                    data[i] = None
        return data


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
        return self.df.nrows

    @property
    def ncols(self):
        return self.df.ncols


    #---------------------------------------------------------------------------
    # Validity checks
    #---------------------------------------------------------------------------

    def check(self):
        self.df.internal.check()
        self.check_shape()
        self.check_types()
        assert self.df.names == tuple(self.names)
        self.check_data()

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


    #---------------------------------------------------------------------------
    # Operations
    #---------------------------------------------------------------------------

    def resize_rows(self, nrows):
        curr_nrows = self.nrows
        self.df.nrows = nrows
        if curr_nrows < nrows:
            append = [None] * (nrows - curr_nrows)
            for i, elem in enumerate(self.data):
                self.data[i] = elem + append
        elif curr_nrows > nrows:
            for i, elem in enumerate(self.data):
                self.data[i] = elem[:nrows]

    def slice_rows(self, s):
        self.df = self.df[s, :]
        if isinstance(s, slice):
            for i in range(self.ncols):
                self.data[i] = self.data[i][s]
        else:
            for i in range(self.ncols):
                col = self.data[i]
                self.data[i] = [col[j] for j in s]

    def delete_rows(self, s):
        del self.df[s, :]
        if isinstance(s, slice):
            s = list(range(self.nrows))[s]
        if isinstance(s, list):
            index = sorted(set(range(self.nrows)) - set(s))
            for i in range(self.ncols):
                col = self.data[i]
                self.data[i] = [col[j] for j in index]

    def slice_cols(self, s):
        self.df = self.df[:, s]
        self.data = self.data[s]
        self.names = self.names[s]
        self.types = self.types[s]

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
        self.df.rbind(*[iframe.df for iframe in frames])
        newdata = [col.copy() for col in self.data]
        for iframe in frames:
            assert iframe.ncols == self.ncols
            for j in range(self.ncols):
                assert self.types[j] == iframe.types[j]
                assert self.names[j] == iframe.names[j]
                newdata[j] += iframe.data[j]
        self.data = newdata


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
                    base += "."
                    num = 0
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
    parser.add_argument("-x", "--exhaustive", action="store_true",
                        help="Run exhaustive checks, i.e. check for validity "
                             "after every step.")
    args = parser.parse_args()
    exhaustive_checks = args.exhaustive

    ra = Attacker(args.seed)
    ra.attack()
