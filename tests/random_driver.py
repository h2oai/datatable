#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import blessed
import os
import subprocess
import random

skip_successful_seeds = True
save_logs_to_file = False


def start_random_attack():
    term = blessed.Terminal()
    n_seeds_tried = 0
    while True:
        seed = random.getrandbits(63)
        try_seed(seed, term)
        n_seeds_tried += 1
        if skip_successful_seeds:
            print(term.bold_bright_white(" Seeds tried: %d" % n_seeds_tried),
                  end="\r")


def try_seed(seed, term):
    proc = subprocess.Popen(["python", "random_attack.py", str(seed)],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        out, err = proc.communicate(timeout=30)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, err = proc.communicate()
    rc = proc.returncode
    if rc == 0:
        if skip_successful_seeds:
            return
    else:
        if save_logs_to_file:
            write_to_file("random_attack_logs/%d.txt" % seed, out, err)
        else:
            write_to_screen(out, err)

    if rc == 0:
        status = "OK   "
    elif rc > 0:
        status = term.yellow("FAIL ")
    elif rc == -9:
        status = term.cyan("HANG ")
    else:
        status = term.bright_red("ABORT")
    print("%-19d: %s  (%d)" % (seed, status, rc))


def write_to_file(filename, out, err):
    os.makedirs("random_attack_logs", exist_ok=True)
    with open(filename, "wb") as o:
        o.write(b"---- STDOUT -----------------------------------------\n")
        o.write(out)
        o.write(b"\n")
        o.write(b"---- STDERR -----------------------------------------\n")
        o.write(err)
        o.write(b"\n")


def write_to_screen(out, err):
    print("    ---- STDOUT ----")
    for l in out.decode("utf-8").split("\n"):
        print("    " + l)
    print()
    print("    ---- STDERR ----")
    for l in err.decode("utf-8").split("\n"):
        print("    " + l)
    print()


if __name__ == "__main__":
    start_random_attack()
