#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
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
import re
import shutil
import subprocess
import sys
import sysconfig
import tempfile
from functools import lru_cache as memoize
from distutils.errors import DistutilsExecError, CompileError

__all__ = (
    "find_linked_dynamic_libraries",
    "get_compiler",
    "get_datatable_version",
    "get_default_compile_flags",
    "get_extra_compile_flags",
    "get_extra_link_args",
    "get_llvm",
    "get_rpath",
    "islinux",
    "ismacos",
    "iswindows",
    "make_git_version_file",
    "monkey_patch_compiler",
    "required_link_libraries",
    "TaskContext",
)

verbose = True
colored = sys.stdout.isatty()


#-------------------------------------------------------------------------------
# Output helpers
#-------------------------------------------------------------------------------

class TaskContext:
    """
    Use this clas for pretty-printing every step of the build process:

        with TaskContext("Important step") as log:
            log.info("step1")
            log.warn("something not right")
            log.fatal("cannot proceed...")
    """

    def __init__(self, head):
        if colored:
            self._msgs = "\x1B[38;5;111m" + head + "\x1B[0m\n"
        else:
            self._msgs = head + "\n"

    def __enter__(self):
        return self

    def __exit__(self, ttype, value, traceback):
        self.emit()

    def info(self, msg):
        if colored:
            self._msgs += "\x1B[38;5;240m  " + msg + "\x1B[0m\n"
        else:
            self._msgs += "  %s\n" % msg

    def warn(self, msg):
        if colored:
            self._msgs += "\x1B[38;5;220m  Warning: " + msg + "\x1B[0m\n"
        else:
            self._msgs += "  Warning: " + msg + "\n"

    def emit(self):
        if verbose:
            print(self._msgs)
        self._msgs = ""

    def fatal(self, msg):
        # The SystemExit exception causes the error message to print without
        # any traceback. Also add some red color for dramatic effect.
        raise SystemExit("\x1B[91m\nSystemExit: %s\n\x1B[39m" % msg)



#-------------------------------------------------------------------------------
# Build process helpers
#-------------------------------------------------------------------------------

def dtroot():
    return os.path.abspath(os.path.join(os.path.basename(__file__), ".."))

def dtpath(*args):
    return os.path.join(dtroot(), *args)

def islinux():
    return sys.platform == "linux"

def ismacos():
    return sys.platform == "darwin"

def iswindows():
    return sys.platform == "win32"




def get_datatable_version():
    # According to PEP-440, the canonical version must have the following
    # general form:
    #
    #     [N!]N(.N)*[{a|b|rc}N][.postN][.devN]
    #
    # In the version file we allow only `N(.N)*[{a|b|rc}N]`, with "dev" suffix
    # optionally added afterwards from CI_VERSION_SUFFIX environment variable.
    #
    with TaskContext("Determine datatable version") as log:
        version = None
        filename = dtpath("datatable", "__version__.py")
        if not os.path.isfile(filename):
            log.fatal("The version file %s does not exist" % filename)
        log.info("Reading file " + filename)
        with open(filename, encoding="utf-8") as f:
            rx = re.compile(r"version\s*=\s*['\"]"
                            r"(\d+(?:\.\d+)+(?:(?:a|b|rc)\d*)?)"
                            r"['\"]\s*")
            for line in f:
                mm = re.match(rx, line)
                if mm is not None:
                    version = mm.group(1)
                    log.info("Detected version: " + version)
                    break
        if version is None:
            log.fatal("Could not detect project version from the "
                      "__version__.py file")
        # Append build suffix if necessary
        suffix = os.environ.get("CI_VERSION_SUFFIX")
        if suffix:
            log.info("Appending suffix from CI_VERSION_SUFFIX = " + suffix)
            mm = re.match(r"(?:master|dev)[.+_-]?(\d+)", suffix)
            if mm:
                suffix = "dev" + str(mm.group(1))
            version += "." + suffix
            log.info("Final version = " + version)
        else:
            log.info("Environment variable CI_VERSION_SUUFFIX not present")
        return version



