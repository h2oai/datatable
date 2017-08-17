#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import re
import subprocess
import sys


if __name__ == "__main__":
    headers_printed = False
    ns = (list(range(2, 17)) +
          [20, 24, 28, 32, 48, 64, 96, 128, 150, 200, 256])
    ns = [48, 64, 96, 128, 150, 200, 256]
    # ns = [500, 1000, 2500, 5000, 10000]
    ns = list(range(100, 200))
    for n in ns:
        cmd = [c.replace("{N}", str(n)) for c in sys.argv[1:]]
        try:
            out = subprocess.check_output(cmd).decode()
        except subprocess.CalledProcessError as e:
            print(" %-7d | %s" % (n, e))
            continue
        headers = []
        values = []
        for line in out.split("\n"):
            mm = re.search(r"^@\s*(\w+).*?\b(\d+(?:\.\d+)?)\b", line)
            if mm:
                name = mm.group(1)
                value = mm.group(2)
                headers.append(name)
                values.append(value)
        if not headers_printed:
            headers_printed = True
            print(" n" + " " * 7, end="")
            for h in headers:
                print("| %12s " % h, end="")
            print()
            print("-" * 9, end="")
            for s in range(len(headers)):
                print("+" + "-" * 14, end="")
            print()
        print(" %-7d " % n, end="")
        for v in values:
            print("| %12s " % v, end="")
        print()
