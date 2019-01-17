#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
from .__version__ import version as __version__
from .dt_append import rbind, cbind
from .frame import Frame
from .expr import (mean, min, max, sd, isna, sum, count, first, abs, exp,
                   log, log10)
from .fread import fread, GenericReader, FreadWarning
from .graph import f, g, join, by
from .lib._datatable import (
    unique, union, intersect, setdiff, symdiff,
    repeat
)
from .nff import save, open
from .options import options
from .str import split_into_nhot
from .types import stype, ltype
from .utils.typechecks import TTypeError as TypeError
from .utils.typechecks import TValueError as ValueError
from .utils.typechecks import DatatableWarning
try:
    from .__git__ import __git_revision__
except:
    __git_revision__ = ""


__all__ = ("__version__", "__git_revision__",
           "Frame", "max", "mean", "min", "open", "sd", "sum", "count", "first",
           "isna", "fread", "GenericReader", "save", "stype", "ltype", "f", "g",
           "join", "by", "abs", "exp", "log", "log10",
           "TypeError", "ValueError", "DatatableWarning", "FreadWarning",
           "DataTable", "options",
           "bool8", "int8", "int16", "int32", "int64",
           "float32", "float64", "str32", "str64", "obj64",
           "cbind", "rbind", "repeat",
           "unique", "union", "intersect", "setdiff", "symdiff",
           "split_into_nhot")

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
DataTable = Frame


Frame.__module__ = "datatable"