def make_git_version_file(force):
    import subprocess
    with TaskContext("Generate git version file" + " (force)" * force) as log:
        filename = os.path.join(dtroot(), "datatable", "__git__.py")
        # Try to read git revision from env.
        githash = os.getenv('DTBL_GIT_HASH', None)
        if githash:
            log.info("Environment variable DTBL_GIT_HASH = " + githash)
        else:
            log.info("Environment variable DTBL_GIT_HASH not present")
            # Need to get git revision from git cmd. Fail if .git dir is not
            # accessible.
            gitdir = os.path.join(dtroot(), ".git")
            if not os.path.isdir(gitdir):
                # Check whether the file below is present. If not, this must
                # be a source distribution, and it will not be possible to
                # rebuild the __git__.py file.
                testfile = os.path.join(dtroot(), "ci", "Jenkinsfile.groovy")
                if not os.path.isfile(testfile) and os.path.isfile(filename):
                    log.info("Source distribution detected, file __git__.py "
                             "cannot be rebuilt")
                    return
                if force:
                    log.fatal("Cannot determine git revision of the package "
                              "because folder `%s` is missing and environment "
                              "variable DTBL_GIT_HASH is not set."
                              % gitdir)
                log.info("Directory .git not found")
                log.warn("Could not generate __git__.py")
                return
            # Read git revision using cmd.
            out = subprocess.check_output(["git", "rev-parse", "HEAD"])
            githash = out.decode("ascii").strip()
            log.info("`git rev-parse HEAD` = " + githash)
        log.info("Generating file " + filename)
        with open(filename, "w", encoding="utf-8") as o:
            o.write(
                "#!/usr/bin/env python3\n"
                "# © H2O.ai 2018; -*- encoding: utf-8 -*-\n"
                "#   This Source Code Form is subject to the terms of the\n"
                "#   Mozilla Public License, v2.0. If a copy of the MPL was\n"
                "#   not distributed with this file, You can obtain one at\n"
                "#   http://mozilla.org/MPL/2.0/.\n"
                "# ----------------------------------------------------------\n"
                "# This file was auto-generated from ci/setup_utils.py\n\n"
                "__git_revision__ = \'%s\'\n"
                % githash)



g_llvmdir = ...
g_llvmver = ...

def get_llvm(with_version=False):
    global g_llvmdir, g_llvmver
    if g_llvmdir is Ellipsis:
        with TaskContext("Find an LLVM installation") as log:
            g_llvmdir = None
            g_llvmver = None
            for LLVMX in ["LLVM", "LLVM7", "LLVM6", "LLVM5", "LLVM4"]:
                g_llvmdir = os.environ.get(LLVMX)
                if g_llvmdir:
                    log.info("Environment variable %s = %s"
                             % (LLVMX, g_llvmdir))
                    if not os.path.isdir(g_llvmdir):
                        log.fatal("Environment variable %s = %r is not a "
                                  "valid directory" % (LLVMX, g_llvmdir))
                    g_llvmver = LLVMX
                    break
                else:
                    log.info("Environment variable %s is not set" % LLVMX)
            if not g_llvmdir:
                candidate_dirs = ["/usr/local/opt/llvm"]
                for cdir in candidate_dirs:
                    if os.path.isdir(cdir):
                        log.info("Directory `%s` found" % cdir)
                        g_llvmdir = cdir
                        break
                    else:
                        log.info("Candidate directory `%s` not found" % cdir)
            if g_llvmdir:
                if not g_llvmver or g_llvmver == "LLVM":
                    try:
                        llc = os.path.join(g_llvmdir, "bin/llvm-config")
                        if os.path.exists(llc):
                            out = subprocess.check_output([llc, "--version"])
                            version = out.decode().strip()
                            g_llvmver = "LLVM" + version.split('.')[0]
                    except Exception as e:
                        log.info("%s when running llvm-config" % str(e))
                        g_llvmver = "LLVM"
                log.info("Llvm directory: %s" % g_llvmdir)
                log.info("Version: %s" % g_llvmver.lower())
            else:
                log.info("The build will proceed without Llvm support")
    if with_version:
        return g_llvmdir, g_llvmver
    else:
        return g_llvmdir



