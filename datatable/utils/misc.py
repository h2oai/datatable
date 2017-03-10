#!/usr/bin/env python
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
from __future__ import division, print_function, unicode_literals

__all__ = ("clamp", "normalize_slice", "plural_form")


def plural_form(n, singular, plural=None):
    if n == 1 or n == -1:
        return "%d %s" % (n, singular)
    else:
        if not plural:
            last_letter = singular[-1]
            prev_letter = singular[-2] if len(singular) >= 2 else ""
            if last_letter == "s" or last_letter == "x":
                plural = singular + "es"
            elif last_letter == "y":
                plural = singular[:-1] + "ies"
            elif last_letter == "f" and prev_letter != "f":
                plural = singular[:-1] + "ves"
            elif last_letter == "h" and prev_letter in "sc":
                plural = singular + "es"
            else:
                plural = singular + "s"
        return "%d %s" % (n, plural)


def clamp(x, lb, ub):
    """Return the value of ``x`` clamped to the range ``[lb, ub]``."""
    return min(max(x, lb), ub)
