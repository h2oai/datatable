#!/usr/bin/env python
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
"""
Utility for checking types at runtime.
"""
import typesentry
from typesentry import checker_for_type, U

_tc = typesentry.Config()
typed = _tc.typed
is_type = _tc.is_type


__all__ = ("typed", "checker_for_type", "U")
