#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .__version__ import version as __version__
from .dt import DataTable
from .fread import fread, FReader
from .memmap import open
from .nff import save
from .expr import mean, min, max, sd, isna

__all__ = ("__version__", "DataTable", "max", "mean", "min", "open", "sd",
           "isna", "fread", "FReader", "save")


DataTable.__module__ = "datatable"
