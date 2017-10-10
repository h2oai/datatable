#!/usr/bin/env python
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
"""
Build script for the `datatable` module.

    $ python setup.py bdist_wheel
    $ twine upload dist/*
"""
import os
import shutil
import sys
import re
import subprocess
import sysconfig
from setuptools import setup, find_packages
from distutils.core import Extension
from sys import stderr

curdir = os.path.dirname(os.path.abspath(__file__))

# Determine the version
version = None
with open("datatable/__version__.py") as f:
    rx = re.compile(r"""version\s*=\s*['"]([\d.]*)['"]\s*""")
    for line in f:
        mm = re.match(rx, line)
        if mm is not None:
            version = mm.group(1)
            break
if version is None:
    raise RuntimeError("Could not detect version from the __version__.py file")
# Append build suffix if necessary
if os.environ.get("CI_VERSION_SUFFIX"):
    version = "%s+%s" % (version, os.environ["CI_VERSION_SUFFIX"])

# Find all C source files in the "c/" directory
c_sources = []
for root, dirs, files in os.walk("c"):
    for name in files:
        if name.endswith(".c") or name.endswith(".cxx") or name.endswith(".cc"):
            c_sources.append(os.path.join(root, name))

# Find python source directories
packages = find_packages(exclude=["tests", "tests.munging", "temp", "c"])
print("\nFound packages: %r\n" % packages, file=stderr)


#-------------------------------------------------------------------------------
# Prepare the environment
#-------------------------------------------------------------------------------

# Check whether there is 'llvm4' folder in the package folder
if "LLVM4" not in os.environ:
    d = os.path.join(curdir, "datatable/llvm4")
    if os.path.isdir(d):
        os.environ["LLVM4"] = d

# Verify the LLVM4 installation directory
if "LLVM4" in os.environ:
    llvm4 = os.path.expanduser(os.environ["LLVM4"])
    if llvm4.endswith("/"):
        llvm4 = llvm4[:-1]
    if " " in llvm4:
        raise ValueError("LLVM4 directory %r contains spaces -- this is not "
                         "supported, please move the folder, or make a symlink "
                         "or provide a 'short' name (if on Windows)" % llvm4)
    if not os.path.isdir(llvm4):
        raise ValueError("Variable LLVM4 = %r is not a directory" % llvm4)
    llvm_config = os.path.join(llvm4, "bin", "llvm-config")
    clang = os.path.join(llvm4, "bin", "clang++")
    libsdir = os.path.join(llvm4, "lib")
    includes = os.path.join(llvm4, "include")
    for f in [llvm_config, clang, libsdir, includes]:
        if not os.path.exists(f):
            raise RuntimeError("Cannot find %r inside the LLVM4 folder. "
                               "Is this a valid installation?" % f)
    ver = subprocess.check_output([llvm_config, "--version"]).decode().strip()
    if not ver.startswith("4.0."):
        raise RuntimeError("Wrong LLVM version: expected 4.0.x but "
                           "found %s" % ver)
else:
    raise RuntimeError("Environment variable LLVM4 is not set. Please set this "
                       "variable to the location of the Clang+Llvm-4.0.0 "
                       "distribution, which you can download from "
                       "http://releases.llvm.org/download.html#4.0.0")

# Compiler
os.environ["CC"] = clang + " "
os.environ["CXX"] = clang + " "
if sysconfig.get_config_var("CONFINCLUDEPY"):
    # Marking this directory as "isystem" prevents Clang from issuing warnings
    # for those files
    os.environ["CC"] += "-isystem " + sysconfig.get_config_var("CONFINCLUDEPY")
    os.environ["CXX"] += "-isystem " + sysconfig.get_config_var("CONFINCLUDEPY")

# Linker
# os.environ["LDSHARED"] = clang

# Compute runtime libpath with respect to bundled LLVM libraries
if sys.platform == "darwin":
    extra_libs = ["libomp.dylib"]
    rpath = "@loader_path/."
else:
    extra_libs = ["libomp.so", "libc++.so.1", "libc++abi.so.1"]
    rpath = "$ORIGIN/."

# Copy system libraries into the datatable/lib folder, so that they can be
# packaged with the wheel
if not os.path.exists(os.path.join("datatable", "lib")):
    os.mkdir(os.path.join("datatable", "lib"))