def get_rpath():
    if ismacos():
        return "@loader_path/."
    else:
        return "$ORIGIN/."


def print_compiler_version(log, cc):
    cmd = [cc, "--version"]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    stdout, _ = proc.communicate()
    stdout = stdout.decode().strip()
    log.info(" ".join(cmd) + " :")
    for line in stdout.split("\n"):
        log.info("  " + line.strip())


@memoize()
def get_compiler():
    with TaskContext("Determine the compiler") as log:
        for envvar in ["CXX", "CC"]:
            cc = os.environ.get(envvar, None)
            if cc:
                log.info("Using compiler from environment variable `%s`: %s"
                         % (envvar, cc))
                print_compiler_version(log, cc)
                return cc
            else:
                log.info("Environment variable `%s` is not set" % envvar)
        llvm = get_llvm()
        if llvm:
            cc = os.path.join(llvm, "bin", "clang++")
            if iswindows():
                cc += ".exe"
            if os.path.isfile(cc):
                log.info("Found Clang compiler %s" % cc)
                print_compiler_version(log, cc)
                return cc
            else:
                log.info("Cannot find Clang compiler at %s" % cc)
                cc = None
        else:
            log.info("Llvm installation not found, cannot search for the "
                     "clang++ compiler")
        fname = None
        outname = None
        try:
            fd, fname = tempfile.mkstemp(suffix=".c")
            outname = fname + ".out"
            os.close(fd)
            assert os.path.isfile(fname)
            candidate_compilers = ["clang++", "gcc"]
            try:
                import distutils.ccompiler
                cc = distutils.ccompiler.new_compiler()
                ccname = cc.executables["compiler_cxx"][0]
                candidate_compilers.append(ccname)
                log.info("distutils.ccompiler reports the default compiler to "
                         "be `%s`" % (ccname, ))
            except Exception as e:
                log.info(str(e))
            for cc in candidate_compilers:
                if iswindows() and not cc.endswith(".exe"):
                    cc += ".exe"
                try:
                    cmd = [cc, "-c", fname, "-o", outname]
                    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                            stderr=subprocess.PIPE)
                    stdout, stderr = proc.communicate()
                    stdout = stdout.decode().strip()
                    stderr = stderr.decode().strip()
                    if proc.returncode == 0:
                        log.info("Compiler `%s` will be used" % cc)
                        print_compiler_version(log, cc)
                        return cc
                    else:
                        log.info("Compiler `%s` returned an error when "
                                 "compiling a blank file: <%s>" % (cc, stderr))
                except FileNotFoundError:
                    log.info("Compiler `%s` is not found" % cc)
        finally:
            if fname and os.path.isfile(fname):
                os.remove(fname)
            if outname and os.path.isfile(outname):
                os.remove(outname)
        log.fatal("Suitable C++ compiler cannot be determined. Please "
                  "specify a compiler executable in the `CXX` environment "
                  "variable.")


@memoize()
def is_gcc():
    cc = get_compiler()
    return ("gcc" in cc or "g++" in cc) and ("clang" not in cc)

@memoize()
def is_clang():
    return "clang" in get_compiler()



#-------------------------------------------------------------------------------
# Determine compiler settings
#-------------------------------------------------------------------------------

@memoize()
def get_compile_includes():
    includes = set()
    with TaskContext("Find compile include directories") as log:
        includes.add("c")
        log.info("`c` is the main C++ source directory")

        # Adding the include directory in sys.prefix as an -isystem flag breaks
        # gcc because it messes with its include_next mechanism on Fedora where
        # sys.prefix resolves to /usr.  It doesn't seem necessary either because
        # with non-standard sys.prefix, CONFINCLUDEPY is good enough.
        confincludepy = sysconfig.get_config_var("CONFINCLUDEPY")
        if confincludepy:
            includes.add(confincludepy)
            log.info("`%s` added from CONFINCLUDEPY" % confincludepy)

        # Include path to C++ header files
        llvmdir = get_llvm()
        if llvmdir:
            dir1 = os.path.join(llvmdir, "include")
            dir2 = os.path.join(llvmdir, "include/c++/v1")
            includes.add(dir1)
            includes.add(dir2)
            log.info("`%s` added from Llvm package" % dir1)
            log.info("`%s` added from Llvm package" % dir2)

        includes = list(includes)

        for i, d in enumerate(includes):
            if not os.path.isdir(d):
                includes[i] = None
                log.info("Directory `%s` not found, ignoring" % d)
    return [i for i in includes if i is not None]



