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
import os
import subprocess
import sys
import sysconfig
import tempfile
from .logger import Logger0


class Compiler:

    def __init__(self):
        # Name of the compiler executable
        self._executable = None    # str

        # The "flavor" of the executable, determines how the flags are
        # passed.
        self._flavor = None        # 'msvc'|'gcc'|'clang'|'unknown'

        # Parent `Extension` class
        self._parent = None        # xbuild.extension.Extension
        self._compiler_flags = []  # List[str]
        self._include_dirs = []    # List[str]
        self._linker_flags = []    # List[str]


    @property
    def log(self):
        if self._parent is None:
            raise RuntimeError("Compiler object must be attached to an "
                               "Extension")
        return self._parent.log


    @property
    def executable(self):
        if self._executable is None:
            self._detect_compiler_executable()
        return self._executable

    @executable.setter
    def executable(self, value):
        assert isinstance(value, str)
        self._executable = value
        self._flavor = "msvc" if "msvc" in value else \
                       "clang" if "clang" in value else \
                       "gcc" if "gcc" in value or "g++" in value else \
                       "unknown"


    @property
    def flavor(self):
        if self._flavor is None:
            self._detect_compiler_executable()
        return self._flavor


    @property
    def include_dirs(self):
        return self._include_dirs


    def is_clang(self):
        return self.flavor == "clang"

    def is_msvc(self):
        return self.flavor == "msvc"

    def is_gcc(self):
        return self.flavor == "gcc"



    #---------------------------------------------------------------------------
    # Setup
    #---------------------------------------------------------------------------

    def _check_compiler(self, cc, source, target):
        """
        Check whether the given compiler is viable, i.e. whether it
        can compile a simple example file. Returns True if the
        compiler works, and False otherwise.
        """
        e = self._executable
        f = self._flavor
        try:
            self.executable = cc
            proc = self.compile(source, target, silent=True)
            ret = proc.wait()
            return (ret == 0)
        except:
            return False
        finally:
            self._executable = e
            self._flavor = f


    def _detect_compiler_executable(self):
        fd, srcname = tempfile.mkstemp(suffix=".cc")
        outname = srcname + ".out"
        os.close(fd)
        assert os.path.isfile(srcname)
        try:
            for envvar in ["CC", "CXX"]:
                compiler = os.environ.get(envvar)
                if not compiler:
                    continue
                if os.path.isabs(compiler) and not os.path.exists(compiler):
                    raise ValueError("The compiler `%s` from environment "
                                     "variable `%s` does not exist"
                                     % (compiler, envvar))
                if not self._check_compiler(compiler, srcname, outname):
                    raise  ValueError("The compiler `%s` from environment "
                                     "variable `%s` failed to compile an "
                                     "empty file" % (compiler, envvar))
                self.executable = compiler
                self.log.report_compiler_executable(compiler, env=envvar)
                return

            if sys.platform == "win32":
                candidates = ["msvc.exe", "clang.exe", "gcc.exe"]
            elif sys.platform == "darwin":
                candidates = ["/usr/local/opt/llvm/bin/clang", "clang"]
            else:
                candidates = ["gcc", "/usr/local/opt/llvm/bin/clang",
                              "clang", "cc"]

            for cc in candidates:
                if self._check_compiler(cc, srcname, outname):
                    self.executable = cc
                    self.log.report_compiler_executable(cc)
                    return

            raise RuntimeError("Suitable C++ compiler cannot be determined. "
                               "Please specify a compiler executable in the "
                               "`CXX` environment variable.")
        finally:
            if srcname and os.path.isfile(srcname):
                os.remove(srcname)
            if outname and os.path.isfile(outname):
                os.remove(outname)



    #---------------------------------------------------------------------------
    # Compiling
    #---------------------------------------------------------------------------

    def _flags_for_include_dir(self, path, as_system):
        if self.is_msvc():
            return ["/I" + path]
        elif as_system:
            return ["-isystem", path]
        else:
            return ["-I" + path]


    def add_include_dir(self, path, system=False):
        if not path:
            return
        assert isinstance(path, str)
        if not os.path.isdir(path):
            raise ValueError("Include directory %s not found" % path)
        self._include_dirs.append(path)
        self._compiler_flags += self._flags_for_include_dir(path, system)
        self.log.report_include_dir(path)


    def add_default_python_include_dir(self):
        dd = sysconfig.get_config_var("CONFINCLUDEPY")
        if not os.path.isdir(dd):
            self._log.warn("Python include directory `%s` does not exist, "
                           "compilation may fail" % dd)
        elif not os.path.exists(os.path.join(dd, "Python.h")):
            self._log.warn("Python include directory `%s` is missing the file "
                           "Python.h, compilation may fail" % dd)
        self.add_include_dir(dd, system=True)


    def add_compiler_flag(self, *flags):
        for flag in flags:
            if flag:
                assert isinstance(flag, str)
                self._compiler_flags.append(flag)


    def enable_colors(self):
        self._compiler_flags.append("-fdiagnostics-color=always")


    def get_compile_command(self, source, target):
        cmd = [self.executable] + self._compiler_flags
        if self.is_msvc():
            cmd += ["/c" + source, "/Fo" + target]
        else:
            cmd += ["-c", source, "-o", target]
        return cmd


    def compile(self, src, obj, silent=False):
        os.makedirs(os.path.dirname(obj), exist_ok=True)
        cmd = self.get_compile_command(src, obj)
        if not silent:
            self.log.report_compile_start(src, cmd)

        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
        proc.source = src
        return proc



    #---------------------------------------------------------------------------
    # Linking
    #---------------------------------------------------------------------------

    def add_linker_flag(self, *flags):
        for flag in flags:
            if flag:
                assert isinstance(flag, str)
                self._linker_flags.append(flag)


    def get_link_command(self, sources, target):
        cmd = [self.executable] + self._linker_flags
        cmd += sources
        if self.is_msvc():
            cmd += ["/Fe:", target]
        else:
            cmd += ["-o", target]
        return cmd


    def link(self, obj_files, target):
        os.makedirs(os.path.dirname(target), exist_ok=True)
        cmd = self.get_link_command(obj_files, target)
        self.log.report_link_file(target, cmd)

        return subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
