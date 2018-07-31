#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
"""
Build script for the `datatable` module.

    $ python setup.py sdist
    $ python setup.py bdist_wheel
    $ twine upload dist/*
"""
import os
import shutil
import sys
import re
import sysconfig
from functools import lru_cache as memoize
from setuptools import setup, find_packages, Extension
from sys import stderr



#-------------------------------------------------------------------------------
# Generic helpers
#-------------------------------------------------------------------------------

def get_version():
    """Determine the package version."""
    version = None
    with open("datatable/__version__.py", encoding="utf-8") as f:
        rx = re.compile(r"""version\s*=\s*['"]([\d.]*)['"]\s*""")
        for line in f:
            mm = re.match(rx, line)
            if mm is not None:
                version = mm.group(1)
                break
    if version is None:
        raise SystemExit("Could not detect project version from the "
                         "__version__.py file")
    # Append build suffix if necessary
    suffix = os.environ.get("CI_VERSION_SUFFIX")
    if suffix:
        # See https://www.python.org/dev/peps/pep-0440/ for valid versioning
        # schemes.
        mm = re.match(r"(?:master|dev)[.+_-]?(\d+)", suffix)
        if mm:
            suffix = "dev" + str(mm.group(1))
        version += "." + suffix
    return version


def get_c_sources(folder, include_headers=False):
    """Find all C/C++ source files in the `folder` directory."""
    allowed_extensions = [".c", ".C", ".cc", ".cpp", ".cxx", ".c++"]
    if include_headers:
        allowed_extensions.extend([".h", ".hpp"])
    sources = []
    for root, dirs, files in os.walk(folder):
        for name in files:
            ext = os.path.splitext(name)[1]
            if ext in allowed_extensions:
                sources.append(os.path.join(root, name))
    return sources


def get_py_sources():
    """Find python source directories."""
    packages = find_packages(exclude=["tests", "tests.munging", "temp", "c"])
    print("\nFound packages: %r\n" % packages, file=stderr)
    return packages


def get_test_dependencies():
    # Test dependencies exposed as extras, based on:
    # https://stackoverflow.com/questions/29870629
    return [
        "pytest>=3.1",
        "pytest-cov",
        "pytest-benchmark>=3.1",
        "pytest-ordering>=0.5",
    ]


def make_git_version_file():
    import subprocess
    if not os.path.isdir(".git"):
        return
    out = subprocess.check_output(["git", "rev-parse", "HEAD"])
    githash = out.decode("ascii").strip()
    with open("datatable/__git__.py", "w", encoding="utf-8") as o:
        o.write(
            "#!/usr/bin/env python3\n"
            "# © H2O.ai 2018; -*- encoding: utf-8 -*-\n"
            "#   This Source Code Form is subject to the terms of the\n"
            "#   Mozilla Public License, v2.0. If a copy of the MPL was\n"
            "#   not distributed with this file, You can obtain one at\n"
            "#   http://mozilla.org/MPL/2.0/.\n"
            "# ----------------------------------------------------------\n"
            "# This file was auto-generated from setup.py\n\n"
            "__git_revision__ = \'%s\'\n"
            % githash)




#-------------------------------------------------------------------------------
# Determine compiler settings
#-------------------------------------------------------------------------------

@memoize()
def get_llvm(with_version=False):
    curdir = os.path.dirname(os.path.abspath(__file__))
    llvmdir = None
    for LLVMX in ["LLVM4", "LLVM5", "LLVM6"]:
        d = os.path.join(curdir, "datatable/" + LLVMX.lower())
        if LLVMX in os.environ:
            llvmdir = os.environ[LLVMX]
            if llvmdir:
                break
        elif os.path.isdir(d):
            llvmdir = d
            break

    if llvmdir and not os.path.isdir(llvmdir):
        raise SystemExit("Environment variable %s = %r is not a directory"
                         % (LLVMX, llvmdir))
    if not llvmdir:
        raise SystemExit("Environment variables LLVM4, LLVM5 or LLVM6 are not "
                         "set. Please set one of these variables to the location"
                         " of the Clang+Llvm distribution, which you can "
                         " downloadfrom http://releases.llvm.org/download.html")

    if with_version:
        return llvmdir, LLVMX
    return llvmdir


@memoize()
def get_rpath():
    if sys.platform == "darwin":
        return "@loader_path/."
    else:
        return "$ORIGIN/."


@memoize()
def get_cc(with_isystem=False):
    cc = os.path.join(get_llvm(), "bin", "clang++")
    if not os.path.exists(cc):
        raise SystemExit("Cannot find CLang compiler at `%r`" % cc)
    if with_isystem and sysconfig.get_config_var("CONFINCLUDEPY"):
        cc += " -isystem " + sysconfig.get_config_var("CONFINCLUDEPY")
    return cc


