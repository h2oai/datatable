#!/usr/bin/env python
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
from __future__ import division, print_function, unicode_literals


def plural_form(n, singular, plural=None):
    if n == 1:
        return "%d %s" % (n, singular)
    else:
        if not plural:
            plural = singular + ("es" if singular[-1] in "sx" else "s")
        return "%d %s" % (n, plural)
