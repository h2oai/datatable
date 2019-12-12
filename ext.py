#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2019 H2O.ai
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
from ci import xbuild

_valid_commands = ["asan", "build", "coverage", "debug"]


def build_extension(cmd, verbosity=3):
    assert cmd in _valid_commands
    windows = (sys.platform == "win32")
    macos = (sys.platform == "darwin")
    linux = (sys.platform == "linux")
    if not (windows or macos or linux):
        print("\x1b[93mWarning: unknown platform %s\x1b[m" % sys.platform)
        linux = True

    ext = xbuild.Extension()
    ext.log = xbuild.Logger0() if verbosity == 0 else \
              xbuild.Logger1() if verbosity == 1 else \
              xbuild.Logger2() if verbosity == 2 else \
              xbuild.Logger3()
    ext.name = "_datatable"
    ext.build_dir = "build/" + cmd
    ext.destination_dir = "datatable/lib/"
    ext.add_sources("c/**/*.cc")

    # Common compile settings
    ext.compiler.enable_colors()
    ext.compiler.add_include_dir("c")
    ext.compiler.add_default_python_include_dir()

    if ext.compiler.is_msvc():
        ext.compiler.add_compiler_flag("/W4")

    else:
        # Common compile flags
        ext.compiler.add_compiler_flag("-std=c++11")
        # "-stdlib=libc++"  (clang ???)
        ext.compiler.add_compiler_flag("-fPIC")

        # Common link flags
        ext.compiler.add_linker_flag("-bundle")
        ext.compiler.add_linker_flag("-undefined", "dynamic_lookup")
        ext.compiler.add_linker_flag("-g")
        ext.compiler.add_linker_flag("-m64")
        # "-lc++"    (linux & clang ???)
        # "-shared"  (linux ???)
        # "-lstdc++" (gcc ???)
        # "-lm"      (gcc ???)
        # "-lz"      (???)

        if cmd == "asan":
            ext.compiler.add_compiler_flag("-fsanitize=address")
            ext.compiler.add_compiler_flag("-fno-omit-frame-pointer")
            ext.compiler.add_compiler_flag("-fsanitize-address-use-after-scope")
            ext.compiler.add_compiler_flag("-shared-libasan")
            ext.compiler.add_compiler_flag("-g3")
            ext.compiler.add_compiler_flag("-glldb" if macos else "-ggdb")
            ext.compiler.add_compiler_flag("-O0")
            ext.compiler.add_compiler_flag("-DDTTEST", "-DDTDEBUG")
            ext.compiler.add_linker_flag("-fsanitize=address", "-shared-libasan")

        if cmd == "build":
            ext.compiler.add_compiler_flag("-g2")  # include some debug info
            ext.compiler.add_compiler_flag("-O3")  # full optimization

        if cmd == "coverage":
            ext.compiler.add_compiler_flag("-g2")
            ext.compiler.add_compiler_flag("-O0")
            ext.compiler.add_compiler_flag("--coverage")
            ext.compiler.add_compiler_flag("-DDTTEST", "-DDTDEBUG")
            ext.compiler.add_linker_flag("-O0")
            ext.compiler.add_linker_flag("--coverage")

        if cmd == "debug":
            ext.compiler.add_compiler_flag("-g3")
            ext.compiler.add_compiler_flag("-fdebug-macro")
            ext.compiler.add_compiler_flag("-glldb" if macos else "-ggdb")
            ext.compiler.add_compiler_flag("-O0")  # no optimization
            ext.compiler.add_compiler_flag("-DDTTEST", "-DDTDEBUG")

        # Compiler warnings
        if ext.compiler.is_clang():
            ext.compiler.add_compiler_flag(
                "-Weverything",
                "-Wno-c++98-compat-pedantic",
                "-Wno-c99-extensions",
                "-Wno-exit-time-destructors",
                "-Wno-float-equal",
                "-Wno-global-constructors",
                "-Wno-reserved-id-macro",
                "-Wno-switch-enum",
                "-Wno-weak-template-vtables",
                "-Wno-weak-vtables",
            )
        else:
            ext.compiler.add_compiler_flag(
                "-Wall",
                "-Wno-unused-value",
                "-Wno-unknown-pragmas"
            )

    # Setup is complete, ready to build
    ext.build()


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description='Build _datatable module')
    parser.add_argument("cmd", metavar="CMD", choices=_valid_commands,
            help="Mode of the _datatable library to build. The following modes "
                 "are supported: `asan` (built with Address Sanitizer), `build`"
                 " (standard build mode), `coverage` (suitable for coverage "
                 "testing), and `debug` (with full debug info).")

    args = parser.parse_args()
    with open("datatable/lib/.xbuild-cmd", "wt") as out:
        out.write(args.cmd)

    build_extension(cmd=args.cmd, verbosity=3)
