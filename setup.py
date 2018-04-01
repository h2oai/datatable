#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
"""
Build script for the `datatable` module.

    $ python setup.py bdist_wheel
    $ twine upload dist/*
"""
import os
import shutil
import sys
import re
import sysconfig
from functools import lru_cache as memoize
from setuptools import setup, find_packages
from distutils.core import Extension
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
        raise RuntimeError("Could not detect version from the "
                           "__version__.py file")
    # Append build suffix if necessary
    suffix = os.environ.get("CI_VERSION_SUFFIX")
    if suffix:
        version += "+" + suffix
    return version


def get_c_sources():
    """Find all C source files in the "c/" directory."""
    c_sources = []
    for root, dirs, files in os.walk("c"):
        for name in files:
            if name.endswith(".c") or name.endswith(".cc"):
                c_sources.append(os.path.join(root, name))
    return c_sources


def get_py_sources():
    """Find python source directories."""
    packages = find_packages(exclude=["tests", "tests.munging", "temp", "c"])
    print("\nFound packages: %r\n" % packages, file=stderr)
    return packages


def get_test_dependencies():
    # Test dependencies exposed as extras, based on:
    # https://stackoverflow.com/questions/29870629
    return [
        "pandas",
        "pytest>=3.1",
        "pytest-cov",
        "pytest-benchmark>=3.1",
        "pytest-ordering>=0.5",
    ]



#-------------------------------------------------------------------------------
# Determine compiler settings
#-------------------------------------------------------------------------------

@memoize()
def get_llvm(with_version=False):
    curdir = os.path.dirname(os.path.abspath(__file__))
    for LLVMX in ["LLVM4", "LLVM5"]:
        # Check whether there is 'llvmx' folder in the package folder
        if LLVMX in os.environ:
            llvmdir = os.environ[LLVMX]
        else:
            d = os.path.join(curdir, "datatable/" + LLVMX.lower())
            if os.path.isdir(d):
                llvmdir = d
            else:
                continue
        if not os.path.isdir(llvmdir):
            raise ValueError("Variable %s = %r is not a directory"
                             % (LLVMX, llvmdir))
        if with_version:
            return llvmdir, LLVMX
        return llvmdir
    # Failed to find LLVM
    raise RuntimeError("Environment variables LLVM4 or LLVM5 are not set. "
                       "Please set one of these variables to the location of "
                       "the Clang+Llvm distribution, which you can download "
                       "from http://releases.llvm.org/download.html")


@memoize()
def get_rpath():
    if sys.platform == "darwin":
        return "@loader_path/."
    else:
        return "$ORIGIN/."


