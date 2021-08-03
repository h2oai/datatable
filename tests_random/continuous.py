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
#
# For usage information, run:
#
#     python tests_random/continuous.py --help
#
#-------------------------------------------------------------------------------
import sys
import os
import subprocess
import random
from datatable.lib import core



def start_random_attack(n_attacks=None,
                        maxfail=None,
                        skip_successful_seeds=False,
                        save_logs_to_file=False,
                        no_forks=False):
    if n_attacks is None:
        n_attacks = 10**10
    if maxfail is None:
        maxfail = n_attacks
    if save_logs_to_file:
        log_dir = os.path.join(os.path.dirname(__file__), "logs")
    else:
        log_dir = None

    n_tests = 0
    n_errors = 0
    try:
        for i in range(n_attacks):
            seed = random.getrandbits(63)
            is_success = try_seed(seed, skip_successful_seeds, log_dir, no_forks)
            n_tests += 1
            n_errors += not is_success
            if n_errors >= maxfail:
                raise KeyboardInterrupt
            if skip_successful_seeds:
                print(core.apply_color("bold", " Seeds tried: %d" % (i + 1)),
                      end="\r")
    except KeyboardInterrupt:
        errmsg = "errors: %d" % n_errors
        if n_errors:
            errmsg = core.apply_color("bright_red", errmsg)
        print("\r" + core.apply_color("bright_cyan", "DONE.") +
              " Seeds tested: %d, %s" % (n_tests, errmsg))



def try_seed(seed, skip_successful_seeds, log_dir, no_forks):
    utf8_env = os.environ
    utf8_env['PYTHONIOENCODING'] = 'utf-8'
    script = os.path.join(os.path.dirname(__file__), "attacker.py")
    proc = subprocess.Popen([sys.executable, script, str(seed)] +
                            ["--noforks"] * no_forks,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            env=utf8_env)
    try:
        out, err = proc.communicate(timeout=100)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, err = proc.communicate()
    rc = proc.returncode
    if rc == 0 and skip_successful_seeds:
        return True

    if rc == 0:
        status = core.apply_color("bright_green", "OK")
    elif rc > 0:
        status = core.apply_color("yellow", "FAIL")
    elif rc == -9:
        status = core.apply_color("cyan", "HANG")
    else:
        status = core.apply_color("bright_red", "ABORT")
    if rc != 0:
        status += " (%d)" % rc
    print("%-19d: %s" % (seed, status))

    if log_dir is None:
        write_to_screen(out, err)
    else:
        os.makedirs(log_dir, exist_ok=True)
        log_file = os.path.join(log_dir, "%d.txt" % seed)
        write_to_file(log_file, out, err)
    return rc == 0


def write_to_file(filename, out, err):
    with open(filename, "wb") as o:
        if out:
            o.write(b"---- STDOUT -----------------------------------------\n")
            o.write(out)
            o.write(b"\n")
        if err:
            o.write(b"---- STDERR -----------------------------------------\n")
            o.write(err)
            o.write(b"\n")


def write_to_screen(out, err):
    if out:
        print("    ---- STDOUT ----")
        for l in out.decode("utf-8").split("\n"):
            print("    " + l)
    if err:
        print("    ---- STDERR ----")
        for l in err.decode("utf-8").split("\n"):
            print("    " + l)



if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="Perform randomized integration tests for datatable "
                    "package. This script will invoke attacker.py with "
                    "different seeds, and record results of each execution. "
                    "The log from each failing run will be stored in "
                    "`tests_random/logs` directory.")
    parser.add_argument("-n", "--ntests", type=int,
                        help="Number of tests to perform; by default will "
                             "run until stopped with Ctrl+C")
    parser.add_argument("-m", "--maxfail", type=int,
                        help="Stop when this many failing tests are found")
    parser.add_argument("-a", "--showall", action="store_true",
                        help="Display all tests that are run, even the "
                             "successful ones")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Show log messages for each test run. If this "
                             "flag is not set, the messages will be stored in "
                             "a file")
    parser.add_argument("--noforks", action="store_true",
                        help="Disable fork action in random attacker")

    args = parser.parse_args()
    start_random_attack(n_attacks=args.ntests,
                        maxfail=args.maxfail,
                        skip_successful_seeds=not args.showall,
                        save_logs_to_file=not args.verbose,
                        no_forks=args.noforks)
