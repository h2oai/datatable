#!/usr/bin/env python
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
"""
Build script for the `datatable` module.

    $ python setup.py bdist_wheel
    $ twine upload dist/*
"""
import os
import sys
import re
import subprocess
import sysconfig
from setuptools import setup, find_packages
from distutils.core import Extension
from sys import stderr
import platform

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
        if name.endswith(".c"):
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
    clang = os.path.join(llvm4, "bin", "clang")
    libs = os.path.join(llvm4, "lib")
    includes = os.path.join(llvm4, "include")
    for f in [llvm_config, clang, libs, includes]:
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

# List of files to append
extra_data_files = []
# Compiler
os.environ["CC"] = clang + " "
if sysconfig.get_config_var("CONFINCLUDEPY"):
    # Marking this directory as "isystem" prevents Clang from issuing warnings
    # for those files
    os.environ["CC"] += "-isystem " + sysconfig.get_config_var("CONFINCLUDEPY")
# Linker flags
os.environ["LDFLAGS"] = "-L%s -Wl,-rpath,%s" % (libs, libs)
if platform.platform().startswith("Darwin"):
    os.environ["LDFLAGS"] = "%s -Wl,-rpath,%s" % (os.environ["LDFLAGS"], "@loader_path/../../../lib")
    extra_data_files = [ ('lib', ["%s/%s" % (libs, "libomp.dylib")])]

# Force to build for a 64-bit platform only
os.environ["ARCHFLAGS"] = "-m64"
# If we need to install llvmlite, this would help
os.environ["LLVM_CONFIG"] = llvm_config


#-------------------------------------------------------------------------------
# Settings for building the extension
#-------------------------------------------------------------------------------
extra_compile_args = ["-std=gnu11"]

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
extra_compile_args += [
    "-Weverything",
    "-Wno-covered-switch-default",
    "-Wno-float-equal",
    "-Wno-gnu-statement-expression",
    "-Wno-switch-enum",
    "-Werror=implicit-function-declaration",
    "-Werror=incompatible-pointer-types",
]
extra_link_args = [
    "-v",
]


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
]


#-------------------------------------------------------------------------------
# Main setup
#-------------------------------------------------------------------------------
setup(
    name="datatable",
    version=version,

    description="Python implementation of R's data.table package",

    # The homepage
    url="https://github.com/h2oai/datatable.git",

    # Author details
    author="Pasha Stetsenko & Matt Dowle",
    author_email="pasha@h2o.ai, mattd@h2o.ai",

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
            "_datatable",
            include_dirs=["c"],
            sources=c_sources,
            extra_compile_args=extra_compile_args,
            extra_link_args=extra_link_args,
        ),
    ],
    data_files=extra_data_files,
)
