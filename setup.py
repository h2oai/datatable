#!/usr/bin/env python
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
"""
Build script for the `datatable` module.

    $ python setup.py bdist_wheel
    $ twine upload dist/*
"""
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


# Main setup
setup(
    name="datatable",
    version=version,

    description="Python implementation of R's data.table package",

    # The homepage
    url="https://github.com/h2oai/datatable.git",

    # Author details
    author="Matt Dowle & Pasha Stetsenko",
    author_email="mattd@h2o.ai, pasha@h2o.ai",

    license="Apache v2.0",

    classifiers=[
        "Development Status :: 2 - Pre-Alpha",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: Apache Software License",
        "Programming Language :: Python :: 3.6",
    ],
    keywords=["datatable", "data", "dataframe", "pandas"],

    packages=find_packages(exclude=["tests*", "docs*"]),

    # Runtime dependencies
    install_requires=["typesentry", "blessed"],
    tests_require=[
        "pytest>=3.0",
        "pytest-cov",
    ],

    zip_safe=False,

    ext_modules=[
        Extension("_datatable",
                  include_dirs=["c"],
                  sources=["c/datatablemodule.c", "c/datatable.c", "c/dtutils.c"])
    ],
)
