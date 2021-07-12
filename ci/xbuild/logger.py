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
import re


#-------------------------------------------------------------------------------
# Logger0
#-------------------------------------------------------------------------------

class Logger0:
    """
    The "silent" logger: it does not report any messages.
    """
    def cmd_audit(self): pass
    def cmd_build(self): pass
    def cmd_sdist(self): pass
    def cmd_wheel(self): pass

    def report_abi_mismatch(self, v1, v2): pass
    def report_abi_variable_missing(self, v): pass
    def report_added_file_to_sdist(self, filename, size): pass
    def report_added_file_to_wheel(self, filename, size): pass
    def report_build_dir(self, dd): pass
    def report_compatibility_tag(self, tag): pass
    def report_compile_cmd_mismatch(self, cmd1, cmd2): pass
    def report_compile_start(self, filename, cmd): pass
    def report_compile_finish(self, filename, has_errors): pass
    def report_compiler(self, cc): pass
    def report_compiler_executable(self, cc, env=None): pass
    def report_dead_files(self, files): pass
    def report_deduplicated(self, files): pass
    def report_destdir(self, dd): pass
    def report_full_rebuild(self): pass
    def report_generating_docs(self, docfile): pass
    def report_include_dir(self, dd): pass
    def report_includes(self, inlcudes): pass
    def report_lib_dir(self, dd): pass
    def report_link_cmd_mismatch(self, cmd1, cmd2): pass
    def report_link_file(self, filename, cmd): pass
    def report_mkdir(self, dd): pass
    def report_name(self, name): pass
    def report_new_header_found(self, filename, modified): pass
    def report_no_state_file(self): pass
    def report_nworkers(self, nworkers): pass
    def report_output_file(self, filename): pass
    def report_removed_file(self, filename): pass
    def report_sdist_file(self, filename): pass
    def report_sources(self, sources, pattern=None): pass
    def report_sources_modified(self, files): pass
    def report_stopped_compiling(self): pass
    def report_src_includes(self, filename, includes): pass
    def report_t0(self, t0): pass
    def report_version_mismatch(self, v1, v2): pass
    def report_wheel_file(self, filename): pass

    def step_audit_done(self, time, newname): pass
    def step_build_done(self, time): pass
    def step_sdist_done(self, time, size): pass
    def step_wheel_done(self, time, size): pass
    def step_compile(self, files): pass
    def step_find_rebuild_targets(self): pass
    def step_link(self, dolink): pass
    def step_load_state(self, filename): pass
    def step_scan_files(self, n): pass
    def step_src2obj(self): pass

    def warn(self, msg, indent=None): pass
    def info(self, msg, indent=None): pass

    def report_errors_and_warnings(self, msgs, errors=False):
        if msgs:
            print()
            for msg in msgs:
                print(msg)




#-------------------------------------------------------------------------------
# Logger1
#-------------------------------------------------------------------------------

class Logger1(Logger0):
    """
    Minimal logger, uses a progress bar
    """

    def __init__(self):
        self._msg = ""
        self._n_started = 0
        self._n_finished = 0
        self._n_total = 0
        self._output_file = None
        self._progress_bar_size = 60

    def step_compile(self, files):
        self._n_total = len(files)
        self._progress_bar_size = min(60, self._n_total)
        self._msg = "Compiling"
        if self._n_total:
            self._redraw()

    def step_link(self, dolink):
        self._msg = "Linking..."
        if dolink:
            self._n_total = 1
            self._redraw()

    def step_build_done(self, time):
        if self._n_total:
            self._msg = "File %s rebuilt in %.3fs" % (self._output_file, time)
            self._redraw()
            print()

    def report_compile_start(self, filename, cmd):
        self._n_started += 1
        self._redraw()

    def report_compile_finish(self, filename, has_errors):
        self._n_finished += 1
        self._redraw()

    def report_output_file(self, filename):
        self._output_file = filename

    def report_generating_docs(self, docfile):
        msg = "Generating documentation %s" % docfile
        print("\r\x1B[90m" + msg + "\x1B[m\x1B[K")
        self._redraw()

    def _redraw(self):
        line = "\r\x1B[90m" + self._msg
        if self._msg == "Compiling":
            sz = self._progress_bar_size
            ch_done = round(sz * self._n_finished / self._n_total)
            ch_queued = round(sz * self._n_started / self._n_total) - ch_done
            ch_future = sz - ch_queued - ch_done
            line += " [%s\x1B[1;97m%s\x1B[0;90m%s]" \
                    % ("#" * ch_done, "#" * ch_queued, "." * ch_future)
            line += " %d/%d" % (self._n_finished, self._n_total)
        line += "\x1B[m\x1B[K"  # clear the rest of the line
        print(line, end="", flush=True)


