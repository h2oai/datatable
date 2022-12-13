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
import sys
import traceback
import warnings
from .lib._datatable import apply_color



class DtException(Exception):
    def __init__(self, message):
        assert isinstance(message, str)
        self.msg = message

    def __str__(self):
        return "".join(_split_backtick_string(self.msg))

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
class IOWarning(DatatableWarning): pass



#-------------------------------------------------------------------------------
# Custom exception handling
#-------------------------------------------------------------------------------

def _handle_dt_exception(exc_class, exc, tb):
    if not isinstance(exc, DtException):
        return _previous_except_hook(exc_class, exc, tb)

    def dim(line):
        return apply_color("dim", line)

    out = ""

    tbframes = traceback.extract_tb(tb)
    col1 = []
    site_packages_dir = None
    last_code_line = None
    for i, frame in enumerate(tbframes):
        ffile = frame.filename
        if ffile == "<stdin>" and frame.name == "<module>":
            col1.append(None)
            continue
        if "site-packages" in ffile:
            if site_packages_dir is None:
                site_packages_dir = ffile[:ffile.index("site-packages")+14]
            if ffile.startswith(site_packages_dir):
                ffile = "$PY/" + ffile[len(site_packages_dir):]
        else:
            last_code_line = i
        line = "%s:%d in %s" % (ffile, frame.lineno, frame.name)
        col1.append(line)

    if col1[-1]:
        out += dim("Traceback (most recent call last):\n")
        col1len = max(len(line) for line in col1 if line) + 2
        prev_line = None
        prev_count = 0
        for i, line1 in enumerate(col1):
            if not line1: continue
            if line1 == prev_line:
                prev_count += 1
                if prev_count >= 3:
                    continue
            else:
                if prev_count >= 3:
                    out += dim("  ... [previous line repeated %d more times]\n"
                               % (prev_count - 3))
                prev_count = 0
                prev_line = line1
            lineout = ("  " + line1 + " "*(col1len - len(line1)) +
                       tbframes[i].line + "\n")
            if i == last_code_line:
                out += lineout
            else:
                out += dim(lineout)
        if prev_count >= 3:
            out += dim("  ... [previous line repeated %d more times]\n"
                       % (prev_count - 3))
        if site_packages_dir:
            out += dim("  (with $PY = %s)\n" % site_packages_dir)
        out += "\n"

    # Lastly, print the exception name & message
    # Also, make sure that any component surrounded with backticks (`like
    # this`) is emphasized
    out += apply_color("red", exc_class.__name__ + ": ")
    # for i, part in enumerate(re.split(r"`(.*?)`", exc.msg)):
    for i, part in enumerate(_split_backtick_string(exc.msg)):
        out += apply_color("bold" if i % 2 else "bright_red", part)
    print(out, file=sys.stderr)


_previous_except_hook = sys.excepthook
sys.excepthook = _handle_dt_exception


def _split_backtick_string(string):
    r"""
    Helper function for processing an exception message with
    backticks. This function will split the string at the
    backtick characters, while also taking care of unescaping any
    escaped special symbols. For example:

        "abc" -> ["abc"]
        "a`b`c" -> ["a", "b", "c"]
        "`abc`" -> ["", "abc"]
        "`\`a\\bc\``" -> ["", "`a\bc`"]
    """
    out = []
    part = ""
    escape_next = False
    for ch in string:
        if escape_next:
            part += ch
            escape_next = False
        elif ch == '\\':
            escape_next = True
        elif ch == '`':
            out.append(part)
            part = ""
        else:
            part += ch
    if part:
        out.append(part)
    return out



#-------------------------------------------------------------------------------
# Custom warning handling
#-------------------------------------------------------------------------------

def _handle_dt_warning(message, category, filename, lineno, file=None,
                       line=None):
    if not issubclass(category, DatatableWarning):
        return _previous_warnings_hook(message, category, filename, lineno,
                                       file, line)
    print(apply_color("yellow", category.__name__ + ": ") +
          apply_color("grey", str(message)),
          file=sys.stderr)


_previous_warnings_hook = warnings.showwarning
warnings.showwarning = _handle_dt_warning
