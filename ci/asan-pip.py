#!/usr/bin/env python
import os

target = os.environ.get("DT_ASAN_TARGETDIR")

text = """#!/usr/bin/env python
import sys
try:
    from pip._internal import main
except ImportError:
    from pip import main

if __name__ == "__main__":
    sys.argv.append("--target=%s")
    ret = main()
    sys.exit(ret)
""" % (target,)

for root, dirs, files in os.walk(os.path.join(target, "..", "..", "..", "bin")):
    for f in files:
        if f.startswith("pip"):
            with open(os.path.join(root, f), "w") as out:
                out.write(text)