#-------------------------------------------------------------------------------
# Logger2
#-------------------------------------------------------------------------------

class Logger2(Logger0):
    """
    Sublime friendly conservative logger.
    """

    def __init__(self):
        self._indent = ""

    def report_compile_start(self, filename, cmd):
        self.info("Compiling `%s`" % filename)

    def report_compiler_executable(self, cc, env=None):
        msg = "Using compiler `%s`" % cc
        if env:
            msg += " from environment variable `%s`" % env
        self.info(msg)

    def report_link_file(self, filename, cmd):
        self.info("Linking `%s`" % filename)

    def report_output_file(self, filename):
        self.info("Output file name will be `%r`" % filename)

    def report_generating_docs(self, docfile):
        self.info("Generating documentation %s" % docfile)

    def step_compile(self, files):
        if files:
            self.info("Compiling `%d` source files" % len(files),
                      indent="")
        else:
            self.info("Compiling source files: SKIPPED", indent="")



    def info(self, msg, indent=None):
        if indent is None:
            indent = self._indent
        msg = re.sub(r"`([^`]*)`", "\x1B[1;97m\\1\x1B[0;90m", msg)
        print(indent + "\x1B[90m" + msg + "\x1B[m")

    def warn(self, msg, indent=None):
        if indent is None:
            indent = self._indent
        msg = re.sub(r"`([^`]*)`", "\x1B[1;97m\\1\x1B[0;91m", msg)
        print(indent + "\x1B[91m" + msg + "\x1B[m")

    def report_errors_and_warnings(self, msgs, errors=False):
        if not msgs:
            return
        color = "\x1B[91m" if errors else \
                "\x1B[93m"
        print(color + "+==== ERRORS & WARNINGS ========\n\x1B[m")
        for msg in msgs:
            for line in msg.split("\n"):
                print(color + "\x1B[m" + line)



#-------------------------------------------------------------------------------
# Logger3
#-------------------------------------------------------------------------------

