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
import time
from datatable.lib import core
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from tests_random.metaframe import MetaFrame
from tests_random.utils import (
    repr_slice,
    random_array,
    random_slice,
    random_string
)


#---------------------------------------------------------------------------
# Main Attacker class
#---------------------------------------------------------------------------

class Attacker:

    def __init__(self, seed=None, exhaustive_checks=False):
        if seed is None:
            seed = random.getrandbits(64)
        self._seed = seed
        self._exhaustive_checks = exhaustive_checks
        random.seed(seed)
        print("# Seed: %r\n" % seed)

    def attack(self, frame=None, rounds=None):
        t0 = time.time()
        if rounds is None:
            rounds = int(random.expovariate(0.05) + 2)
        assert isinstance(rounds, int)
        if frame is None:
            frame = MetaFrame.random()
        print("# Launching an attack for %d rounds" % rounds)
        for i in range(rounds):
            action = random.choices(population=ATTACK_METHODS,
                                    cum_weights=ATTACK_WEIGHTS, k=1)[0]
            if action:
                action(frame)
            else:
                # Non-standard actions
                fork_and_run(frame, rounds - i)
                break
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


def fattack(filename, seed, nrounds):
    """
    Entry-point function for a forked process.
    """
    print("Entering child process")
    frame = MetaFrame.load_from_jay(filename)
    attacker = Attacker(seed=seed)
    attacker.attack(frame, nrounds)



#---------------------------------------------------------------------------
# Individual attack methods
#---------------------------------------------------------------------------

def resize_rows(frame):
    t = random.random()
    curr_nrows = frame.nrows
    new_nrows = int(curr_nrows * 10 / (19 * t + 1) + 1)
    frame.resize_rows(new_nrows)


def slice_rows(frame):
    s = random_slice(frame.nrows)
    new_nrows = len(range(*s.indices(frame.nrows)))
    frame.slice_rows(s)


def slice_columns(frame):
    s = random_slice(frame.ncols)
    new_ncols = len(range(*s.indices(frame.ncols)))
    frame.slice_cols(s)


def cbind_self(frame):
    if frame.ncols > 1000:
        return slice_columns(frame)
    t = random.randint(1, min(5, 100 // (1 + frame.ncols)) + 1)
    frame.cbind([frame] * t)


def rbind_self(frame):
    t = random.randint(1, min(5, 1000 // (1 + frame.nrows)) + 1)
    res = frame.rbind([frame] * t)


def select_rows_array(frame):
    if frame.nrows == 0:
        return
    s = random_array(frame.nrows)
    frame.slice_rows(s)


def delete_rows_array(frame):
    if frame.nrows == 0:
        return
    s = random_array(frame.nrows, positive=True)
    s = sorted(set(s))
    frame.delete_rows(s)


def select_rows_with_boolean_column(frame):
    bool_columns = [i for i in range(frame.ncols) if frame.types[i] is bool]
    if frame.ncols <= 1 or not bool_columns:
        return
    filter_column = random.choice(bool_columns)
    frame.filter_on_bool_column(filter_column)


def replace_nas_in_column(frame):
    icol = random.randint(0, frame.ncols - 1)
    if frame.types[icol] is bool:
        replacement_value = random.choice([True, False])
    elif frame.types[icol] is int:
        replacement_value = random.randint(-100, 100)
    elif frame.types[icol] is float:
        replacement_value = random.random() * 1000
    elif frame.types[icol] is str:
        replacement_value = random_string()
    res = frame.replace_nas_in_column(icol, replacement_value)


def sort_columns(frame):
    if frame.ncols == 0:
        return
    ncols_sort = min(int(random.expovariate(1.0)) + 1, frame.ncols)
    a = random.sample(range(0, frame.ncols), ncols_sort)
    frame.sort_columns(a)


def cbind_numpy_column(frame):
    frame.cbind_numpy_column()


def add_range_column(frame):
    start = int(random.expovariate(0.05) - 5)
    step = 0
    while step == 0:
        step = int(1 + random.random() * 3)
    stop = start + step * frame.nrows
    rangeobj = range(start, stop, step)
    name = random_string()
    frame.add_range_column(name, rangeobj)


def set_key_columns(frame):
    if frame.ncols == 0:
        return

    nkeys = min(int(random.expovariate(1.0)) + 1, frame.ncols)
    keys = random.sample(range(0, frame.ncols), nkeys)
    names = [frame.names[i] for i in keys]

    frame.set_key_columns(keys, names)


def delete_columns_array(frame):
    # datatable supports a shape of n x 0 for non-zero n's, while
    # python doesn't, so we never remove all the columns from a Frame.
    if frame.ncols < 2:
        return
    s = random_array(frame.ncols - 1, positive=True)
    frame.delete_columns(s)


def join_self(frame):
    if frame.ncols > 1000:
        return slice_columns(frame)

    frame.join_self()


def shallow_copy(frame):
    frame.shallow_copy()


def fork_and_run(frame, nrounds):
    import multiprocessing as mp
    import tempfile
    print(f"Forking the parent process")
    filename = tempfile.mktemp(suffix=".jay")
    seed = random.getrandbits(64)
    try:
        frame.save_to_jay(filename)
        print("Starting mp.Process for fattack(%r, %r, %r)"
              % (filename, seed, nrounds))
        process = mp.Process(target=fattack, args=(filename, seed, nrounds))
        process.start()
        process.join()
        ret = process.exitcode
        if ret != 0:
            raise RuntimeError(f"Child terminated with exit code {ret}")
    finally:
        os.remove(filename)



#---------------------------------------------------------------------------
# Helpers
#---------------------------------------------------------------------------

_METHODS = {
    None: 1,
    resize_rows: 1,
    slice_rows: 3,
    slice_columns: 0.5,
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
ATTACK_WEIGHTS = list(itertools.accumulate(_METHODS.values()))
ATTACK_METHODS = list(_METHODS.keys())
del _METHODS



#---------------------------------------------------------------------------
# Script runner
#---------------------------------------------------------------------------

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="Perform single batch of randomized testing. The SEED "
                    "argument is required. Use this script for troubleshooting "
                    "a particular seed value, otherwise run `continuous.py`."
    )
    parser.add_argument("seed", type=int, metavar="SEED")
    parser.add_argument("-x", "--exhaustive", action="store_true",
                        help="Run exhaustive checks, i.e. check for validity "
                             "after every step.")
    args = parser.parse_args()
    ra = Attacker(args.seed, args.exhaustive)
    ra.attack()
