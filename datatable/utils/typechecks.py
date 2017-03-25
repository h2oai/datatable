#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
"""
Utility for checking types at runtime.
"""
import typesentry
from typesentry import U

_tc = typesentry.Config()
typed = _tc.typed
is_type = _tc.is_type
TypeError = _tc.TypeError
ValueError = _tc.ValueError
TypeError.__module__ = "dt"
ValueError.__module__ = "dt"

__all__ = ("typed", "is_type", "U", "TypeError", "ValueError")
