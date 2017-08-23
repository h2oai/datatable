#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import re
import subprocess
import sys
import colorama


if __name__ == "__main__":
    colorama.init()
    headers_printed = []
    ns = (list(range(2, 17)) + list(range(20, 100, 4)) +
          [100, 128, 160, 196, 256])
    if "algo=4" in sys.argv:
        ns += [1 << i for i in range(9, 23)]
        ns += [3 << i for i in range(7, 22)]
        ns.sort()

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
            mm = re.search(r"^@\s*([\w\-/]+):\s*([\d\.\-]+)", line)
            if mm:
                name = mm.group(1)
                value = mm.group(2)
                headers.append(name)
                if value == "-":
                    values.append(None)
                else:
                    v = float(value)
                    values.append(int(v + 0.5))
        if headers_printed:
            assert headers_printed == headers
        else:
            headers_printed = list(headers)
            divider = "+".join(["-" * 9] + ["-" * 14] * len(headers))
            print(divider)
            print(" n" + " " * 7, end="")
            for h in headers:
                print("| %12s " % h, end="")
            print()
            print(divider)
        print(" %-7d " % n, end="")
        minval = min(v for v in values if v is not None)
        for v in values:
            if v is None:
                print("|" + " " * 14, end="")
            elif v == minval:
                print("|" + colorama.Fore.GREEN + (" %12s " % v) +
                      colorama.Style.RESET_ALL, end="")
            else:
                print("| %12d " % v, end="")
        print()