@memoize()
def get_cc(with_isystem=False):
    clang = os.path.join(get_llvm(), "bin", "clang++")
    if not os.path.exists(clang):
        raise RuntimeError("Cannot find CLang compiler at `%r`" % clang)
    if with_isystem and sysconfig.get_config_var("CONFINCLUDEPY"):
        clang += " -isystem " + sysconfig.get_config_var("CONFINCLUDEPY")
    return clang


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

    # This macro is needed to combat "-DNDEBUG" flag in default Python. This
    # flag in turn enables all `assert` statements at the C level.
    if "DTDEBUG" in os.environ:
        flags += ["-DNONDEBUG"]

    # Enable/disable OpenMP support
    if "DTNOOPENMP" in os.environ:
        flags.append("-DDTNOOMP")
        flags.append("-Wno-source-uses-openmp")
    else:
        flags.insert(0, "-fopenmp")

    if "DTDEBUG" in os.environ:
        flags += ["-g", "-ggdb", "-O0"]
    elif "DTASAN" in os.environ:
        flags += ["-g", "-ggdb", "-O0",
                  "-fsanitize=address",
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
    #   -Wcovered-switch-default: we add `default` statement to
    #       an exhaustive switch to guard against memory
    #       corruption and careless enum definition expansion.
    #   -Wfloat-equal: this warning is just plain wrong...
    #       Comparing x == 0 or x == 1 is always safe.
    #   -Wgnu-statement-expression: we use GNU statement-as-
    #       expression syntax in some macros...
    #   -Wswitch-enum: generates spurious warnings about missing
    #       cases even if `default` clause is present. -Wswitch
    #       does not suffer from this drawback.
    #   -Wdeprecated: warning about compiling .c files under C++
    #       mode... we should just rename those files at some point.
    flags += [
        "-Weverything",
        "-Wno-covered-switch-default",
        "-Wno-float-equal",
        "-Wno-gnu-statement-expression",
        "-Wno-switch-enum",
        "-Wno-old-style-cast",
        "-Wno-c++98-compat-pedantic",
        "-Wno-nested-anon-types",
        "-Wno-c99-extensions",
        "-Wno-deprecated",
        "-Werror=implicit-function-declaration",
        "-Werror=incompatible-pointer-types",
        "-Wno-weak-vtables",  # TODO: Remove
        "-Wno-weak-template-vtables",
    ]
    return flags


@memoize()
def get_default_link_flags():
    flags = sysconfig.get_config_var("LDSHARED")
    # remove the name of the linker program
    flags = re.sub(r"^\w+[\w\.\-]+\s+", "", flags)
    # remove -arch XXX flags, and add "-m64" to force 64-bit only builds
    flags = re.sub(r"-arch \w+\s*", "", flags) + " -m64"
    # Add "-isystem" path with system libraries
    flags += " -isystem " + sysconfig.get_config_var("CONFINCLUDEPY")
    return flags


@memoize()
def get_extra_link_args():
    flags = ["-L%s" % os.path.join(get_llvm(), "lib"),
             "-Wl,-rpath,%s" % get_rpath()]

    if sys.platform == "linux":
        flags += ["-lc++"]

    if not("DTNOOPENMP" in os.environ):
        flags += ["-fopenmp"]

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

argcmd = sys.argv[1] if len(sys.argv) == 2 else ""

if argcmd.startswith("get_"):
    os.environ["DTDEBUG"] = "1"  # Force debug flag
    cmd = argcmd[4:]
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
        raise RuntimeError("Unknown setup.py command '%s'" % argcmd)
    sys.exit(0)



#-------------------------------------------------------------------------------
# Prepare the environment
#-------------------------------------------------------------------------------

# Verify the LLVM4/LLVM5 installation directory
llvmx, llvmver = get_llvm(True)
llvm_config = os.path.join(llvmx, "bin", "llvm-config")
clang = os.path.join(llvmx, "bin", "clang++")
libsdir = os.path.join(llvmx, "lib")
includes = os.path.join(llvmx, "include")
llvmlite_req = ("==0.20.0" if llvmver == "LLVM4" else
                ">=0.21.0" if llvmver == "LLVM5" else None)
for f in [llvm_config, clang, libsdir, includes]:
    if not os.path.exists(f):
        raise RuntimeError("Cannot find %s folder. "
                           "Is this a valid installation?" % f)

# Compiler
os.environ["CC"] = os.environ["CXX"] = get_cc(True)

# Linker
# On linux we need to pass proper flag to clang linker which
# is not used for some reason at linux
if "DTCOVERAGE" in os.environ and sys.platform == "linux":
    os.environ["LDSHARED"] = clang

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



#-------------------------------------------------------------------------------
# Main setup
#-------------------------------------------------------------------------------
setup(
    name="datatable",
    version=get_version(),

    description="Python library for fast multi-threaded data manipulation and "
                "munging.",

    # The homepage
    url="https://github.com/h2oai/datatable.git",

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
        "psutil"
    ],

    tests_require=get_test_dependencies(),

    extras_require={
        "testing": get_test_dependencies()
    },

    zip_safe=True,

    ext_modules=[
        Extension(
            "datatable/lib/_datatable",
            include_dirs=["c"],
            sources=get_c_sources(),
            extra_compile_args=get_extra_compile_flags(),
            extra_link_args=get_extra_link_args(),
        ),
    ],

    package_dir={"datatable": "datatable"},
    package_data={"datatable": ["lib/*.*"]},
)
