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
           "TypeError", "ValueError",
           "bool8", "int8", "int16", "int32", "int64",
           "float32", "float64", "str32", "str64", "obj64")

bool8 = stype.bool8
int8 = stype.int8
int16 = stype.int16
int32 = stype.int32
int64 = stype.int64
float32 = stype.float32
float64 = stype.float64
str32 = stype.str32
str64 = stype.str64
obj64 = stype.obj64

DataTable.__module__ = "datatable"
