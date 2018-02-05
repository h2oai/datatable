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


def get_files():
    sources = []
    headers = []
    for dirpath, _, filenames in os.walk("c"):
        for f in filenames:
            fullname = os.path.join(dirpath, f)
            if f.endswith(".h"):
                headers.append(fullname)
            elif f.endswith(".c") or f.endswith(".cc"):
                sources.append(fullname)
    return (sources, headers)


def find_includes(filename):
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
    headermap = {}
    for hfile in headers:
        headermap[hfile] = None
    for hfile in headers:
        inc = find_includes(hfile)
        for f in inc:
            assert f != hfile, "File %s includes itself?" % f
            if f not in headers:
                raise ValueError("Unknown header \"%s\" included from %s"
                                 % (f, hfile))
        headermap[hfile] = set(inc)
    transitively_extend(headermap)
    return headermap


def build_sourcemap(sources, headermap):
    sourcemap = {}
    for sfile in sources:
        inc = find_includes(sfile)
        for f in inc:
            if f not in headermap:
                raise ValueError("Unknown header \"%s\" included from %s"
                                 % (f, sfile))
        sourcemap[sfile] = set(inc)
    return sourcemap


def transitively_extend(nodemap):
    modified = True
    while modified:
        modified = False
        for node in nodemap:
            values = nodemap[node]
            len0 = len(values)
            for n in list(values):
                values |= nodemap[n]
            if len(values) > len0:
                modified = True
            nodemap[node] = values


def parse_makefile():
    objects_map = {}
    with open("Makefile", "r", encoding="utf-8") as inp:
        read_objects = False
        for iline, line in enumerate(inp):
            if read_objects:
                line = line.strip(" \t\n\\")
                if line == ")":
                    read_objects = False
                else:
                    objects_map[line] = None
                continue
            if line.startswith("fast_objects"):
                read_objects = True
                continue
            if line.startswith("$(BUILDDIR)/"):
                sline = line.strip()
                sline = sline[len("$(BUILDDIR)/"):]
                obj, deps = sline.split(":")
                obj = obj.strip()
                if obj == "_datatable.so":
                    continue
                if obj not in objects_map:
                    raise ValueError("Object file `%s` was not declared in "
                                     "$(fast_objects), however a build target "
                                     "for it exists (line %d in Makefile):\n%s"
                                     % (obj, iline + 1, "  " + line))
                deps = deps.strip()
                depslist = deps.split(" ")
                objects_map[obj] = depslist
        for obj, deps in objects_map.items():
            if deps is None:
                raise ValueError("Object file `%s` is present in "
                                 "$(fast_objects) but there is no "
                                 "corresponding make target" % obj)
            assert isinstance(deps, list), "Invalid deps object: %r" % (deps,)
            if not deps:
                raise ValueError("Dependencies for object file `%s` are "
                                 "missing" % obj)
    return objects_map


def verify_dependencies(sourcemap, objmap):
    # `sourcemap`:
    #     dict where keys are all .c/.cc filenames in the project, and values
    #     are sets of includes in each of those files.
    # `headermap`:
    #     same as `sourcemap`, but keys are .h files.
    # `objmap`:
    #     dictionary where the keys are all object files that are declared in
    #     the Makefile, and values are all declared dependencies of those
    #     object files.
    for srcfile in sourcemap:
        assert srcfile.startswith("c/")
        f = srcfile[len("c/"):]
        if f.endswith(".c"):
            f = f[:-len(".c")] + ".o"
        elif f.endswith(".cc"):
            f = f[:-len(".cc")] + ".o"
        else:
            raise ValueError("Unexpected source file name: %s" % srcfile)
        if f not in objmap:
            raise ValueError("Source file `%s` has no corresponding "
                             "`$(BUILDDIR)/%s` target in the Makefile."
                             % (srcfile, f))
    for objfile in objmap:
        deps = objmap[objfile]
        srcfile = deps[0]
        makefile_deps = set(deps[1:])
        if srcfile not in sourcemap:
            raise ValueError("Unknown dependency `%s` stated for object file "
                             "`%s` in the Makefile" % (srcfile, objfile))
        actual_deps = sourcemap[srcfile]
        if makefile_deps != actual_deps:
            diff1 = makefile_deps - actual_deps
            diff2 = actual_deps - makefile_deps
            if diff1:
                raise ValueError("Source file `%s` is declared in the Makefile "
                                 "to depend on %r, while in reality it doesn't "
                                 "depend on them." % (srcfile, diff1))
            if diff2:
                raise ValueError("Source file `%s` depends on %r, however these"
                                 " dependencies were not declared in the "
                                 "Makefile.\n"
                                 "Include the following line in the Makefile:\n"
                                 "$(BUILDDIR)/%s : %s %s"
                                 % (srcfile, diff2, objfile, srcfile,
                                    " ".join(sorted(actual_deps))))


def create_build_directories(objmap):
    dirs = set(os.path.join("build/fast", os.path.dirname(objfile))
               for objfile in objmap)
    for d in dirs:
        os.makedirs(d, exist_ok=True)


def main():
    sources, headers = get_files()
    headermap = build_headermap(headers)
    sourcemap = build_sourcemap(sources, headermap)
    objmap = parse_makefile()
    verify_dependencies(sourcemap, objmap)
    create_build_directories(objmap)


if __name__ == "__main__":
    try:
        main()
    except ValueError as e:
        print("\n  Error: %s" % e)
        sys.exit(1)
