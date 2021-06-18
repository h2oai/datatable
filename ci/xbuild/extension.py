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
import glob
import json
import os
import re
import subprocess
import sysconfig
import time

from .logger import Logger0, Logger3
from .compiler import Compiler

rx_include = re.compile(r'\s*#\s*include\s*"([^"]+)"')

def normalize_path(path):
    return os.path.relpath(os.path.expanduser(path))



#-------------------------------------------------------------------------------
# Main Extension class
#-------------------------------------------------------------------------------

class Extension:
    """
    Example
    -------
    ::
        import xbuild

        ext = xbuild.Extension()
        ext.name = "_mypkg"
        ext.destination_dir = "lib/"
        ext.add_sources("c/**/*.cc")

        ext.build()
        print("File `%s` built" % ext.output_file)

    """

    def __init__(self):
        # ----------------
        # Input properties
        # ----------------

        # The directory where all temporary files will be stored, such
        # as compiled .o files, and the .xbuild state file.
        self._build_dir = None         # str

        # The compiler object used to compile/link source files.
        self._compiler = None          # compiler.Compiler

        # The directory where the final .so file should be stored.
        self._destination_dir = None   # str

        # A logger object which gets notified about different events
        # during the build process.
        self._log = None               # Logger0

        # Stop compiling after seeing this many lines of errors.
        self._max_error_lines = 100    # int

        # The name of the module being built.
        self._name = None              # str

        # The number of worker processes to use for compiling the
        # source files. Defaults to the number of cores - 1.
        self._nworkers = None          # int

        # List of source files, i.e. files that need to be compiled
        # in order to produce the final executable.
        self._sources = []             # List[str]

        # List of user-provided functions that should be run before
        # the main build phase (but once the build info is already
        # available).
        self._prebuild_triggers = []   # List[callable]


        # ------------------
        # Runtime properties
        # ------------------

        # Dictionary of "modified" status for each source or header
        # file. The keys are the source/header file names. The values
        # are: True if the file was modified since the last build run
        # and False otherwise.
        # All source files will be placed into this dictionary. If
        # a source file has "modified" status, it will be recompiled.
        # All header files that are recursively reachable from any of
        # the source files will eventually be placed into this
        # dictionary too. If a header file is "modified" it will be
        # rescanned to determine its list of includes.
        self._files_modified = {}   # Dict[str, bool]

        # List of source files that need to be compiled. This list is
        # derived from `self._files_modified` at "rescan_files" stage.
        self._files_to_compile = [] # List[str]

        # If True, then force running the link stage, even if no files
        # were re-compiled.
        self._force_link = False

        # Name of the file that was built. This property will be
        # filled after the `build()` command succeeds.
        self._output_file = None    # str

        # Mapping of source file names (from ._sources list) into
        # their corresponding object file names.
        self._src2obj = {}          # Dict[str, str]

        # Mapping of source/header files into their corresponding
        # lists of .h includes. This mapping is persisted in the
        # .xbuild state file.
        self._src_includes = {}     # Dict[str, List[str]]

        # The time of the previous invocation of xbuilder. This is
        # detected via the last modification time of .xbuild file.
        # This time is used to detect which files need to be
        # recompiled?
        self._t0 = 0                # int



    #---------------------------------------------------------------------------
    # Properties
    #---------------------------------------------------------------------------

    @property
    def build_dir(self):
        """
        The directory where xbuild will keep its temporary files
        during the build process. For best results, this directory
        should remain the same during subsequent invocations of
        xbuild.
        """
        if self._build_dir is None:
            self.build_dir = "build"
        return self._build_dir

    @build_dir.setter
    def build_dir(self, value):
        assert isinstance(value, str)
        bdir = normalize_path(value)
        self._build_dir = bdir
        self.log.report_build_dir(bdir)
        if not os.path.exists(bdir):
            os.makedirs(bdir)
            self.log.report_mkdir(bdir)
        if not os.path.isdir(bdir):
            raise IOError("Build directory `%s` cannot be a file" % bdir)


    @property
    def compiler(self):
        """
        The compiler object used to compile/link source files. This
        should be an instance of a class derived from
        :class:`xbuild.compiler.Compiler`.
        """
        if self._compiler is None:
            self.compiler = Compiler()
        return self._compiler

    @compiler.setter
    def compiler(self, cc):
        assert isinstance(cc, Compiler)
        cc._parent = self
        self._compiler = cc
        self.log.report_compiler(cc)


    @property
    def log(self):
        """
        A logger object which gets notified about different events
        during the build process. This should be an instance of a
        class derived from :class:`xbuild.logger.Logger0`.
        """
        if self._log is None:
            self._log = Logger0()
        return self._log

    @log.setter
    def log(self, value):
        assert isinstance(value, Logger0)
        self._log = value


    @property
    def name(self):
        """
        The name of the module, this should contain only alphanumeric
        characters, so that it can work both as a file name and a
        python identifier.
        """
        return self._name

    @name.setter
    def name(self, value):
        assert isinstance(value, str)
        assert re.fullmatch(r"\w+", value)
        self._name = value
        self.log.report_name(value)


    @property
    def destination_dir(self):
        """
        The directory where the final .so file should be stored.
        """
        if self._destination_dir is None:
            self.destination_dir = self.build_dir
        return self._destination_dir

    @destination_dir.setter
    def destination_dir(self, value):
        assert isinstance(value, str)
        ddir = normalize_path(value)
        self._destination_dir = ddir
        self.log.report_destdir(ddir)
        if not os.path.exists(ddir):
            os.makedirs(ddir)
            self.log.report_mkdir(ddir)
        if not os.path.isdir(ddir):
            raise IOError("Destination directory `%s` cannot be a file" % ddir)


    @property
    def sources(self):
        """
        The list of files that the user specified as the "source"
        files, i.e. files that have to be compiled to obtain the
        executable. These should not include the header files.
        """
        return self._sources

    def add_sources(self, *srcs):
        for s in srcs:
            assert s and isinstance(s, str)
            ns = normalize_path(s)
            if re.search(r"[\*\?\[]", s):  # a glob pattern
                srcs = glob.glob(ns, recursive=True)
                self._sources += srcs
                self.log.report_sources(srcs, pattern=s)
            else:
                if not os.path.isfile(ns):
                    raise ValueError("Cannot find source file `%s`" % s)
                self._sources.append(ns)
                self.log.report_sources(ns)

        # deduplicate
        if len(set(self._sources)) != len(self._sources):
            deduplicated = []
            seen = set()
            for src in self._sources:
                if src not in seen:
                    seen.add(src)
                    deduplicated.append(src)
            self._sources = deduplicated
            self.log.report_deduplicated(deduplicated)


    @property
    def nworkers(self):
        """
        The number of worker processes to use for compiling the
        source files. Defaults to the 80% of the number of cores.
        """
        if self._nworkers is None:
            self.nworkers = "auto"
        return self._nworkers

    @nworkers.setter
    def nworkers(self, value):
        if value == "auto":
            ncpus = os.cpu_count()  # could return None
            value = round(ncpus * 0.8) if ncpus else 1
        assert isinstance(value, int)
        assert value > 0
        self._nworkers = value
        self.log.report_nworkers(value)


    @property
    def output_file(self):
        if self._output_file is None:
            self.output_file = "auto"
        return self._output_file

    @output_file.setter
    def output_file(self, value):
        if value == "auto":
            ext_suffix = sysconfig.get_config_var('EXT_SUFFIX')
            assert ext_suffix[0] == "."
            value = os.path.join(self.destination_dir,
                                 self.name + ext_suffix)
        assert isinstance(value, str)
        self._output_file = value
        self.log.report_output_file(value)


    @property
    def state_file(self):
        """
        Path to the file where we keep certain information from the
        previous run of xbuild.
        """
        return os.path.join(self.build_dir, ".xbuild")


    @property
    def xbuild_version(self):
        """
        The iteration version of the xbuild module. Changing this
        variable causes all source files to be rebuilt.
        """
        return "1"


    @property
    def pyabi(self):
        """
        Python `SOABI` config variable, which may look something like
        'cpython-37m-darwin'. If this does not correspond to the value
        stored in .xbuild, then all sources will be rebuilt.
        """
        return sysconfig.get_config_var("SOABI")


    @property
    def max_error_lines(self):
        return self._max_error_lines

    @max_error_lines.setter
    def max_error_lines(self, value):
        assert isinstance(value, int)
        if value <= 0:
            value = 10**9  # Any large number
        self._max_error_lines = value


    def add_prebuild_trigger(self, fn):
        """
        The function should take a single argument -- the extension
        object -- and return None.
        """
        self._prebuild_triggers.append(fn)


    #---------------------------------------------------------------------------
    # State retention
    #---------------------------------------------------------------------------

    def _save_state(self):
        with open(self.state_file, "wt") as out:
            json.dump({
                "version": self.xbuild_version,
                "abi": self.pyabi,
                "includes": self._src_includes,
                "compile": self.compiler.get_compile_command("X", "Y"),
                "link": self.compiler.get_link_command(["X"], "Y"),
            }, out, indent=2)


    def _load_state(self):
        state_file = self.state_file
        self.log.step_load_state(state_file)
        self._t0 = 0
        if os.path.exists(state_file):
            with open(state_file, "rt") as inp:
                info = json.load(inp)
                xver = self.xbuild_version
                ccmd = self.compiler.get_compile_command("X", "Y")
                lcmd = self.compiler.get_link_command(["X"], "Y")
                if info["version"] != xver:
                    self.log.report_version_mismatch(info["version"], xver)
                elif info["abi"] != self.pyabi:
                    self.log.report_abi_mismatch(info["abi"], self.pyabi)
                elif info["compile"] != ccmd:
                    self.log.report_compile_cmd_mismatch(info["compile"], ccmd)
                else:
                    if info["link"] != lcmd:
                        self.log.report_link_cmd_mismatch(info["link"], lcmd)
                        self._force_link = True
                    self._src_includes = info["includes"]
                    self._t0 = int(float(os.path.getmtime(state_file)))
                    self.log.report_includes(info["includes"])
        else:
            self.log.report_no_state_file()

        if self._t0 == 0:
            self.log.report_full_rebuild()
        else:
            self.log.report_t0(self._t0)



    #---------------------------------------------------------------------------
    # Building
    #---------------------------------------------------------------------------

    def build(self):
        """
        Main "build" command: compiles and links a dynamic library
        of the extension.
        """
        if not self.name:
            raise ValueError("An extension doesn't have a name")
        if not self.sources:
            raise ValueError("No source files were specified for compiling "
                             "this extension")
        t_start = time.time()
        self.output_file  # Instantiate the value of this variable
        self.log.cmd_build()
        self._load_state()              # [1]
        self._run_prebuild_triggers()
        self._build_src2obj_map()       # [2]
        self._check_obj_uniqueness()
        self._find_files_to_rebuild()   # [3]
        self._scan_files()              # [4]
        self._compile_files()           # [5]
        self._link_files()              # [6]
        self._save_state()              # [7]
        t_finish = time.time()
        self.log.step_build_done(t_finish - t_start)


    def _build_src2obj_map(self):
        """
        Create dictionary `self._src2obj` which maps every source
        file from `self._sources` into its object file name. For
        example:

            "c/column.cc": "build/column.o",
            "c/frame/repr/html.cc": "build/frame/repr/html.o",
            ...
        """
        self.log.step_src2obj()
        commonpath = os.path.commonpath(self.sources)
        self._src2obj = {}
        for src in self.sources:
            shortpath = os.path.relpath(src, start=commonpath)
            target = os.path.splitext(shortpath)[0].lower() + ".o"
            objfile = os.path.join(self.build_dir, target)
            self._src2obj[src] = objfile


    def _check_obj_uniqueness(self):
        """
        Verify that object file names are unique, i.e. no two source
        files map to the same object file.
        """
        obj2src = {}
        for src in self.sources:
            objfile = self._src2obj[src]
            if objfile in obj2src:
                raise ValueError("Source files %s and %s both produce the "
                                 "same object file %s"
                                 % (obj2src[objfile], src, objfile))
            obj2src[objfile] = src


    def _run_prebuild_triggers(self):
        for fn in self._prebuild_triggers:
            fn(self)


    def is_modified(self, srcfile):
        """
        Memoized check for whether the file `srcfile` is <modified>.

        A file is presumed modified if either (1) it hasn't been seen
        during the previous run of xbuild, or (2) its modification
        timestamp is after the `self._t0` epoch, or (3) one of its
        includes is modified.

        The function stores the <modified> status of the `srcfile` in
        map `self._files_modified`.
        """
        if srcfile not in self._files_modified:
            self._files_modified[srcfile] = (
                srcfile not in self._src_includes or
                not os.path.exists(srcfile) or
                int(os.path.getmtime(srcfile) > self._t0) or
                any(self.is_modified(hfile)
                    for hfile in self._src_includes[srcfile])
            )
        return self._files_modified[srcfile]


    def _find_files_to_rebuild(self):
        """
        Check which of the source files need to be recompiled, either
        because the corresponding obj file is missing, or if the file
        was <modified> (see :meth:`is_modified`).

        This method populates the map `self._files_modified` with
        boolean indicators of whether each file is considered
        <mofified> and should be rescanned. Also it creates the list
        `self._files_to_compile` of source files that need to be
        compiled. And finally, this method deletes all obj files that
        no longer correspond to any source file.
        """
        self.log.step_find_rebuild_targets()
        obj_files_pattern = os.path.join(self._build_dir, "**/*.o")
        existing_obj_files = set(glob.iglob(obj_files_pattern, recursive=True))

        for src_file in self._sources:
            obj_file = self._src2obj[src_file]
            self._files_modified[src_file] = (
                self._t0 == 0 or
                obj_file not in existing_obj_files or
                self.is_modified(src_file)
            )
            existing_obj_files.discard(obj_file)
        self._files_to_compile = [src for src in self._sources
                                  if self._files_modified[src]]
        self.log.report_sources_modified(self._files_to_compile)

        # Remaining `existing_obj_files` do not correspond to any source file,
        # and therefore should be removed
        self.log.report_dead_files(existing_obj_files)
        for obj_file in existing_obj_files:
            os.unlink(obj_file)
            self.log.report_removed_file(obj_file)


    def _scan_files(self):
        """
        Parse all <modified> files and determine their set of
        includes. As each file is scanned, its <modified> flag is
        cleared. At the same time, we recursively scan all includes
        from all modified files, ensuring that full dependency tree
        is known.

        The final effect of this method is that it will populate the
        `self._src_includes` map with up-to-date information about
        the list of includes for each source/header file.
        """
        self.log.step_scan_files(sum(self._files_modified.values()))
        while any(self._files_modified.values()):
            # need list() here because we modify the dictionary in the loop
            for src_file, modified in list(self._files_modified.items()):
                if modified:
                    self._scan_file(src_file)
                    self._files_modified[src_file] = False
                    # Also check the modified status of ALL include files;
                    # the `.is_modified()` call updates the `._files_modified`
                    # dictionary, which is why we have the outer while loop.
                    for hfile in self._src_includes[src_file]:
                        if hfile not in self._files_modified:
                            mod = self.is_modified(hfile)
                            self.log.report_new_header_found(hfile, mod)
        self._files_modified = None


    def _scan_file(self, src_file):
        """
        Scan file `src_file` and detect its list of includes.

        The includes are lines of form `#include "header.h"`, where
        the file `header.h` may be either in the same directory as
        the source file, or relative to one of the "include"
        directories.

        This method will resolve the list of includes to their
        normalized paths, and stores the list into the
        `self._src_includes` map.
        """
        # If a file was removed, then there's nothing to scan
        if not os.path.exists(src_file):
            return
        src_path = os.path.dirname(src_file)
        includes = []
        with open(src_file, "rt", encoding = "utf-8") as inp:
            for line in inp:
                mm = re.match(rx_include, line)
                if mm:
                    hfile = mm.group(1)
                    hfile1 = os.path.join(src_path, hfile)
                    if os.path.exists(hfile1):
                        includes.append(hfile1)
                    else:
                        found = False
                        for incl_dir in self.compiler.include_dirs:
                            hfile2 = os.path.join(incl_dir, hfile)
                            if os.path.exists(hfile2):
                                includes.append(hfile2)
                                found = True
                                break
                        if not found:
                            raise ValueError("Cannot locate header file `%s` "
                                             "included from `%s`"
                                             % (hfile, src_file))
        self._src_includes[src_file] = includes
        self.log.report_src_includes(src_file, includes)


    def _compile_files(self):
        """
        In this step we compile all the files in the
        `self._files_to_compile` list.

        Compilation is done in-parallel, using multiple processes
        (up to `self.nworkers`). All errors/warnings are reported at
        the end. A SystemExit exception is raised if any of the files
        fail to compile. The exception, however, is deferred to a
        point when all files were attempted to be compiled, or at
        least until a sufficient volume of errors was produced (up to
        `self.max_error_lines` lines).

        At the end of this step, if it succeeds, all source files will
        have their corresponding object files physically present on
        disk.
        """
        self.log.step_compile(self._files_to_compile)

        # Sort files by size, from largest to smallest -- this reduces the
        # total compilation time (on my machine from 50s down to 40s).
        sizes = {src: os.path.getsize(src) for src in self._files_to_compile}
        self._files_to_compile.sort(key=lambda s: -sizes[s])

        def compile_queue():
            # Given a queue of workers (`subprocess.Popen` processes),
            # wait until at least one of them finishes and return that
            # worker, and also remove it from the `queue`.
            def await_one(queue):
                while queue:
                    for i, worker in enumerate(queue):
                        if worker.poll() is not None:
                            os.close(worker.fd)
                            del queue[i]
                            return worker
                    time.sleep(0.1)

            queue = []
            for src_file in self._files_to_compile:
                if len(queue) == self.nworkers:
                    yield await_one(queue)
                obj_file = self._src2obj[src_file]
                queue.append(self._compiler.compile(src_file, obj_file))
            while queue:
                yield await_one(queue)

        errors_and_warnings = []
        n_error_lines = 0
        has_errors = False
        for proc in compile_queue():
            self.log.report_compile_finish(proc.source, (proc.returncode != 0))
            if proc.returncode:
                has_errors = True
            with open(proc.output, "rt", encoding="utf-8") as proc_output:
                out = proc_output.read()
            os.remove(proc.output)

            # MSVC prints a translation unit name even when 
            # no warnings/errors are generated. Unfortunately, 
            # there is no way to disable this feature, so here 
            # we just filter out such useless messages. 
            if self._compiler.is_msvc() and out[:-1] in proc.source:
                out = None
            
            if out:
                errors_and_warnings.append(out)
                n_error_lines += len(out.split("\n"))
                if n_error_lines >= self.max_error_lines and has_errors:
                    self.log.report_stopped_compiling()
                    break

        self.log.report_errors_and_warnings(errors_and_warnings, has_errors)
        if has_errors:
            raise SystemExit("Error compiling %s\n" % self.name)


    def _link_files(self):
        """
        Final link stage: take all object files and link them into
        the final dynamic library.
        """
        need_to_link = (self._force_link or
                        self._files_to_compile or
                        not os.path.exists(self.output_file) or
                        int(os.path.getmtime(self.output_file)) > self._t0)
        self.log.step_link(need_to_link)
        if need_to_link:
            obj_files = list(self._src2obj.values())
            proc = self.compiler.link(obj_files, self.output_file)
            proc.wait()
            with open(proc.output, "rt", encoding="utf-8") as proc_output:
                msg = proc_output.read()
            os.close(proc.fd)
            os.remove(proc.output)
            fail = (proc.returncode != 0)
            if msg:
                self.log.report_errors_and_warnings([msg], errors=fail)
            if fail:
                raise SystemExit("Error linking `%s`\n" % self.output_file)
