#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
#
# For usage information, run:
#
#     python tests/random_driver.py --help
#
#-------------------------------------------------------------------------------
import sys; sys.path = ['.', '..'] + sys.path
import os
import subprocess
import random
import datatable
from datatable.utils.terminal import term

skip_successful_seeds = False
save_logs_to_file = False


def start_random_attack(n_attacks=None, maxfail=None):
    if n_attacks is None:
        n_attacks = 10**10
    if maxfail is None:
        maxfail = n_attacks
    n_tests = 0
    n_errors = 0
    try:
        for i in range(n_attacks):
            seed = random.getrandbits(63)
            ret = try_seed(seed)
            n_tests += 1
            n_errors += not ret
            if n_errors >= maxfail:
                raise KeyboardInterrupt
            if skip_successful_seeds:
                print(term.color("bold", " Seeds tried: %d" % (i + 1)),
                      end="\r")
    except KeyboardInterrupt:
        errmsg = "errors: %d" % n_errors
        if n_errors:
            errmsg = term.color("bright_red", errmsg)
        print("\r" + term.color("bright_cyan", "DONE.") +
              " Seeds tested: %d, %s" % (n_tests, errmsg))


def try_seed(seed):
    utf8_env = os.environ
    utf8_env['PYTHONIOENCODING'] = 'utf-8'
    proc = subprocess.Popen([sys.executable, "random_attack.py", str(seed)],
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
        status = term.color("bright_green", "OK")
    elif rc > 0:
        status = term.color("yellow", "FAIL")
    elif rc == -9:
        status = term.color("cyan", "HANG")
    else:
        status = term.color("bright_red", "ABORT")
    if rc != 0:
        status += " (%d)" % rc
    print("%-19d: %s" % (seed, status))

    if save_logs_to_file:
        write_to_file("random_attack_logs/%d.txt" % seed, out, err)
    else:
        write_to_screen(out, err)
    return rc == 0


def write_to_file(filename, out, err):
    os.makedirs("random_attack_logs", exist_ok=True)
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
                    "package. This script will invoke random_attack.py "
                    "with different seeds, and record results of each "
                    "execution. The log from each failing run will be stored "
                    "in `tests/random_attack_logs` directory.")
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

    args = parser.parse_args()
    skip_successful_seeds = not args.showall
    save_logs_to_file = not args.verbose

    if not os.path.exists("random_attack.py"):
        if os.path.exists("tests/random_attack.py"):
            os.chdir("tests")
        else:
            raise SystemExit("File random_attack.py not found")
    start_random_attack(n_attacks=args.ntests, maxfail=args.maxfail)