def get_default_compile_flags():
    flags = sysconfig.get_config_var("PY_CFLAGS")
    # remove -arch XXX flags, and add "-m64" to force 64-bit only builds
    flags = re.sub(r"-arch \w+\s*", " ", flags) + " -m64"
    # remove -WXXX flags, because we set up all warnings manually afterwards
    flags = re.sub(r"\s*-W[a-zA-Z\-]+\s*", " ", flags)
    # remove -O3 flag since we'll be setting it manually to either -O0 or -O3
    # depending on the debug mode
    flags = re.sub(r"\s*-O\d\s*", " ", flags)
    # remove -DNDEBUG so that the program can use asserts if needed
    flags = re.sub(r"\s*-DNDEBUG\s*", " ", flags)
    # remove '=format-security' because this is not even a real flag...
    flags = re.sub(r"=format-security", "", flags)
    # Clear additional flags not recognized by Clang
    flags = re.sub(r"-fuse-linker-plugin", "", flags)
    flags = re.sub(r"-ffat-lto-objects", "", flags)
    # Add the python include dir as '-isystem' to prevent warnings in Python.h
    if sysconfig.get_config_var("CONFINCLUDEPY"):
        flags += " -isystem %s" % sysconfig.get_config_var("CONFINCLUDEPY")
    # Squash spaces
    flags = re.sub(r"\s+", " ", flags)
    return flags


@memoize()
def get_extra_compile_flags():
    flags = []
    with TaskContext("Determine the extra compiler flags") as log:
        flags += ["-std=c++11"]
        if is_clang():
            flags += ["-stdlib=libc++"]

        # Path to source files / Python include files
        flags += ["-Ic"]

        # Generate 'Position-independent code'. This is required for any
        # dynamically-linked library.
        flags += ["-fPIC"]

        if "DTASAN" in os.environ:
            flags += ["-g3", "-ggdb", "-O0",
                      "-DDTTEST", "-DDTDEBUG",
                      "-fsanitize=address",
                      "-fno-omit-frame-pointer",
                      "-fsanitize-address-use-after-scope",
                      "-shared-libasan"]
        elif "DTDEBUG" in os.environ:
            flags += ["-g3", "-ggdb", "-O0"]
            flags += ["-DDTTEST", "-DDTDEBUG"]
        elif "DTCOVERAGE" in os.environ:
            flags += ["-g2", "--coverage", "-O0"]
            flags += ["-DDTTEST", "-DDTDEBUG"]
        else:
            # Optimize at best level, but still include some debug information
            flags += ["-g2", "-O3"]

        if "CI_EXTRA_COMPILE_ARGS" in os.environ:
            flags += [os.environ["CI_EXTRA_COMPILE_ARGS"]]

        if iswindows():
            flags += ["/W4"]
        elif is_clang():
            # Ignored warnings:
            #   -Wc++98-compat-pedantic:
            #   -Wc99-extensions: since we're targeting C++11, there is no need
            #       to worry about compatibility with earlier C++ versions.
            #   -Wfloat-equal: this warning is just plain wrong...
            #       Comparing x == 0 or x == 1 is always safe.
            #   -Wswitch-enum: generates spurious warnings about missing
            #       cases even if `default` clause is present. -Wswitch
            #       does not suffer from this drawback.
            #   -Wreserved-id-macros: macro _XOPEN_SOURCE is standard on Linux
            #       platform, unclear why Clang would complain about it.
            #   -Wweak-template-vtables: this waning's purpose is unclear, and
            #       it is also unclear how to prevent it...
            #   -Wglobal-constructors, -Wexit-time-destructors: having static
            #       global objects is not only legal, but also unavoidable since
            #       this is the only kind of object that can be passed to a
            #       template...
            flags += [
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
            ]
        elif is_gcc():
            # Ignored warnings:
            #   -Wunused-value: generates spurious warnings for OMP code.
            #   -Wunknown-pragmas: do not warn about clang-specific macros,
            #       ignoring them is just fine...
            flags += [
                "-Wall",
                "-Wno-unused-value",
                "-Wno-unknown-pragmas"
            ]

        for d in get_compile_includes():
            flags += ["-I" + d]
            if d != "c":
                flags += ["-isystem", d]

        i = 0
        while i < len(flags):
            flag = flags[i]
            if i + 1 < len(flags) and not flags[i + 1].startswith("-"):
                flag += " " + flags[i + 1]
                i += 1
            log.info(flag)
            i += 1

    return flags


