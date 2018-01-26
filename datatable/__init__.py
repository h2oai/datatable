#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from datatable.graph.dtproxy import f
from .__version__ import version as __version__
from .dt import DataTable
from .expr import mean, min, max, sd, isna
from .fread import fread, TextReader
from .nff import save, open
from .types import stype, ltype
from .utils.typechecks import TTypeError as TypeError
from .utils.typechecks import TValueError as ValueError

__all__ = ("__version__", "DataTable", "max", "mean", "min", "open", "sd",
           "isna", "fread", "TextReader", "save", "stype", "ltype", "f",
           "TypeError", "ValueError")

DataTable.__module__ = "datatable"
