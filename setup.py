#!/usr/bin/env python
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
"""
Build script for the `datatable` module.

    $ python setup.py bdist_wheel
    $ twine upload dist/*
"""
from setuptools import find_packages, setup
from datatable.__version__ import version

packages = find_packages(exclude=["tests*", "docs*"])

setup(
    name="datatable",
    version=version,

    description="Python implementation of R's data.table package",

    # The homepage
    url="https://github.com/h2oai/datatable.git",

    # Author details
    author="Matt Dowle",
    author_email="mattd@h2o.ai",

    license="Apache v2.0",

    classifiers=[
        "Development Status :: 2 - Pre-Alpha",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: Apache Software License",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3.5",
    ],
    keywords=["datatable", "data", "dataframe", "pandas"],

    packages=packages,

    # Runtime dependencies
    install_requires=[],
    tests_require=[
        "pytest>=3.0",
        "pytest-cov",
    ],

    # This module doesn't expect to introspect its own source code.
    zip_safe=True,
)
