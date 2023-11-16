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
import pathlib
import subprocess
import sys
import sysconfig
import tempfile


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
        self._lib_dirs = []        # List[str]
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
        self._flavor = "msvc" if "cl.exe" in value else \
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

    def _check_compiler(self, cc: str, source: str, target: str):
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
                self.linker = compiler
                self.log.report_compiler_executable(compiler, env=envvar)
                return

            if sys.platform == "win32" or True:
                self._detect_winsdk()
                msvc_path : str = os.environ.get("DT_MSVC_PATH", "")
                if msvc_path:
                    if not os.path.isdir(msvc_path):
                        raise SystemExit(
                            f"Directory DT_MSVC_PATH=`{msvc_path}` does not exist."
                        )
                else:
                    while True:
                        path0 = pathlib.Path(
                            "C:\\Program Files (x86)\\Microsoft Visual Studio"
                        )
                        if not path0.is_dir():
                            path0 = pathlib.Path(
                                "C:\\Program Files\\Microsoft Visual Studio"
                            )
                        if not path0.is_dir(): break
                        subdirs = [
                            subpath for subpath in path0.iterdir() 
                            if subpath.is_dir() and subpath.name.isdigit()
                        ]
                        if not subdirs: break
                        path0 = sorted(subdirs)[-1]
                        subdirs = [
                            subpath for subpath in path0.iterdir()
                            if (subpath.is_dir() and
                                (subpath/"VC"/"Tools"/"MSVC").is_dir())
                        ]
                        if not subdirs: break
                        # Possible subdirectory names could be: BuildTools, Community,
                        # Enterprise, Professional.
                        msvc_path = str(sorted(subdirs)[-1])
                        break
                    if not os.path.isdir(msvc_path):
                        raise SystemExit(
                            "Cannot find Microsoft Visual Studio installation dir. "
                            "Please specify its location in `DT_MSVC_PATH` environment "
                            "variable."
                        )
                
                def print_path(path: pathlib.Path, indent: str):
                    for p in path.iterdir():
                        if p.is_dir():
                            self.log.info(p.name + '/', indent=indent)
                            print_path(p, indent + "  ")
                        elif p.suffix.endswith('exe'):
                            self.log.info(p.name, indent=indent)

                self.log.info(f"MSVC_path = {msvc_path}:")
                print_path(pathlib.Path(msvc_path), indent = "  ")

                candidates = []
                compiler_versions = next(os.walk(msvc_path))[1]
                for compiler_version in reversed(compiler_versions):
                    path =  os.path.join(msvc_path, compiler_version)
                    bin_path = os.path.join(path, "bin\\Hostx64\\x64")
                    if os.path.exists(os.path.join(bin_path, "cl.exe")):
                        candidates += [{
                            "compiler": os.path.join(bin_path, "cl.exe"),
                            "linker": os.path.join(bin_path, "link.exe"),
                            "path" : path
                        }]
                candidates += [{"compiler": "cl.exe", "linker": "link.exe"}]
            elif sys.platform == "darwin":
                candidates = [
                    {"compiler": "/usr/local/opt/llvm/bin/clang"},
                    {"compiler": "clang"}
                ]
            else:
                candidates = [
                    {"compiler": "gcc"},
                    {"compiler": "/usr/local/opt/llvm/bin/clang"},
                    {"compiler": "clang"},
                    {"compiler": "cc"},
                ]

            for candidate in candidates:
                if self._check_compiler(candidate["compiler"], srcname, outname):
                    self.executable = candidate["compiler"]
                    self.linker = candidate.get("linker", candidate["compiler"])
                    self.path = candidate.get("path", "")
                    self.log.report_compiler_executable(candidate["compiler"])
                    return

            self.log.info(f"Candidates: {candidates!r}")
            raise RuntimeError("Suitable C++ compiler cannot be determined. "
                               "Please specify a compiler executable in the "
                               "`CXX` environment variable.")
        finally:
            if srcname and os.path.isfile(srcname):
                os.remove(srcname)
            if outname and os.path.isfile(outname):
                os.remove(outname)


    def _detect_winsdk(self):
        def is_winsdk_version(version):
            version_ids = version.split(".")
            if len(version_ids) != 4:
                return False
            return all(version_id.isdigit() for version_id in version_ids)

        winsdk_default_path = "C:\\Program Files (x86)\\Windows Kits\\10\\"
        winsdk_path = os.environ.get("DT_WINSDK_PATH", winsdk_default_path)
        if not os.path.isdir(winsdk_path):
            raise ValueError("Windows SDK directory %s not found. "
                             "Please specify its location in `DT_WINSDK_PATH` environment variable."
                             % msvc_path)

        # Detect the latest available SDK version
        winsdk_version_dir = ""
        winsdk_versions = next(os.walk(winsdk_default_path + "\\include"))[1]
        for version in reversed(winsdk_versions):
            if is_winsdk_version(version):
                winsdk_version_dir = version
                break

        if winsdk_version_dir == "":
            raise ValueError("A valid Windows SDK version directory %s not found.")

        winsdk_include_path = winsdk_default_path + "\\Include\\" + winsdk_version_dir
        winsdk_lib_path = winsdk_default_path + "\\Lib\\" + winsdk_version_dir
        if not os.path.isdir(winsdk_include_path):
            raise ValueError("Windows SDK include directory %s not found" % winsdk_include_path)
        if not os.path.isdir(winsdk_lib_path):
            raise ValueError("Windows SDK lib directory %s not found" % winsdk_lib_path)

        self.winsdk_include_path = winsdk_include_path
        self.winsdk_lib_path = winsdk_lib_path


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
        py_include_dir = sysconfig.get_config_var("INCLUDEPY")
        if not os.path.isdir(py_include_dir):
            self.log.warn("Python include directory `%s` does not exist, "
                          "compilation may fail" % py_include_dir)
        elif not os.path.exists(os.path.join(py_include_dir, "Python.h")):
            self.log.warn("Python include directory `%s` is missing the file "
                          "Python.h, compilation may fail" % py_include_dir)
        self.add_include_dir(py_include_dir, system=True)


    def add_compiler_flag(self, *flags):
        for flag in flags:
            if flag:
                assert isinstance(flag, str)
                self._compiler_flags.append(flag)


    def enable_colors(self):
        if not self.is_msvc():
            self._compiler_flags.append("-fdiagnostics-color=always")


    def get_compile_command(self, source, target):
        cmd = [self.executable] + self._compiler_flags
        if self.is_msvc():
            cmd += ["/c", source, "/Fo" + target]
        else:
            cmd += ["-c", source, "-o", target]
        return cmd


    def compile(self, src, obj, silent=False):
        os.makedirs(os.path.dirname(obj), exist_ok=True)
        cmd = self.get_compile_command(src, obj)
        if not silent:
            self.log.report_compile_start(src, cmd)

        fd, srcname = tempfile.mkstemp(suffix=".out")
        proc = subprocess.Popen(cmd, stdout=fd, stderr=fd)
        proc.fd = fd
        proc.source = src
        proc.output = srcname
        return proc



    #---------------------------------------------------------------------------
    # Linking
    #---------------------------------------------------------------------------

    def _flags_for_lib_dir(self, path):
        if self.is_msvc():
            return ["/LIBPATH:" + path]
        else:
            return ["-L" + path]


    def add_lib_dir(self, path):
        if not path:
            return
        assert isinstance(path, str)
        if not os.path.isdir(path):
            raise ValueError("Lib directory %s not found" % path)
        self._lib_dirs.append(path)
        self._linker_flags += self._flags_for_lib_dir(path)
        self.log.report_lib_dir(path)


    def add_default_python_lib_dir(self):
        py_dir = sysconfig.get_config_var("BINDIR")
        py_lib_dir = os.path.join(py_dir, "libs")
        if not os.path.isdir(py_lib_dir):
            self.log.warn("Python lib directory `%s` does not exist, "
                          "linking may fail" % py_lib_dir)
        self.add_lib_dir(py_lib_dir)


    def add_linker_flag(self, *flags):
        for flag in flags:
            if flag:
                assert isinstance(flag, str)
                self._linker_flags.append(flag)


    def get_link_command(self, sources, target):
        cmd = [self.linker]
        cmd += sources
        if self.is_msvc():
            cmd += ["/OUT:" + target]
        else:
            cmd += ["-o", target]
        # Certain linker flags (such as included libraries) must come AFTER the
        # list of object files. Should we give the user a better control over
        # the location of different flags?
        cmd += self._linker_flags
        return cmd


    def link(self, obj_files, target):
        os.makedirs(os.path.dirname(target), exist_ok=True)
        cmd = self.get_link_command(obj_files, target)
        self.log.report_link_file(target, cmd)

        fd, srcname = tempfile.mkstemp(suffix=".out")
        proc = subprocess.Popen(cmd, stdout=fd, stderr=fd)
        proc.fd = fd
        proc.output = srcname
        return proc