for libname in extra_libs:
    srcfile = os.path.join(libsdir, libname)
    tgtfile = os.path.join("datatable", "lib", libname)
    if not os.path.exists(tgtfile):
        print("Copying %s to %s" % (srcfile, tgtfile))
        shutil.copy(srcfile, tgtfile)

# Add linker flags: path to system libraries, and the rpath
os.environ["LDFLAGS"] = "-L%s -Wl,-rpath,%s" % (libsdir, rpath)

# Force to build for a 64-bit platform only
os.environ["ARCHFLAGS"] = "-m64"

# If we need to install llvmlite, this would help
os.environ["LLVM_CONFIG"] = llvm_config



#-------------------------------------------------------------------------------
# Settings for building the extension
#-------------------------------------------------------------------------------
extra_compile_args = ["-std=gnu++11", "-stdlib=libc++", "-x", "c++"]

# Include path to C++ header files
extra_compile_args += ["-I" + os.environ["LLVM4"] + "/include/c++/v1",
                       "-isystem " + os.environ["LLVM4"] + "/include/c++/v1"]

# This flag becomes C-level macro DTPY, which indicates that we are compiling
# (Py)datatable. This is used for example in fread.c to distinguish between
# Python/R datatable.
extra_compile_args += ["-DDTPY"]

# This macro enables all `assert` statements at the C level. By default they
# are disabled...
extra_compile_args += ["-DNONDEBUG"]

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
extra_compile_args += [
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
extra_link_args = [
    "-v",
]

if sys.platform == "linux":
    extra_link_args += ["-lc++"]

if "DTNOOPENMP" in os.environ:
    extra_compile_args.append("-DDTNOOMP")
else:
    extra_compile_args.insert(0, "-fopenmp")
    extra_link_args.append("-fopenmp")

if "DTCOVERAGE" in os.environ:
    # On linux we need to pass proper flag to clang linker which
    # is not used for some reason at linux
    if sys.platform == "linux":
        os.environ["LDSHARED"] = clang
        extra_link_args += ["-shared"]
    extra_compile_args += ["-g", "--coverage", "-O0"]
    extra_link_args += ["--coverage", "-O0"]

if "DTDEBUG" in os.environ:
    extra_compile_args += ["-ggdb", "-O0"]

if "CI_EXTRA_COMPILE_ARGS" in os.environ:
    extra_compile_args += [os.environ["CI_EXTRA_COMPILE_ARGS"]]


# Test dependencies exposed as extras, based on:
# https://stackoverflow.com/questions/29870629/pip-install-test-dependencies-for-tox-from-setup-py
test_deps = [
    "pandas",
    "pytest>=3.1",
    "pytest-cov",
    "pytest-benchmark>=3.1",
    "pytest-ordering>=0.5",
]


#-------------------------------------------------------------------------------
# Main setup
#-------------------------------------------------------------------------------
setup(
    name="datatable",
    version=version,

    description="Python library for fast multi-threaded data manipulation and "
                "munging.",

    # The homepage
    url="https://github.com/h2oai/datatable.git",

    # Author details
    author="Pasha Stetsenko",
    author_email="pasha@h2o.ai",

    license="Apache v2.0",

    classifiers=[
        "Development Status :: 2 - Pre-Alpha",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: Apache Software License",
        "Programming Language :: Python :: 3.5",
        "Programming Language :: Python :: 3.6",
    ],
    keywords=["datatable", "data", "dataframe", "munging", "numpy", "pandas",
              "data processing", "ETL"],

    packages=packages,

    # Runtime dependencies
    install_requires=[
        "typesentry>=0.2.4",
        "blessed",
        "llvmlite",
        "psutil"
    ],

    tests_require=test_deps,

    extras_require={
        "testing": test_deps
    },

    zip_safe=True,

    ext_modules=[
        Extension(
            "datatable/lib/_datatable",
            include_dirs=["c"],
            sources=c_sources,
            extra_compile_args=extra_compile_args,
            extra_link_args=extra_link_args,
        ),
    ],

    package_dir={"datatable": "datatable"},
    package_data={"datatable": ["lib/*.*"]},
)