class Logger3(Logger0):
    """
    Logger with maximum verbosity level.
    """

    def __init__(self):
        self._indent = ""

    def cmd_audit(self):
        self.info("==== AUDIT ====")

    def cmd_build(self):
        self.info("==== BUILD ====")

    def cmd_sdist(self):
        self.info("==== SDIST ====")

    def cmd_wheel(self):
        self.info("==== WHEEL ====")


    def report_abi_mismatch(self, v1, v2):
        self.info("Config file contains abi=`%s`, whereas current abi is `%s`"
                  % (v1, v2))

    def report_abi_variable_missing(self, v):
        self.info("Config variable `%s` is missing, ABI tag may be incorrect"
                  % v)

    def report_added_file_to_sdist(self, filename, size):
        self.info("Added file `%s` of size %d" % (filename, size))

    def report_added_file_to_wheel(self, filename, size):
        self.info("Added file `%s` of size %d" % (filename, size))

    def report_build_dir(self, dd):
        self.info("Build directory = %r" % dd)

    def report_compatibility_tag(self, tag):
        self.info("Wheel file compatible with tag `%s`" % tag)

    def report_compile_cmd_mismatch(self, cmd1, cmd2):
        self.info("Compiler flags changed")

    def report_compile_start(self, filename, cmd):
        self.info("Compiling `%s`: %s" % (filename, " ".join(cmd)))

    def report_compiler(self, cc):
        self.info("Using compiler of class %s.%s"
                  % (cc.__class__.__module__, cc.__class__.__qualname__))

    def report_compiler_executable(self, cc, env=None):
        msg = "Using compiler `%s`" % cc
        if env:
            msg += " from environment variable `%s`" % env
        self.info(msg)

    def report_dead_files(self, files):
        if files:
            self.info("Found `%d dead files` which will be removed"
                      % len(files))
        else:
            self.info("No dead files found")

    def report_deduplicated(self, files):
        self.info("After deduplication `%d` souce files remain" % len(files))

    def report_destdir(self, dd):
        self.info("Destination directory = %r" % dd)

    def report_full_rebuild(self):
        self.info("All source files will be recompiled")

    def report_generating_docs(self, docfile):
        self.info("Generating documentation %s" % docfile)

    def report_include_dir(self, dd):
        self.info("Added include directory %r" % dd)

    def report_includes(self, includes):
        self.info("Loaded information about %d source and header files"
                  % len(includes))

    def report_lib_dir(self, dd):
        self.info("Added library directory %r" % dd)

    def report_link_cmd_mismatch(self, cmd1, cmd2):
        self.info("Linker flags changed")

    def report_link_file(self, filename, cmd):
        self.info("Linking `%s`: %s" % (filename, ' '.join(cmd)))

    def report_mkdir(self, dd):
        self.info("Created directory %r" % dd)

    def report_name(self, name):
        self.info("Building extension `%s`" % name)

    # def report_new_header_found(self, filename, modified):
    #     self.info("New header file %s, %s"
    #               % (filename, "added to queue" if modified else "unmodified"))

    def report_no_state_file(self):
        self.info("The file does not exist")

    def report_nworkers(self, nworkers):
        self.info("Use %d worker processes" % nworkers)

    def report_output_file(self, filename):
        self.info("Output file name will be `%r`" % filename)

    def report_removed_file(self, filename):
        self.info("File `%s` deleted" % filename)

    def report_sdist_file(self, filename):
        self.info("Preparing to build sdist file `%s`" % filename)
        self._indent = "    "

    def report_sources(self, sources, pattern=None):
        if pattern is None:
            self.info("Added source file %s" % sources)
        else:
            assert isinstance(sources, list)
            self.info("Added %d source files from pattern `%s`: %s"
                      % (len(sources), pattern, sources))

    def report_sources_modified(self, files):
        if files:
            self.info("Found `%d modified source files` which will be rebuilt"
                      % len(files))
        else:
            self.info("No modified source files found")

    # def report_src_includes(self, filename, includes):
    #     self.info("Includes for %s: %r" % (filename, includes))

    def report_stopped_compiling(self):
        self.info("Stopped compiling because too many errors produced")

    def report_t0(self, t0):
        self.info("Timestamp of previous successful build: %d" % t0)

    def report_version_mismatch(self, v1, v2):
        self.info("Config files contains version=%s, whereas the current "
                  "version is %s" % (v1, v2))

    def report_wheel_file(self, filename):
        self.info("Preparing to build wheel file `%s`" % filename)
        self._indent = "    "



    def step_audit_done(self, time, newname):
        self.info("Wheel file renamed into `%s`" % newname)
        self.info("==== Audit finished in %.3fs" % time)

    def step_build_done(self, time):
        self._indent = ""
        self.info("==== Build finished in %.3fs" % time)

    def step_sdist_done(self, time, size):
        self._indent = ""
        self.info("Final sdist size: %d bytes" % size)
        self.info("==== Sdist created in %.3fs" % time)

    def step_wheel_done(self, time, size):
        self._indent = ""
        self.info("Final wheel size: %d bytes" % size)
        self.info("==== Wheel built in %.3fs" % time)


    def step_compile(self, files):
        if files:
            self.info("[5] Compiling `%d` source files" % len(files),
                      indent="")
        else:
            self.info("[5] Compiling source files: SKIPPED", indent="")

    def step_find_rebuild_targets(self):
        self.info("[3] Determining which source files to compile",
                  indent="")

    def step_link(self, dolink):
        self.info("[6] Linking the final dynamic library%s"
                  % ("" if dolink else ": SKIPPED"),
                  indent="")

    def step_load_state(self, filename):
        self.info("[1] Loading previous state from file `%s`" % filename,
                  indent="")
        self._indent = "    "

    def step_scan_files(self, n):
        if n:
            self.info("[4] Scanning %d modified files to detect headers" % n,
                      indent="")
        else:
            self.info("[4] Scanning modified files: SKIPPED", indent="")

    def step_src2obj(self):
        self.info("[2] Determining the names of obj files to build",
                  indent="")



    def info(self, msg, indent=None):
        if indent is None:
            indent = self._indent
        msg = re.sub(r"`([^`]*)`", "\x1B[1;97m\\1\x1B[0;90m", msg)
        print(indent + "\x1B[90m" + msg + "\x1B[m")


    def warn(self, msg, indent=None):
        if indent is None:
            indent = self._indent
        msg = re.sub(r"`([^`]*)`", "\x1B[1;97m\\1\x1B[0;91m", msg)
        print(indent + "\x1B[91m" + msg + "\x1B[m")



    def report_errors_and_warnings(self, msgs, errors=False):
        if not msgs:
            return
        color = "\x1B[91m" if errors else \
                "\x1B[93m"
        print(color + "+==== ERRORS & WARNINGS ========\n|\x1B[m")
        for msg in msgs:
            for line in msg.split("\n"):
                print(color + "|\x1B[m " + line)
        print(color + "+===============================\x1B[m")
