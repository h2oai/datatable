#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable as dt
import itertools
import random
import sys


class RandomAttacker:

    def __init__(self, seed=None):
        if seed is None:
            seed = random.getrandbits(64)
        self._seed = seed
        random.seed(seed)

    def attack(self, frame=None, rounds=None):
        if rounds is None:
            rounds = int(random.expovariate(0.05) + 2)
        assert isinstance(rounds, int)
        if frame is None:
            frame = Frame0()
        for i in range(rounds):
            self.attack_frame(frame)
        frame.check()

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
        print("Setting nrows to %d" % new_nrows)
        frame.resize_rows(new_nrows)

    def slice_rows(self, frame):
        t = random.random()
        s = random.random()
        nrows = frame.nrows
        row0 = int(nrows * t * 0.2) if t > 0.5 else None
        row1 = int(nrows * (s * 0.2 + 0.8)) if s > 0.5 else None
        if row0 is not None and row1 is not None and row1 < row0:
            row0, row1 = row1, row0
        step = random.choice([-2, -1, -1, 1, 1, 1, 1, 2, 2, 2, 3, 3, 4, 5, 7])
        if step < 0:
            row0, row1 = row1, row0
        if len(range(*slice(row0, row1, step).indices(nrows))) == 0:
            return
        print("Applying row slice (%r, %r, %r)" % (row0, row1, step))
        frame.slice_rows(row0, row1, step)


    ATTACK_METHODS = {
        resize_rows: 1,
        slice_rows: 3,
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
        assert all(t in [bool, int, float, str] for t in types)
        data = [self.random_column(nrows, types[i], missing_fraction)
                for i in range(ncols)]
        self.data = data
        self.names = tuple(names)
        self.types = tuple(types)
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
        l = int(random.expovariate(0.2) + 1.5)
        while True:
            name = "".join(random.choice(alphabet) for _ in range(l))
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
    # Operations
    #---------------------------------------------------------------------------

    def check(self):
        assert self.df.nrows == len(self.data[0]) if self.data else 0
        assert self.df.ncols == len(self.data)
        assert self.df.names == self.names
        self.check_types()
        assert self.df.topython() == self.data

    def check_types(self):
        df_ltypes = self.df.ltypes
        py_ltypes = tuple(dt.ltype(x) for x in self.types)
        if df_ltypes != py_ltypes:
            print("ERROR: types mismatch")
            print("  dt types: %r" % (df_ltypes, ))
            print("  py types: %r" % (py_ltypes, ))
            sys.exit(1)

    def resize_rows(self, nrows):
        curr_nrows = self.nrows
        self.df.nrows = nrows
        if curr_nrows == 1:
            for elem in self.data:
                elem *= nrows
        elif curr_nrows < nrows:
            append = [None] * (nrows - curr_nrows)
            for elem in self.data:
                elem += append
        elif curr_nrows > nrows:
            for i in range(len(self.data)):
                self.data[i] = self.data[i][:nrows]

    def slice_rows(self, start, stop, step):
        self.df = self.df[start:stop:step, :]
        for i in range(self.ncols):
            self.data[i] = self.data[i][start:stop:step]



if __name__ == "__main__":
    seed = sys.argv[1]
    assert seed, "seed is missing"
    assert seed.isdigit(), "seed is not a number: %s" % seed
    ra = RandomAttacker(seed)
    ra.attack()