def get_default_link_flags():
    # No need for TaskContext here, since this is executed in non-verbose
    # mode only.

    flags = sysconfig.get_config_var("LDSHARED")
    # remove the name of the linker program
    flags = re.sub(r"^\w+[\w.\-]+\s+", "", flags)
    # remove -arch XXX flags, and add "-m64" to force 64-bit only builds
    flags = re.sub(r"-arch \w+\s*", "", flags) + " -m64"
    return flags


@memoize()
def get_extra_link_args():
    flags = []
    with TaskContext("Determine the extra linker flags") as log:
        flags += ["-Wl,-rpath,%s" % get_rpath()]

        if islinux() and is_clang():
            flags += ["-lc++"]
        if islinux():
            # On linux we need to pass -shared flag to clang linker which
            # is not used for some reason
            flags += ["-shared"]
        if is_gcc():
            flags += ["-lstdc++", "-lm"]

        if "DTASAN" in os.environ:
            flags += ["-fsanitize=address", "-shared-libasan"]

        if "DTCOVERAGE" in os.environ:
            flags += ["--coverage", "-O0"]

        libs = sorted(set(os.path.dirname(lib)
                          for lib in find_linked_dynamic_libraries()))
        for lib in libs:
            flags += ["-L%s" % lib]

        # flags += ["-L" + sysconfig.get_config_var("LIBDIR")]

        for flag in flags:
            log.info(flag)

    return flags


def required_link_libraries():
    # GCC on Ubuntu18.04 links to the following libraries (`ldd`):
    #   linux-vdso.so.1
    #   /lib64/ld-linux-x86-64.so.2
    #   libstdc++.so.6  => /usr/lib/x86_64-linux-gnu/libstdc++.so.6
    #   libgcc_s.so.1   => /lib/x86_64-linux-gnu/libgcc_s.so.1
    #   libpthread.so.0 => /lib/x86_64-linux-gnu/libpthread.so.0
    #   libc.so.6       => /lib/x86_64-linux-gnu/libc.so.6
    #   libm.so.6       => /lib/x86_64-linux-gnu/libm.so.6
    #   libdl.so.2      => /lib/x86_64-linux-gnu/libdl.so.2
    # These are all standard system libraries, so there is no need to bundle
    # them.
    if is_gcc():
        return []

    # Clang on MacOS links to the following libraries (`otool -L`):
    #   @rpath/libc++.1.dylib
    #   /usr/lib/libSystem.B.dylib
    # In addition, `libc++abi.1.dylib` (or `libc++abi.dylib`) is referenced
    # from `libc++.1.dylib`.
    # The @rpath- libraries have to be bundled into the datatable package.
    #
    if is_clang():
        if ismacos():
            return ["libc++.1.dylib", "libc++abi.dylib"]
        if islinux():
            return ["libc++.so.1", "libc++abi.so.1"]
    return []