@memoize()
def get_default_compile_flags():
    flags = sysconfig.get_config_var("PY_CFLAGS")
    # remove -arch XXX flags, and add "-m64" to force 64-bit only builds
    flags = re.sub(r"-arch \w+\s*", "", flags) + " -m64"
    # remove -WXXX flags, because we set up all warnings manually afterwards
    flags = re.sub(r"\s*-W[a-zA-Z\-]+\s*", " ", flags)
    # remove -O3 flag since we'll be setting it manually to either -O0 or -O3
    # depending on the debug mode
    flags = re.sub(r"\s*-O\d\s*", " ", flags)
    # remove -DNDEBUG so that the program can easier use asserts
    flags = re.sub(r"\s*-DNDEBUG\s*", " ", flags)
    return flags


@memoize()
def get_extra_compile_flags():
    flags = []
    if sysconfig.get_config_var("CONFINCLUDEPY"):
        # Marking this directory as "isystem" prevents Clang from issuing
        # warnings for those files
        flags += ["-isystem " + sysconfig.get_config_var("CONFINCLUDEPY"),
                  "-I" + sysconfig.get_config_var("CONFINCLUDEPY")]

    flags += ["-std=gnu++11", "-stdlib=libc++", "-x", "c++"]

    # Path to source files / Python include files
    flags += ["-Ic",
              "-I" + os.path.join(sys.prefix, "include")]

    # Include path to C++ header files
    flags += ["-I" + get_llvm() + "/include/c++/v1",
              "-I" + get_llvm() + "/include",
              "-isystem " + get_llvm() + "/include/c++/v1"]

    # Enable OpenMP support
    flags.insert(0, "-fopenmp")

    if "DTDEBUG" in os.environ:
        flags += ["-g", "-ggdb", "-O0"]
    elif "DTASAN" in os.environ:
        flags += ["-g", "-ggdb", "-O0",
                  "-fsanitize=address",
                  "-fno-omit-frame-pointer",
                  "-fsanitize-address-use-after-scope",
                  "-shared-libasan"]
    elif "DTCOVERAGE" in os.environ:
        flags += ["-g", "--coverage", "-O0"]
    else:
        flags += ["-O3"]

    if "CI_EXTRA_COMPILE_ARGS" in os.environ:
        flags += [os.environ["CI_EXTRA_COMPILE_ARGS"]]

    if "-O0" in flags:
        flags += ["-DDTDEBUG"]

    # Ignored warnings:
    #   -Wc++98-compat-pedantic:
    #   -Wc99-extensions: since we're targeting C++11, there is no need to
    #       worry about compatibility with earlier C++ versions.
    #   -Wfloat-equal: this warning is just plain wrong...
    #       Comparing x == 0 or x == 1 is always safe.
    #   -Wswitch-enum: generates spurious warnings about missing
    #       cases even if `default` clause is present. -Wswitch
    #       does not suffer from this drawback.
    #   -Wweak-template-vtables: this waning's purpose is unclear, and it
    #       is also unclear how to prevent it...
    flags += [
        "-Weverything",
        "-Wno-c++98-compat-pedantic",
        "-Wno-c99-extensions",
        "-Wno-float-equal",
        "-Wno-switch-enum",
        "-Wno-weak-template-vtables",
    ]
    return flags


@memoize()
def get_default_link_flags():
    flags = sysconfig.get_config_var("LDSHARED")
    # remove the name of the linker program
    flags = re.sub(r"^\w+[\w.\-]+\s+", "", flags)
    # remove -arch XXX flags, and add "-m64" to force 64-bit only builds
    flags = re.sub(r"-arch \w+\s*", "", flags) + " -m64"
    # Add "-isystem" path with system libraries
    flags += " -isystem " + sysconfig.get_config_var("CONFINCLUDEPY")
    return flags


@memoize()
def get_extra_link_args():
    flags = ["-L%s" % os.path.join(get_llvm(), "lib"),
             "-Wl,-rpath,%s" % get_rpath()]

    flags += ["-fopenmp"]

    # Omit all symbol information from the output
    # ld warns that this option is obsolete and is ignored. However with this
    # option the size of the executable is ~25% smaller...
    if "DTDEBUG" not in os.environ:
        flags += ["-s"]

    if sys.platform == "linux":
        flags += ["-lc++"]

    if "DTASAN" in os.environ:
        flags += ["-fsanitize=address", "-shared-libasan"]

    if "DTCOVERAGE" in os.environ:
        flags += ["--coverage", "-O0"]
        # On linux we need to pass proper flag to clang linker which
        # is not used for some reason at linux
        if sys.platform == "linux":
            flags += ["-shared"]
    return flags



#-------------------------------------------------------------------------------
# Process extra commands
#-------------------------------------------------------------------------------

def process_args(cmd):
    """
    Support for additional setup.py commands:
        python setup.py get_CC
        python setup.py get_CCFLAGS
        python setup.py get_LDFLAGS
        python setup.py get_EXTEXT
    """
    if not cmd.startswith("get_"):
        return
    os.environ["DTDEBUG"] = "1"  # Force debug flag
    cmd = cmd[4:]
    if cmd == "EXTEXT":
        print(sysconfig.get_config_var("EXT_SUFFIX"))
    elif cmd == "CC":
        print(get_cc())
    elif cmd == "CCFLAGS":
        flags = [get_default_compile_flags()] + get_extra_compile_flags()
        print(" ".join(flags))
    elif cmd == "LDFLAGS":
        flags = [get_default_link_flags()] + get_extra_link_args()
        print(" ".join(flags))
    else:
        raise SystemExit("Unknown setup.py command '%s'" % cmd)
    sys.exit(0)


