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
            if f.endswith(".h"):
                if "flatbuffers" in dirpath: continue
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
                    if mm.group(1) == "flatbuffers/flatbuffers.h": continue
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
        inc = find_includes(hfile)
        for f in inc:
            assert f != hfile, "File %s includes itself?" % f
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


def parse_makefile():
    """
    Parse Makefile in order to detect all declarations related to "fast"
    compilation.
    """
    objects_map = {}
    headers_map = {}
    with open("Makefile", "r", encoding="utf-8") as inp:
        read_objects = False
        for iline, line in enumerate(inp):
            if read_objects:
                line = line.strip(" \t\n\\")
                if line == ")":
                    read_objects = False
                else:
                    objects_map[line] = None
            elif line.startswith("fast_objects"):
                read_objects = True
            elif line.startswith("$(BUILDDIR)/"):
                sline = line.strip()
                sline = sline[len("$(BUILDDIR)/"):]
                obj, deps = sline.split(":")
                obj = obj.strip()
                deps = deps.strip()
                depslist = deps.split(" ")
                if obj.endswith(".so"):
                    continue
                elif obj.endswith(".o"):
                    if obj not in objects_map:
                        raise ValueError("Object file `%s` was not declared in "
                                         "$(fast_objects), however a build "
                                         " targetfor it exists (line %d in "
                                         "Makefile):\n%s"
                                         % (obj, iline + 1, "  " + line))
                    objects_map[obj] = depslist
                elif obj.endswith(".h"):
                    headers_map["$(BUILDDIR)/" + obj] = depslist
        for obj, deps in objects_map.items():
            if deps is None:
                raise ValueError("Object file `%s` is present in "
                                 "$(fast_objects) but there is no "
                                 "corresponding make target" % obj)
            assert isinstance(deps, list), "Invalid deps object: %r" % (deps,)
            if not deps:
                raise ValueError("Dependencies for object file `%s` are "
                                 "missing" % obj)
    return objects_map, headers_map


def sourcefile_to_obj(srcfile):
    assert srcfile.startswith("c/")
    if srcfile.endswith(".c"):
        return srcfile[2:-len(".c")] + ".o"
    elif srcfile.endswith(".cc"):
        return srcfile[2:-len(".cc")] + ".o"
    else:
        raise ValueError("Unexpected source file name: %s" % srcfile)


def headerfile_to_obj(hdrfile):
    assert hdrfile.startswith("c/")
    assert hdrfile.endswith(".h")
    return "$(BUILDDIR)/" + hdrfile[2:]


def verify_dependencies(realsrcs, realhdrs, makeobjs, makehdrs):
    """
    realsrcs:
        dict where keys are names of all .c/.cc files in the project, and values
        are sets of includes in each of those files.
    realhdrs:
        same as `realsrcs`, but keys are .h files.
    makeobjs:
        dictionary where the keys are all object files that are declared in
        the Makefile, and values are all declared dependencies of those
        object files.
    makehdrs:
        dictionary where keys are target names corresponding to each header
        file, and values are sets of dependencies for those targets
    """
    for hdrfile in realhdrs:
        hdrkey = headerfile_to_obj(hdrfile)
        actual_deps = sorted(realhdrs[hdrfile])
        expect_deps = [hdrfile] + [headerfile_to_obj(k) for k in actual_deps]
        if not expect_deps:
            if hdrkey in makehdrs:
                raise ValueError("Target '%s' has no dependencies, and "
                                 "shouldn't be present in Makefile"
                                 % (hdrfile, ))
            continue
        if hdrkey not in makehdrs:
            raise ValueError("Missing target '%s' in Makefile. Add the "
                             "following line:\n%s: %s"
                             % (hdrkey, hdrkey, " ".join(expect_deps)))
        make_deps = makehdrs[hdrkey]
        del makehdrs[hdrkey]
        if set(make_deps) != set(expect_deps):
            raise ValueError("Invalid dependencies for target '%s'. Include "
                             "the following line in Makefile:\n"
                             "%s: %s"
                             % (hdrkey, hdrkey, " ".join(expect_deps)))
    if makehdrs:
        raise ValueError("Makefile has targets %r which do not correspond to "
                         "any existing header files." % list(makehdrs.keys()))

    for srcfile in sorted(realsrcs):
        objkey = sourcefile_to_obj(srcfile)
        actual_deps = sorted(realsrcs[srcfile])
        expect_deps = [srcfile] + [headerfile_to_obj(k) for k in actual_deps]
        if objkey not in makeobjs:
            raise ValueError("Source file `%s` has no corresponding "
                             "`$(BUILDDIR)/%s` target in the Makefile. "
                             "Add the following line:\n$(BUILDDIR)/%s : %s"
                             % (srcfile, objkey, objkey, " ".join(expect_deps)))
        make_deps = makeobjs[objkey]
        del makeobjs[objkey]
        if set(make_deps) != set(expect_deps):
            # print("$(BUILDDIR)/%s : %s" % (objkey, " ".join(expect_deps)))
            # print("\t@echo • Compiling $<")
            # print("\t@$(CC) -c $< $(CCFLAGS) -o $@\n")
            # continue
            raise ValueError("Invalid dependencies for object file '%s'. "
                             "Please include the following line in Makefile:\n"
                             "$(BUILDDIR)/%s : %s"
                             % (objkey, objkey, " ".join(expect_deps)))
    if makeobjs:
        raise ValueError("Makefile has targets %r which do not correspond to "
                         "any existing source files." % list(makeobjs.keys()))


def create_build_directories(objmap):
    dirs = set(os.path.join("build/fast", os.path.dirname(objfile))
               for objfile in objmap)
    for d in dirs:
        os.makedirs(d, exist_ok=True)


def main():
    sources, headers = get_files()
    realhdrs = build_headermap(headers)
    realsrcs = build_sourcemap(sources)
    objmap, makehdrs = parse_makefile()
    objs = list(objmap.keys())
    verify_dependencies(realsrcs, realhdrs, objmap, makehdrs)
    create_build_directories(objs)


if __name__ == "__main__":
    try:
        main()
    except ValueError as e:
        print("\n  Error: %s" % e)
        sys.exit(1)
