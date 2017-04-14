#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

import os
from .dt import DataTable


def open(dirname):
    if dirname.startswith("~"):
        dirname = os.path.expanduser(dirname)
    if not os.path.isdir(dirname):
        raise ValueError("%s is not a valid directory" % dirname)
    self = DataTable()
    self._fill_from_pdt(dirname)
    return self
