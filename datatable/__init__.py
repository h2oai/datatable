#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------
from .__version__ import version as __version__
from .dt_append import rbind, cbind
from .frame import Frame
from .expr import (mean, min, max, sd, isna, sum, count, first, abs, exp,
                   log, log10, f, g)
from .fread import fread, GenericReader, FreadWarning
from .lib._datatable import (
    unique, union, intersect, setdiff, symdiff,
    repeat, by, join, sort
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
except ImportError:
    __git_revision__ = ""


__all__ = ("__version__", "__git_revision__",
           "Frame", "max", "mean", "min", "open", "sd", "sum", "count", "first",
           "isna", "fread", "GenericReader", "save", "stype", "ltype", "f", "g",
           "join", "by", "abs", "exp", "log", "log10",
           "TypeError", "ValueError", "DatatableWarning", "FreadWarning",
           "DataTable", "options",
           "bool8", "int8", "int16", "int32", "int64",
           "float32", "float64", "str32", "str64", "obj64",
           "cbind", "rbind", "repeat", "sort",
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
