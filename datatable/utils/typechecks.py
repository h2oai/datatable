#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
"""
Utility for checking types at runtime.
"""
import importlib
import typesentry
from typesentry import U

_tc = typesentry.Config()
typed = _tc.typed
is_type = _tc.is_type
TTypeError = _tc.TypeError
TValueError = _tc.ValueError

class TImportError(ImportError):
    """Custom (soft) import error."""
    _handle_ = TTypeError._handle_


TTypeError.__module__ = "dt"
TValueError.__module__ = "dt"
TImportError.__module__ = "dt"
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


DataTable_t = _LazyClass("datatable", "DataTable")
PandasDataFrame_t = _LazyClass("pandas", "DataFrame")

__all__ = ("typed", "is_type", "U", "TTypeError", "TValueError", "TImportError",
           "DataTable_t")
