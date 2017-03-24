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
c_sources = [os.path.join("c", filename)
             for filename in os.listdir("c")
             if filename.endswith(".c")]


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

    packages=find_packages(exclude=["tests*", "docs*", "c*", "temp*"]),

    # Runtime dependencies
    install_requires=["typesentry", "blessed", "llvmlite"],
    tests_require=[
        "pytest>=3.0",
        "pytest-cov",
    ],

    zip_safe=True,

    ext_modules=[
        Extension("_datatable",
                  include_dirs=["c"],
                  sources=c_sources)
    ],
)
