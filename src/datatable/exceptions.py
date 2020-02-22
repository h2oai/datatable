#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2020 H2O.ai
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
import builtins
import os
import re
import sys
import traceback
import warnings
from .lib._datatable import apply_color



class DtException(Exception):
    def __init__(self, message):
        assert isinstance(message, str)
        self.msg = message

    def __str__(self):
        return self.msg.replace('`', '')

    def __repr__(self):
        return self.__class__.__name__ + '(' + repr(str(self)) + ')'


class ImportError(DtException, builtins.ImportError): pass
class IndexError(DtException, builtins.IndexError): pass
class InvalidOperationError(DtException): pass
class IOError(DtException, builtins.IOError): pass
class KeyError(DtException, builtins.KeyError): pass
class MemoryError(DtException, builtins.MemoryError): pass
class NotImplementedError(DtException, builtins.NotImplementedError): pass
class OverflowError(DtException, builtins.OverflowError): pass
class TypeError(DtException, builtins.TypeError): pass
class ValueError(DtException, builtins.ValueError): pass


class DatatableWarning(DtException, UserWarning): pass
class FreadWarning(DatatableWarning): pass



#-------------------------------------------------------------------------------
# Custom exception handling
#-------------------------------------------------------------------------------

def _handle_dt_exception(exc_class, exc, tb):
    if not isinstance(exc, DtException):
        return _previous_except_hook(exc_class, exc, tb)

    # First, print the exception name
    out = apply_color("red", exc_class.__name__ + ": ")
    # Then the exception message, however make sure that any component
    # surrounded with backticks (`like this`) is emphasized
    for i, part in enumerate(re.split(r"`(.*?)`", exc.msg)):
        out += apply_color("bright_white" if i % 2 else "bright_red", part)
    out += "\n"

    tbframes = traceback.extract_tb(tb)
    filename_prefix = os.path.commonprefix([frame.filename
                                            for frame in tbframes
                                            if frame.filename[:1] != "<"])
    if os.path.isfile(filename_prefix):
        filename_prefix = os.path.dirname(filename_prefix)
    col1 = []
    col2 = []
    for frame in tbframes:
        if frame.filename == "<stdin>" and frame.name == "<module>":
            continue
        if frame.filename[:1] == '<':
            relname = frame.filename
        else:
            relname = os.path.relpath(frame.filename, filename_prefix)
        line = relname + ':' + str(frame.lineno) + ' in ' + frame.name
        col1.append(line)
        col2.append(frame.line)
    if col1:
        tbout = ""
        col1len = max(len(line) for line in col1) + 2
        for line1, line2 in zip(col1, col2):
            tbout += "    " + line1
            tbout += " "*(col1len - len(line1))
            tbout += line2 + "\n"
        tbout += "    . = %s\n" % filename_prefix
        out += apply_color("dim", tbout)
    print(out, file=sys.stderr)


_previous_except_hook = sys.excepthook
sys.excepthook = _handle_dt_exception



#-------------------------------------------------------------------------------
# Custom warning handling
#-------------------------------------------------------------------------------

def _handle_dt_warning(message, category, filename, lineno, file=None,
                       line=None):
    if not isinstance(category, DatatableWarning):
        return _previous_warnings_hook(message, category, filename, lineno,
                                       file, line)
    print(apply_color("yellow", category.__class__.__name__ + ": ") +
          apply_color("grey", message),
          file=sys.stderr)


_previous_warnings_hoook = warnings.showwarning
warnings.showwarning = _handle_dt_warning


# tmp
# from .lib import core
# core._register_function(4, TypeError)
# core._register_function(5, ValueError)
