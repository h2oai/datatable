#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#-------------------------------------------------------------------------------
import os
import re
import sys

rx_include = re.compile(r'#include\s+"(.*?)"')
rx_targeth = re.compile(r'^([/\w]+\.h)\s*:\s*(.*)')


def get_files():
    """
    Return the list of all source/header files in `c/` directory.

    The files will have pathnames relative to the current folder, for example
    "c/csv/reader_utils.cc".
    """
    sources = []
    headers = ["datatable/include/datatable.h"]
    assert os.path.isfile(headers[0])
    for dirpath, _, filenames in os.walk("c"):
        for f in filenames:
            fullname = os.path.join(dirpath, f)
            if f.endswith(".h") or f.endswith(".inc"):
                headers.append(fullname)
            elif f.endswith(".c") or f.endswith(".cc"):
                sources.append(fullname)
    return (sources, headers)


def find_includes(filename):
    """
    Find user includes (no system includes) requested from given source file.

    All .h files will be given relative to the current folder, e.g.
    ["c/rowindex.h", "c/column.h"].
    """
    includes = []
    with open(filename, "r", encoding="utf-8") as inp:
        for line in inp:
            line = line.strip()
            if not line or line.startswith("//"):
                continue
            if line.startswith("#"):
                mm = re.match(rx_include, line)
                if mm:
                    includename = os.path.join("c", mm.group(1))
                    includes.append(includename)
    return includes


def build_headermap(headers):
    """
    Construct dictionary {header_file : set_of_included_files}.

    This function operates on "real" set of includes, in the sense that it
    parses each header file to check which files are included from there.
    """
    # TODO: what happens if some headers are circularly dependent?
    headermap = {}
    for hfile in headers:
        headermap[hfile] = None
    for hfile in headers:
        assert (hfile.startswith("c/") or
                hfile.startswith("datatable/include/"))
        inc = find_includes(hfile)
        for f in inc:
            assert f != hfile, "File %s includes itself?" % f
            assert f.startswith("c/")
            if f not in headers:
                raise ValueError("Unknown header \"%s\" included from %s"
                                 % (f, hfile))
        headermap[hfile] = set(inc)
    return headermap


def build_sourcemap(sources):
    """
    Similar to build_headermap(), but builds a dictionary of includes from
    the "source" files (i.e. ".c/.cc" files).
    """
    sourcemap = {}
    for sfile in sources:
        inc = find_includes(sfile)
        sourcemap[sfile] = set(inc)
    return sourcemap


def write_header(out):
    out.write("#" + "-" * 79 + "\n")
    out.write("""#  Copyright 2018 H2O.ai
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in
#  all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
#  IN THE SOFTWARE.
""")
    out.write("#" + "-" * 79 + "\n")
    out.write("# This file is auto-generated from ci/make_fast.py\n")
    out.write("#" + "-" * 79 + "\n")


def write_headers_to_makefile(headermap, out):
    out.write("\n\n")
    out.write("#" + "-" * 79 + "\n")
    out.write("# Header files\n")
    out.write("#" + "-" * 79 + "\n\n")
    for hfile in sorted(headermap.keys()):
        if hfile.startswith("datatable"):
            target = "$(BUILDDIR)/" + hfile
            dependencies = ""
        else:
            target = "$(BUILDDIR)/" + hfile[2:]
            dependencies = " ".join("$(BUILDDIR)/%s" % d[2:]
                                    for d in sorted(headermap[hfile]))
        hdir = os.path.dirname(target)
        out.write("%s: %s %s | %s\n" % (target, hfile, dependencies, hdir))
        out.write("\t@echo • Refreshing %s\n" % hfile)
        out.write("\t@cp %s $@\n" % hfile)
        out.write("\n")