argcmd = ""
if len(sys.argv) == 2:
    argcmd = sys.argv[1]
    process_args(argcmd)



#-------------------------------------------------------------------------------
# Prepare the environment
#-------------------------------------------------------------------------------

# Verify the LLVM4/LLVM5 installation directory
llvmx, llvmver = get_llvm(True)
llvm_config = os.path.join(llvmx, "bin", "llvm-config")
clang = os.path.join(llvmx, "bin", "clang++")
libsdir = os.path.join(llvmx, "lib")
includes = os.path.join(llvmx, "include")
llvmlite_req = (">=0.20.0,<0.21.0" if llvmver == "LLVM4" else
                ">=0.21.0,<0.23.0" if llvmver == "LLVM5" else
                ">=0.23.0        " if llvmver == "LLVM6" else None)
for ff in [llvm_config, clang, libsdir, includes]:
    if ff == llvm_config and sys.platform == "win32":
        # There is no llvm-config on Windows:
        # https://github.com/ihnorton/Clang.jl/issues/183
        continue
    ff = os.path.abspath(ff)
    if not os.path.exists(ff):
        raise SystemExit("Cannot find %s. "
                         "Is this a valid Llvm installation?" % ff)

# Compiler
os.environ["CC"] = os.environ["CXX"] = get_cc(True)

# Linker
# On linux we need to pass proper flag to clang linker which
# is not used for some reason at linux
if "DTCOVERAGE" in os.environ and sys.platform == "linux":
    os.environ["LDSHARED"] = clang

if "MACOSX_DEPLOYMENT_TARGET" not in os.environ and sys.platform == "darwin":
    os.environ["MACOSX_DEPLOYMENT_TARGET"] = "10.13"

# Compute runtime libpath with respect to bundled LLVM libraries
if sys.platform == "darwin":
    extra_libs = ["libomp.dylib"]
else:
    extra_libs = ["libomp.so", "libc++.so.1", "libc++abi.so.1"]

# Copy system libraries into the datatable/lib folder, so that they can be
# packaged with the wheel
if not os.path.exists(os.path.join("datatable", "lib")):
    os.mkdir(os.path.join("datatable", "lib"))
for libname in extra_libs:
    srcfile = os.path.join(libsdir, libname)
    tgtfile = os.path.join("datatable", "lib", libname)
    if not os.path.exists(tgtfile):
        print("Copying %s to %s" % (srcfile, tgtfile), file=stderr)
        shutil.copy(srcfile, tgtfile)

# Force to build for a 64-bit platform only
os.environ["ARCHFLAGS"] = "-m64"

# If we need to install llvmlite, this would help
os.environ["LLVM_CONFIG"] = llvm_config

print("Setting environment variables:", file=stderr)
for n in ["CC", "CXX", "LDFLAGS", "ARCHFLAGS", "LLVM_CONFIG"]:
    print("  %s = %s" % (n, os.environ.get(n, "")), file=stderr)

make_git_version_file()


#-------------------------------------------------------------------------------
# Main setup
#-------------------------------------------------------------------------------
setup(
    name="datatable",
    version=get_version(),

    description="Python library for fast multi-threaded data manipulation and "
                "munging.",
    long_description="""
        This is a Python package for manipulating 2-dimensional tabular data
        structures (aka data frames). It is close in spirit to pandas or SFrame;
        however we put specific emphasis on speed and big data support. As the
        name suggests, the package is closely related to R's data.table and
        attempts to mimic its core algorithms and API.

        See https://github.com/h2oai/datatable for more details.
    """,

    # The homepage
    url="https://github.com/h2oai/datatable",

    # Author details
    author="Pasha Stetsenko",
    author_email="pasha@h2o.ai",

    license="Mozilla Public License v2.0",

    classifiers=[
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "Intended Audience :: Science/Research",
        "License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)",
        "Programming Language :: Python :: 3.5",
        "Programming Language :: Python :: 3.6",
        "Topic :: Scientific/Engineering :: Information Analysis",
    ],
    keywords=["datatable", "data", "dataframe", "frame", "data.table",
              "munging", "numpy", "pandas", "data processing", "ETL"],

    packages=get_py_sources(),

    # Runtime dependencies
    install_requires=[
        "typesentry>=0.2.4",
        "blessed",
        "llvmlite" + llvmlite_req,
    ],

    python_requires=">=3.5",

    tests_require=get_test_dependencies(),

    extras_require={
        "testing": get_test_dependencies()
    },

    zip_safe=True,

    ext_modules=[
        Extension(
            "datatable/lib/_datatable",
            include_dirs=["c"],
            sources=get_c_sources("c"),
            extra_compile_args=get_extra_compile_flags(),
            extra_link_args=get_extra_link_args(),
            language="c++",
        ),
    ],

    package_dir={"datatable": "datatable"},
    package_data={"datatable": ["lib/*.*"]},
)
