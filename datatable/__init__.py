#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .__version__ import version as __version__
from .dt import DataTable
from .fread import fread, FReader
from .memmap import open
from .expr import mean, min, max, sd

__all__ = ("__version__", "DataTable", "max", "mean", "min", "open", "sd",
           "fread", "FReader")


DataTable.__module__ = "datatable"
