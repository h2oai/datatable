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
TypeError = _tc.TypeError
ValueError = _tc.ValueError
TypeError.__module__ = "dt"
ValueError.__module__ = "dt"


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
            self._checker = lambda: False


DataTable_t = _LazyClass("datatable", "DataTable")

__all__ = ("typed", "is_type", "U", "TypeError", "ValueError", "DataTable_t")
