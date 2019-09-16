#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
"""
Utility for checking types at runtime.
"""
import typesentry
import warnings
import sys
from typesentry import U
from datatable.utils.terminal import term
from datatable.lib import core

_tc = typesentry.Config()
typed = _tc.typed
is_type = _tc.is_type
name_type = _tc.name_type
TTypeError = _tc.TypeError
TValueError = _tc.ValueError

class TImportError(ImportError):
    """Custom (soft) import error."""
    _handle_ = TTypeError._handle_


TTypeError.__module__ = "datatable"
TValueError.__module__ = "datatable"
TImportError.__module__ = "datatable"
TTypeError.__qualname__ = "TypeError"
TValueError.__qualname__ = "ValueError"
TImportError.__qualname__ = "ImportError"
TImportError.__name__ = "ImportError"



#-------------------------------------------------------------------------------
# Warnings
#-------------------------------------------------------------------------------

class DatatableWarning(UserWarning):
    def _handle_(self, *exc_args):
        if exc_args:
            # Warning was converted into an exception
            TTypeError._handle_(self, *exc_args)
        else:
            print(term.color("yellow", self.__class__.__name__ + ": ") +
                  term.color("bright_black", str(self)))


def dtwarn(message):
    warnings.warn(message, category=DatatableWarning)


def _showwarning(message, category, filename, lineno, file=None, line=None):
    custom_handler = getattr(category, "_handle_", None)
    if custom_handler:
        custom_handler(message)
    else:
        _default_warnings_hoook(message, category, filename, lineno, file, line)


# Replace the default warnings handler
_default_warnings_hoook = warnings.showwarning
warnings.showwarning = _showwarning

core._register_function(4, TTypeError)
core._register_function(5, TValueError)
core._register_function(6, DatatableWarning)


__all__ = ("typed", "is_type", "U", "TTypeError", "TValueError", "TImportError",
           "DatatableWarning", "dtwarn", "name_type")
