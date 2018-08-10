#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
    headers = []
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
        assert hfile.startswith("c/")
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
    out.write("# © H2O.ai 2018; -*- encoding: utf-8 -*-\n")
    out.write("#   This Source Code Form is subject to the terms of the\n")
    out.write("#   Mozilla Public License, v. 2.0. If a copy of the MPL\n")
    out.write("#   was not distributed with this file, You can obtain one\n")
    out.write("#   at http://mozilla.org/MPL/2.0/.\n")
    out.write("#" + "-" * 79 + "\n")
    out.write("# This file is auto-generated from ci/make_fast.py\n")
    out.write("#" + "-" * 79 + "\n")


def write_headers_to_makefile(headermap, out):
    out.write("\n\n")
    out.write("#" + "-" * 79 + "\n")
    out.write("# Header files\n")
    out.write("#" + "-" * 79 + "\n\n")
    for hfile in sorted(headermap.keys()):
        target = "$(BUILDDIR)/" + hfile[2:]
        hdir = os.path.dirname(target)
        dependencies = " ".join("$(BUILDDIR)/%s" % d[2:]
                                for d in sorted(headermap[hfile]))
        out.write("%s: %s %s | %s\n" % (target, hfile, dependencies, hdir))
        out.write("\t@echo • Refreshing $<\n")
        out.write("\t@cp $< $@\n")
        out.write("\n")


def write_sources_to_makefile(sourcemap, out):
    out.write("\n\n")
    out.write("#" + "-" * 79 + "\n")
    out.write("# Object files\n")
    out.write("#" + "-" * 79 + "\n\n")
    for ccfile in sorted(sourcemap.keys()):
        assert ccfile.endswith(".cc")
        target = "$(BUILDDIR)/" + ccfile[2:-3] + ".o"
        odir = os.path.dirname(target)
        dependencies = " ".join("$(BUILDDIR)/%s" % d[2:]
                                for d in sorted(sourcemap[ccfile]))
        out.write("%s: %s %s | %s\n" % (target, ccfile, dependencies, odir))
        out.write("\t@echo • Compiling $<\n")
        out.write("\t@$(CC) -c $< $(CCFLAGS) -o $@\n")
        out.write("\n")


def write_objects_list(sourcemap, out):
    ml = max(len(c) for c in sourcemap) - 2
    out.write("\n\n")
    out.write("fast_objects = %s\\\n" % (" " * (ml + 1)))
    for ccfile in sorted(sourcemap.keys()):
        ofile = ccfile[2:-3] + ".o"
        out.write("\t$(BUILDDIR)/%s%s\\\n" % (ofile, " " * (ml - len(ofile))))


def write_build_directories(realhdrs, realsrcs, out):
    out.write("\n\n")
    out.write("#" + "-" * 79 + "\n")
    out.write("# Build directories\n")
    out.write("#" + "-" * 79 + "\n")
    out.write("\n")
    inputs = list(realhdrs) + list(realsrcs)
    alldirs = set(os.path.dirname("$(BUILDDIR)/" + inp[2:])
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
    out.write("\t$(eval CC := $(shell python setup.py get_CC))\n")
    out.write("\t$(eval CCFLAGS := $(shell python setup.py get_CCFLAGS))\n")
    out.write("\t$(eval LDFLAGS := $(shell python setup.py get_LDFLAGS))\n")
    out.write("\t$(eval EXTEXT := $(shell python setup.py get_EXTEXT))\n")
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