@memoize()
def find_linked_dynamic_libraries():
    """
    This function attempts to locate the required link libraries, and returns
    them as a list of absolute paths.
    """
    with TaskContext("Find the required dynamic libraries") as log:
        llvm = get_llvm()
        libs = required_link_libraries()
        resolved = []
        for libname in libs:
            if llvm:
                fullpath = os.path.join(llvm, "lib", libname)
                if os.path.isfile(fullpath):
                    resolved.append(fullpath)
                    log.info("Library `%s` found at %s" % (libname, fullpath))
                    continue
                else:
                    log.info("%s does not exist" % fullpath)
            # Rely on the shell `locate` command to find the dynamic libraries.
            proc = subprocess.Popen(["locate", libname], stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE)
            stdout, stderr = proc.communicate()
            if proc.returncode == 0:
                results = stdout.decode().strip().split("\n")
                results = [r for r in results if r]
                if results:
                    results.sort(key=len)
                    fullpath = results[0]
                    assert os.path.isfile(fullpath), "Invalid path: %r" % (fullpath,)
                    resolved.append(fullpath)
                    log.info("Library `%s` found at %s" % (libname, fullpath))
                    continue
                else:
                    log.fatal("Cannot locate dynamic library `%s`" % libname)
            else:
                print("%s :::: %d :::: %s" % (libname, proc.returncode, stdout.decode()))
                log.fatal("`locate` command returned the following error while looking for %s:\n%s"
                          % (libname, stderr.decode()))
        return resolved



#-------------------------------------------------------------------------------
# Augmented compiler
#-------------------------------------------------------------------------------

