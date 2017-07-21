#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import os
import sys
import pytest
sys.path.insert(0, os.path.abspath(".."))

if sys.version_info < (3, 5):
    print("Python 3.5+ is required. Your Python version is:")
    print(sys.version)
    raise RuntimeError

try:
    import datatable  # noqa
except ImportError as e:
    datatable = None
    print("Unable to import module datatable: %s\n" % e)
    print("Python: %s" % sys.version)
    print("Base prefix: %s" % sys.base_prefix)
    print("Executable: %s" % sys.executable)
    print("Paths: %s" % sys.path)
    raise e


#
# Add fixtures here
#
try:
    import pandas
    import numpy
except ImportError:
    pandas = None
    numpy = None

ifpandas = pytest.mark.skipif(pandas is None or numpy is None,
                              reason="Pandas & numpy modules are required")

__all__ = ("datatable", "ifpandas")
