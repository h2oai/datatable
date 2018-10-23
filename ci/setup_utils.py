#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import os
import re
import subprocess
import sys
import sysconfig
import tempfile
from functools import lru_cache as memoize

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

def islinux():
    return sys.platform == "linux"

def ismacos():
    return sys.platform == "darwin"

def iswindows():
    return sys.platform == "win32"



def get_datatable_version():
    with TaskContext("Determine datatable version") as log:
        version = None
        filename = os.path.join(dtroot(), "datatable", "__version__.py")
        if not os.path.isfile(filename):
            log.fatal("The version file %s does not exist" % filename)
        log.info("Reading file " + filename)
        with open(filename, encoding="utf-8") as f:
            rx = re.compile(r"version\s*=\s*['\"]([\d.]*)['\"]\s*")
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
            # See https://www.python.org/dev/peps/pep-0440/ for the acceptable
            # versioning schemes.
            log.info("... appending version suffix " + suffix)
            mm = re.match(r"(?:master|dev)[.+_-]?(\d+)", suffix)
            if mm:
                suffix = "dev" + str(mm.group(1))
            version += "." + suffix
            log.info("Final version = " + version)
        return version



def make_git_version_file(force):
    import subprocess
    with TaskContext("Generate git version file" + " (force)" * force) as log:
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
        filename = os.path.join(dtroot(), "datatable", "__git__.py")
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



@memoize()
def get_compiler():
    with TaskContext("Determine the compiler") as log:
        for envvar in ["CXX", "CC"]:
            cc = os.environ.get(envvar, None)
            if cc:
                log.info("Using compiler from environment variable `%s`: %s"
                         % (envvar, cc))
                return cc
            else:
                log.info("Environment variable `%s` is not set" % envvar)
        llvm = get_llvm()
        if llvm:
            cc = os.path.join(llvm, "bin", "clang++")
            if os.path.isfile(cc):
                log.info("Found Clang compiler %s" % cc)
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
                try:
                    cmd = [cc, "-c", fname, "-o", outname]
                    cmd += ["-fopenmp"]
                    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                            stderr=subprocess.PIPE)
                    stdout, stderr = proc.communicate()
                    stdout = stdout.decode().strip()
                    stderr = stderr.decode().strip()
                    if proc.returncode == 0:
                        log.info("Compiler `%s` will be used" % cc)
                        return cc
                    elif "-fopenmp" in stderr:
                        log.info("Compiler `%s` does not support OpenMP" % cc)
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
    return "gcc" in get_compiler()

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

        confincludepy = sysconfig.get_config_var("CONFINCLUDEPY")
        if confincludepy:
            includes.add(confincludepy)
            log.info("`%s` added from CONFINCLUDEPY" % confincludepy)

        sysprefixinclude = os.path.join(sys.prefix, "include")
        includes.add(sysprefixinclude)
        log.info("`%s` added from sys.prefix" % sysprefixinclude)

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
    return sorted(includes)



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
    # Add the python include dir as '-isystem' to prevent warnings in Python.h
    if sysconfig.get_config_var("CONFINCLUDEPY"):
        flags += " -isystem %s" % sysconfig.get_config_var("CONFINCLUDEPY")
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

        # Enable OpenMP support
        flags.insert(0, "-fopenmp")

        if "DTDEBUG" in os.environ:
            flags += ["-g", "-ggdb", "-O0"]
            flags += ["-DDTTEST"]
        elif "DTASAN" in os.environ:
            flags += ["-g", "-ggdb", "-O0",
                      "-fsanitize=address",
                      "-fno-omit-frame-pointer",
                      "-fsanitize-address-use-after-scope",
                      "-shared-libasan"]
        elif "DTCOVERAGE" in os.environ:
            flags += ["-g", "--coverage", "-O0"]
            flags += ["-DDTTEST"]
        else:
            flags += ["-O3"]

        if "CI_EXTRA_COMPILE_ARGS" in os.environ:
            flags += [os.environ["CI_EXTRA_COMPILE_ARGS"]]

        if "-O0" in flags:
            flags += ["-DDTDEBUG"]

        if is_clang():
            # Ignored warnings:
            #   -Wc++98-compat-pedantic:
            #   -Wc99-extensions: since we're targeting C++11, there is no need
            #       to worry about compatibility with earlier C++ versions.
            #   -Wfloat-equal: this warning is just plain wrong...
            #       Comparing x == 0 or x == 1 is always safe.
            #   -Wswitch-enum: generates spurious warnings about missing
            #       cases even if `default` clause is present. -Wswitch
            #       does not suffer from this drawback.
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
                "-Wno-switch-enum",
                "-Wno-weak-template-vtables",
            ]
        if is_gcc():
            # Ignored warnings:
            #   -Wunused-value: generates spurious warnings for OMP code.
            flags += ["-Wall", "-Wno-unused-value"]

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
        flags += ["-fopenmp"]

        # Omit all symbol information from the output
        # ld warns that this option is obsolete and is ignored. However with
        # this option the size of the executable is ~25% smaller...
        if "DTDEBUG" not in os.environ:
            flags += ["-s"]

        if islinux() and is_clang():
            # On linux we need to pass -shared flag to clang linker which
            # is not used for some reason at linux
            flags += ["-lc++", "-shared"]

        if "DTASAN" in os.environ:
            flags += ["-fsanitize=address", "-shared-libasan"]

        if "DTCOVERAGE" in os.environ:
            flags += ["--coverage", "-O0"]

        libs = sorted(set(os.path.dirname(lib)
                          for lib in find_linked_dynamic_libraries()))
        for lib in libs:
            flags += ["-L%s" % lib]

        for flag in flags:
            log.info(flag)

    return flags


def required_link_libraries():
    if is_gcc():
        return []
    if ismacos():
        return ["libomp.dylib", "libc++.dylib", "libc++abi.dylib"]
    if islinux():
        return ["libomp.so", "libc++.so.1", "libc++abi.so.1"]
    if iswindows():
        return ["libomp.dll", "libc++.dll", "libc++abi.dll"]
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
                if results:
                    results.sort(key=len)
                    fullpath = results[0]
                    assert os.path.isfile(fullpath)
                    resolved.append(fullpath)
                    log.info("Library `%s` found at %s" % (libname, fullpath))
                    continue
                else:
                    log.fatal("Cannot locate dynamic library `%s`" % libname)
            else:
                log.fatal("`locate` command returned the following error:\n%s"
                          % stderr.decode())
        return resolved




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
            os.environ["DTDEBUG"] = "1"  # Force the debug flag
            flags = [get_default_compile_flags()] + get_extra_compile_flags()
            print(" ".join(flags))
        elif cmd == "compiler":
            print(get_compiler())
        elif cmd == "ext_suffix":
            print(sysconfig.get_config_var("EXT_SUFFIX"))
        elif cmd == "ldflags":
            os.environ["DTDEBUG"] = "1"  # Force the debug flag
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
