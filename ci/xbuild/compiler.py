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
import sysconfig
from .logger import Logger0


class Compiler:

    def __init__(self):
        self._executable = "gcc"
        self._parent = None
        self._compiler_flags = []
        self._include_dirs = []
        self._linker_flags = []


    @property
    def log(self):
        if self._parent is None:
            raise RuntimeError("Compiler object must be attached to an "
                               "Extension")
        return self._parent.log


    @property
    def include_dirs(self):
        return self._include_dirs



    #---------------------------------------------------------------------------
    # Compiling
    #---------------------------------------------------------------------------

    def add_include_dir(self, path):
        if not path:
            return
        assert isinstance(path, str)
        if not os.path.isdir(path):
            raise ValueError("Include directory %s not found" % path)
        self._include_dirs.append(path)
        self._compiler_flags += ["-I" + path]
        self.log.report_include_dir(path)


    def add_default_python_include_dir(self):
        dd = sysconfig.get_config_var("CONFINCLUDEPY")
        if not os.path.isdir(dd):
            self._log.warn("Python include directory `%s` does not exist, "
                           "compilation may fail" % dd)
        elif not os.path.exists(os.path.join(dd, "Python.h")):
            self._log.warn("Python include directory `%s` is missing the file "
                           "Python.h, compilation may fail" % dd)
        self.add_include_dir(dd)


    def add_compiler_flag(self, *flags):
        for flag in flags:
            if flag:
                assert isinstance(flag, str)
                self._compiler_flags.append(flag)


    def enable_colors(self):
        self._compiler_flags.append("-fdiagnostics-color=always")


    def get_compile_command(self, source, target):
        cmd = [self._executable] + self._compiler_flags
        cmd += ["-c", source]
        cmd += ["-o", target]
        return cmd


    def compile(self, src, obj):
        os.makedirs(os.path.dirname(obj), exist_ok=True)
        cmd = self.get_compile_command(src, obj)
        self.log.report_compile_file(src, cmd)

        return subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)



    #---------------------------------------------------------------------------
    # Linking
    #---------------------------------------------------------------------------

    def add_linker_flag(self, *flags):
        for flag in flags:
            if flag:
                assert isinstance(flag, str)
                self._linker_flags.append(flag)


    def get_link_command(self, sources, target):
        cmd = [self._executable] + self._linker_flags
        cmd += sources
        cmd += ["-o", target]
        return cmd


    def link(self, obj_files, target):
        os.makedirs(os.path.dirname(target), exist_ok=True)
        cmd = self.get_link_command(obj_files, target)
        self.log.report_link_file(target, cmd)

        return subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
