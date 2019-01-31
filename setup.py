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
"""
Build script for the `datatable` module.

    $ python setup.py sdist
    $ python setup.py bdist_wheel
    $ twine upload dist/*
"""
import os
import shutil
import sys
from setuptools import setup, find_packages, Extension
from ci.setup_utils import (get_datatable_version, make_git_version_file,
                            get_compiler, get_extra_compile_flags,
                            get_extra_link_args, find_linked_dynamic_libraries,
                            TaskContext, islinux, ismacos, iswindows,
                            monkey_patch_compiler)

print()
cmd = ""
with TaskContext("Start setup.py") as log:
    if len(sys.argv) > 1:
        cmd = sys.argv[1]
    log.info("command = `%s`" % cmd)



#-------------------------------------------------------------------------------
# Generic helpers
#-------------------------------------------------------------------------------

def get_c_sources(folder, include_headers=False):
    """Find all C/C++ source files in the `folder` directory."""
    allowed_extensions = [".c", ".C", ".cc", ".cpp", ".cxx", ".c++"]
    if include_headers:
        allowed_extensions += [".h", ".hpp"]
    sources = []
    for root, _, files in os.walk(folder):
        for name in files:
            ext = os.path.splitext(name)[1]
            if name == "types.cc":
                # Make sure `types.cc` is compiled first, as it has multiple
                # useful static assertions.
                sources.insert(0, os.path.join(root, name))
            elif ext in allowed_extensions:
                sources.append(os.path.join(root, name))
    return sources


def get_py_sources():
    """Find python source directories."""
    packages = find_packages(exclude=["tests", "tests.munging", "temp", "c"])
    return packages


def get_main_dependencies():
    deps = ["typesentry>=0.2.6", "blessed"]
    # If there is an active LLVM installation, then also require the
    # `llvmlite` module.
    # llvmdir, llvmver = get_llvm(True)
    # if llvmdir:
    #     llvmlite_req = (">=0.20.0,<0.21.0" if llvmver == "LLVM4" else
    #                     ">=0.21.0,<0.23.0" if llvmver == "LLVM5" else
    #                     ">=0.23.0,<0.27.0" if llvmver == "LLVM6" else
    #                     ">=0.27.0")
    #     deps += ["llvmlite" + llvmlite_req]
    #     # If we need to install llvmlite, this can help
    #     if not os.environ.get("LLVM_CONFIG"):
    #         os.environ["LLVM_CONFIG"] = \
    #             os.path.join(llvmdir, "bin", "llvm-config")
    return deps


def get_test_dependencies():
    # Test dependencies are exposed as extras, see
    # https://stackoverflow.com/questions/29870629
    return [
        "pytest>=3.1",
        "pytest-cov",
        "pytest-benchmark>=3.1",
    ]




#-------------------------------------------------------------------------------
# Prepare the environment
#-------------------------------------------------------------------------------
cpp_files = []
extra_compile_args = []
extra_link_args = []

if cmd in ("build", "bdist_wheel", "build_ext", "install"):
    with TaskContext("Prepare the environment") as log:
        # Check whether the environment is sane...
        if not(islinux() or ismacos() or iswindows()):
            log.warn("Unknown platform=%s os=%s" % (sys.platform, os.name))

        # Compiler
        os.environ["CC"] = os.environ["CXX"] = get_compiler()

        # Linker
        # On linux we need to pass proper flag to clang linker which
        # is not used for some reason at linux
        if islinux() and os.environ.get("DTCOVERAGE"):
            os.environ["LDSHARED"] = os.environ["CC"]

        if ismacos() and not os.environ.get("MACOSX_DEPLOYMENT_TARGET"):
            os.environ["MACOSX_DEPLOYMENT_TARGET"] = "10.13"

        # Force to build for a 64-bit platform only
        os.environ["ARCHFLAGS"] = "-m64"

        for n in ["CC", "CXX", "LDFLAGS", "ARCHFLAGS", "LLVM_CONFIG",
                  "MACOSX_DEPLOYMENT_TARGET"]:
            log.info("%s = %s" % (n, os.environ.get(n, "")))

    extra_compile_args = get_extra_compile_flags()
    extra_link_args = get_extra_link_args()
    cpp_files = get_c_sources("c")

    with TaskContext("Copy dynamic libraries") as log:
        # Copy system libraries into the datatable/lib folder, so that they can
        # be packaged with the wheel
        libs = find_linked_dynamic_libraries()
        for libpath in libs:
            trgfile = os.path.join("datatable", "lib",
                                   os.path.basename(libpath))
            if os.path.exists(trgfile):
                log.info("File %s already exists, skipped" % trgfile)
            else:
                log.info("Copying %s to %s" % (libpath, trgfile))
                shutil.copy(libpath, trgfile)

    if ismacos():
        monkey_patch_compiler()


# Create the git version file
if cmd in ("build", "sdist", "bdist_wheel", "install"):
    make_git_version_file(True)



#-------------------------------------------------------------------------------
# Main setup
#-------------------------------------------------------------------------------
setup(
    name="datatable",
    version=get_datatable_version(),

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
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Intended Audience :: Science/Research",
        "License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)",
        "Operating System :: MacOS",
        "Operating System :: Unix",
        "Programming Language :: Python :: 3.5",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Topic :: Scientific/Engineering :: Information Analysis",
    ],
    keywords=["datatable", "data", "dataframe", "frame", "data.table",
              "munging", "numpy", "pandas", "data processing", "ETL"],

    packages=get_py_sources(),

    # Runtime dependencies
    install_requires=get_main_dependencies(),

    python_requires=">=3.5",

    tests_require=get_test_dependencies(),

    extras_require={
        "testing": get_test_dependencies()
    },

    zip_safe=True,

    ext_modules=[
        Extension(
            "datatable/lib/_datatable",
            include_dirs=["c", "include"],
            sources=cpp_files,
            extra_compile_args=extra_compile_args,
            extra_link_args=extra_link_args,
            language="c++",
        ),
    ],

    package_dir={"datatable": "datatable"},
    package_data={"datatable": ["lib/*.*", "include/*.h"]},
)