def write_sources_to_makefile(sourcemap, out):
    def header_file(d):
        if d.startswith("c/"): d = d[2:]
        if d.startswith("../"): d = d[3:]
        return d

    out.write("\n\n")
    out.write("#" + "-" * 79 + "\n")
    out.write("# Object files\n")
    out.write("#" + "-" * 79 + "\n\n")
    for ccfile in sorted(sourcemap.keys()):
        assert ccfile.endswith(".cc")
        target = "$(BUILDDIR)/" + ccfile[2:-3] + ".o"
        odir = os.path.dirname(target)
        dependencies = " ".join("$(BUILDDIR)/%s" % header_file(d)
                                for d in sorted(sourcemap[ccfile]))
        out.write("%s: %s %s | %s\n" % (target, ccfile, dependencies, odir))
        out.write("\t@echo • Compiling %s\n" % ccfile)
        out.write("\t@$(CC) -c %s $(CCFLAGS) -o $@\n" % ccfile)
        out.write("\n")


def write_objects_list(sourcemap, out):
    ml = max(len(c) for c in sourcemap) - 2
    out.write("\n\n")
    out.write("fast_objects = %s\\\n" % (" " * (ml + 1)))
    for ccfile in sorted(sourcemap.keys()):
        ofile = ccfile[2:-3] + ".o"
        out.write("\t$(BUILDDIR)/%s%s\\\n" % (ofile, " " * (ml - len(ofile))))


def write_build_directories(realhdrs, realsrcs, out):
    def clean_dir_name(inp):
        if inp.startswith("c/"): inp = inp[2:]
        return inp

    out.write("\n\n")
    out.write("#" + "-" * 79 + "\n")
    out.write("# Build directories\n")
    out.write("#" + "-" * 79 + "\n")
    out.write("\n")
    inputs = list(realhdrs) + list(realsrcs)
    alldirs = set(os.path.dirname("$(BUILDDIR)/" + clean_dir_name(inp))
                  for inp in inputs)
    for target in alldirs:
        out.write("%s:\n" % target)
        out.write("\t@echo • Making directory $@\n")
        out.write("\t@mkdir -p $@\n")
        out.write("\n")


def write_make_targets(out):
    out.write("\n\n")
    out.write("#" + "-" * 79 + "\n")
    out.write("# Main\n")
    out.write("#" + "-" * 79 + "\n")
    out.write(".PHONY: fast main-fast\n")
    out.write("\n")
    out.write("fast:\n")
    out.write("\t$(eval DTDEBUG := 1)\n")
    out.write("\t$(eval export DTDEBUG)\n")
    out.write("\t$(eval CC := $(shell python ci/setup_utils.py compiler))\n")
    out.write("\t$(eval CCFLAGS := $(shell python ci/setup_utils.py ccflags))\n")
    out.write("\t$(eval LDFLAGS := $(shell python ci/setup_utils.py ldflags))\n")
    out.write("\t$(eval EXTEXT := $(shell python ci/setup_utils.py ext_suffix))\n")
    out.write("\t$(eval export CC CCFLAGS LDFLAGS EXTEXT)\n")
    out.write("\t@$(MAKE) main-fast\n")
    out.write("\n")
    out.write("main-fast: $(BUILDDIR)/_datatable.so\n")
    out.write("\t@echo • Done.\n")
    out.write("\n")
    out.write("$(BUILDDIR)/_datatable.so: $(fast_objects)\n")
    out.write("\t@echo • Linking object files into _datatable.so\n")
    out.write("\t@$(CC) $(LDFLAGS) -o $@ $+\n")
    out.write("\t@echo • Copying _datatable.so into ``datatable/lib/_datatable$(EXTEXT)``\n")
    out.write("\t@cp $(BUILDDIR)/_datatable.so datatable/lib/_datatable$(EXTEXT)\n")
    out.write("\n")



def main():
    sources, headers = get_files()
    realhdrs = build_headermap(headers)
    realsrcs = build_sourcemap(sources)
    with open("ci/fast.mk", "wt", encoding="utf-8") as out:
        write_header(out)
        write_build_directories(realhdrs, realsrcs, out)
        write_headers_to_makefile(realhdrs, out)
        write_sources_to_makefile(realsrcs, out)
        write_objects_list(realsrcs, out)
        write_make_targets(out)


if __name__ == "__main__":
    try:
        main()
    except ValueError as e:
        print("\n  Error: %s" % e)
        sys.exit(1)
