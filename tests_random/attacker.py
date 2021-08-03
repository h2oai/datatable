#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2021 H2O.ai
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
import pytest
import random
import re
import time
from datatable.lib import core
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from tests_random.metaframe import MetaFrame
from tests_random.methods import MethodsLibrary, EvaluationContext
from tests_random.utils import (
    random_array,
    random_slice,
    random_string
)


#---------------------------------------------------------------------------
# Main Attacker class
#---------------------------------------------------------------------------

class Attacker:

    def __init__(self, seed=None, exhaustive_checks=False, allow_forks=True):
        if seed is None:
            seed = random.getrandbits(64)
        self._seed = seed
        self._exhaustive_checks = exhaustive_checks
        self._allow_forks = allow_forks
        random.seed(seed)
        print("# Seed: %r\n" % seed)

    def attack(self, frame=None, rounds=None):
        t0 = time.time()
        if rounds is None:
            rounds = int(random.expovariate(0.05) + 2)
        assert isinstance(rounds, int)

        print("# Launching an attack for %d rounds" % rounds)
        context = EvaluationContext()
        for i in range(rounds):
            action = MethodsLibrary.random_action(context)
            if action.skipped:
                print(f"# SKIPPED: {action.__class__.__name__}")
            elif action.raises:
                print(f"# RAISES: ", end="")
                action.log_to_console()
                msg = re.escape(action.error_message)
                with pytest.raises(action.raises, match=msg):
                    action.apply_to_dtframe()
            else:
                action.log_to_console()
                action.apply_to_dtframe()
                action.apply_to_pyframe()

            # elif self._allow_forks:
            #     # Non-standard actions
            #     # fork_and_run(frame, rounds - i)
            #     break
            if self._exhaustive_checks:
                context.check()
            if time.time() - t0 > 60:
                print(">>> Stopped early, taking too long <<<")
                break
        print("\nAttack ended, checking the outcome... ", end='')
        context.check_all()
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
    parser.add_argument("--noforks", action="store_true",
                        help="Disable fork action in random attacker")
    args = parser.parse_args()
    ra = Attacker(seed=args.seed,
                  exhaustive_checks=args.exhaustive,
                  allow_forks=not args.noforks)
    ra.attack()
