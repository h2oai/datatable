#!/usr/bin/env python
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
"""
Build script for the `datatable` module.

    $ python setup.py bdist_wheel
    $ twine upload dist/*
"""
import os
import re
from setuptools import setup, find_packages
from distutils.core import Extension


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


# Find all C source files in the "c/" directory
c_sources = []
for root, dirs, files in os.walk("c"):
    for name in files:
        if name.endswith(".c"):
            c_sources.append(os.path.join(root, name))

# Find python source directories
packages = find_packages(exclude=["tests", "temp", "c"])
print("\nFound packages: %r\n" % packages)


if "CC" in os.environ:
    os.environ["CC"] += " -fopenmp"

# Force to build for a 64-bit platform only
os.environ["ARCHFLAGS"] = "-m64"


# Main setup
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
        "Programming Language :: Python :: 3.6",
    ],
    keywords=["datatable", "data", "dataframe", "munging", "numpy", "pandas"],

    packages=packages,

    # Runtime dependencies
    install_requires=["typesentry", "blessed", "llvmlite"],
    tests_require=[
        "pytest>=3.0",
        "pytest-cov",
    ],

    zip_safe=True,

    ext_modules=[
        Extension(
            "_datatable",
            include_dirs=["c"],
            sources=c_sources,
            # Ignored warnings:
            #   -Wreserved-id-macro : triggers for python internals
            #   -Wpadded: warning about gaps in a struct, which are normal
            #   -Wunused-parameter: standard PyCFunction takes 2 params,
            #       even if one of them is NULL
            #   -Wpointer-arith: this warns about treating (void*) as
            #       (char*), which is a GNU extension. However since we
            #       only ever want to compile in GCC / CLang, such use is
            #       appropriate.
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
            extra_compile_args=[
                "-Weverything",
                "-Wno-reserved-id-macro",
                "-Wno-padded",
                "-Wno-unused-parameter",
                "-Wno-pointer-arith",
                "-Wno-covered-switch-default",
                "-Wno-float-equal",
                "-Wno-gnu-statement-expression",
                "-Wno-switch-enum",
                "-Werror=implicit-function-declaration",
                "-Werror=incompatible-pointer-types",
                "-fopenmp",
                "-std=gnu11",
                # "-ggdb", "-O0",
            ],
            extra_link_args=[
                "-v",
                "-fopenmp",
            ],
        ),
    ],
)