def monkey_patch_compiler():
    from distutils.ccompiler import new_compiler
    from subprocess import check_output as run
    cc = new_compiler().__class__

    class NewCC(cc):
        """
        Extension of the standard compiler from distutils. This class
        adds a post-link stage where the dependencies of the dynamic
        library are verified, and fixed if necessary.
        """

        def get_load_dylib_entries(self, executable, log):
            otool_result = run(["otool", "-L", executable]).decode()
            log.info("Checking dependencies of %s"
                     % os.path.basename(executable))
            log.info("  $ otool -L %s" % executable)
            execname = os.path.basename(executable)
            dylibs = []
            for libinfo in otool_result.split('\n')[1:]:
                lib = libinfo.strip().split(' ', 1)[0]
                if lib and os.path.basename(lib) != execname:
                    dylibs.append(lib)
                    log.info("    %s" % lib)
            return dylibs


        def find_recursive_dependencies(self, out, executable, log):
            dylibs = self.get_load_dylib_entries(executable, log)
            for lib in dylibs:
                if lib in out:
                    continue
                out.append(lib)
                if lib.startswith("/usr/lib/"):
                    continue
                if lib.startswith("@rpath/"):
                    resolved_name = os.path.join("datatable", "lib",
                                                 lib[len("@rpath/"):])
                    if not os.path.isfile(resolved_name):
                        raise SystemExit("Dependency %s does not exist"
                                        % resolved_name)
                else:
                    resolved_name = lib
                if resolved_name == executable:
                    continue
                self.find_recursive_dependencies(out, resolved_name, log)


        def relocate_dependencies(self, executable, log):
            dylibs = self.get_load_dylib_entries(executable, log)
            for lib in dylibs:
                if lib.startswith("/usr/lib/") or lib.startswith("@rpath/"):
                    continue
                libname = os.path.basename(lib)
                newname = "@rpath/" + libname
                log.info("Relocating dependency %s"
                         % os.path.basename(libname))
                log.info("  $ install_name_tool -change %s %s %s"
                         % (lib, newname, executable))
                run(["install_name_tool", "-change",
                     lib, newname, executable])
                destname = os.path.join("datatable", "lib", libname)
                if not os.path.exists(destname):
                    log.info("Copying %s -> %s" % (lib, destname))
                    shutil.copyfile(lib, destname)

        def _compile(self, obj, src, ext, cc_args, extra_postargs, pp_opts):
            if cc.__name__ == "UnixCCompiler":
                compiler_so = self.fixup_compiler(self.compiler_so,
                                                  cc_args + extra_postargs)
                try:
                    self.spawn(compiler_so + cc_args + [src, '-o', obj] +
                               extra_postargs)
                except DistutilsExecError as msg:
                    raise CompileError(msg)
            else:
                cc._compile(self, obj, src, ext, cc_args, extra_postargs, pp_opts)


        def fixup_compiler(self, compiler_so, cc_args):
            if ismacos():
                import _osx_support
                compiler_so = _osx_support.compiler_fixup(compiler_so, cc_args)
            for token in ["-Wstrict-prototypes", "-O2"]:
                if token in compiler_so:
                    del compiler_so[compiler_so.index(token)]
            return compiler_so


        def fixup_linker(self):
            # Remove duplicate arguments
            # Remove any explicit library paths -- having a user-defined library
            # path before -L/usr/lib messes up with dylib resolution, and we end
            # up creating a wrong .so file.
            seen_args = set()
            new_linker = []
            delayed_args = []
            for arg in self.library_dirs:
                if not arg.startswith("-L"):
                    arg = "-L" + arg
                delayed_args.append(arg)
            for arg in self.linker_so:
                if arg in seen_args: continue
                seen_args.add(arg)
                if arg.startswith("-L"):
                    delayed_args.append(arg)
                else:
                    new_linker.append(arg)
            self.linker_so = new_linker
            self.library_dirs = []
            self.delayed_libraries = delayed_args


        def link(self,
                 target_desc,
                 objects,
                 output_filename,
                 output_dir=None,
                 libraries=None,
                 library_dirs=None,
                 runtime_library_dirs=None,
                 export_symbols=None,
                 debug=0,
                 extra_preargs=None,
                 extra_postargs=None,
                 build_temp=None,
                 target_lang=None):
            self.fixup_linker()
            extra_postargs.extend(self.delayed_libraries)
            super().link(target_desc=target_desc,
                         objects=objects,
                         output_filename=output_filename,
                         output_dir=output_dir,
                         libraries=libraries,
                         library_dirs=library_dirs,
                         runtime_library_dirs=runtime_library_dirs,
                         export_symbols=export_symbols,
                         debug=debug,
                         extra_preargs=extra_preargs,
                         extra_postargs=extra_postargs,
                         build_temp=build_temp,
                         target_lang=target_lang)
            if output_dir is not None:
                output_filename = os.path.join(output_dir, output_filename)
            if ismacos():
                self.postlink(output_filename)


        def postlink(self, outname):
            print()
            with TaskContext("Post-link processing") as log:
                log.info("Output file: %s" % outname)
                self.relocate_dependencies(outname, log)
                dylibs = []
                self.find_recursive_dependencies(dylibs, outname, log)
                log.info("Resolved list of dependencies:")
                for lib in sorted(dylibs):
                    log.info("  " + lib)


    vars(sys.modules[cc.__module__])[cc.__name__] = NewCC




#-------------------------------------------------------------------------------
# Run as a script
#-------------------------------------------------------------------------------

def usage():
    print("Usage: \n"
          "  python setup_utils.py CMD\n\n"
          "where CMD can be one of:\n"
          "  ccflags\n"
          "  compiler\n"
          "  ext_suffix\n"
          "  ldflags\n"
          "  version\n"
          )


if __name__ == "__main__":
    verbose = False
    if len(sys.argv) == 2:
        cmd = sys.argv[1]
        if cmd == "--help":
            usage()
        elif cmd == "ccflags":
            flags = [get_default_compile_flags()] + get_extra_compile_flags()
            print(" ".join(flags))
        elif cmd == "compiler":
            print(get_compiler())
        elif cmd == "ext_suffix":
            print(sysconfig.get_config_var("EXT_SUFFIX"))
        elif cmd == "ldflags":
            flags = [get_default_link_flags()] + get_extra_link_args()
            print(" ".join(flags))
        elif cmd == "version":
            make_git_version_file(True)
            print(get_datatable_version())
        else:
            print("Unknown command: %s" % cmd)
            sys.exit(1)
    else:
        usage()
        sys.exit(1)
