#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
"""
Utility for checking types at runtime.
"""
import importlib
import typesentry
import warnings
from typesentry import U
from datatable.utils.terminal import term

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


class _LazyClass(typesentry.MagicType):
    def __init__(self, module, symbol):
        self._module = module
        self._symbol = symbol
        self._checker = None

    def name(self):
        if self._module == "datatable":
            return self._symbol
        else:
            return self._module + "." + self._symbol

    def check(self, var):
        if not self._checker:
            self._load_symbol()
        return self._checker(var)

    def _load_symbol(self):
        try:
            mod = importlib.import_module(self._module)
            cls = getattr(mod, self._symbol, False)
            self._checker = lambda x: isinstance(x, cls)
        except ImportError:
            self._checker = lambda x: False


Frame_t = _LazyClass("datatable", "Frame")
PandasDataFrame_t = _LazyClass("pandas", "DataFrame")
PandasSeries_t = _LazyClass("pandas", "Series")
NumpyArray_t = _LazyClass("numpy", "ndarray")
NumpyMaskedArray_t = _LazyClass("numpy.ma", "MaskedArray")



#-------------------------------------------------------------------------------
# Warnings
#-------------------------------------------------------------------------------

class DatatableWarning(UserWarning):
    def _handle_(self):
        name = self.__class__.__name__
        message = str(self)
        print(term.dim_yellow(name + ": ") + term.bright_black(message))


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



__all__ = ("typed", "is_type", "U", "TTypeError", "TValueError", "TImportError",
           "Frame_t", "NumpyArray_t", "DatatableWarning", "dtwarn", "name_type")
