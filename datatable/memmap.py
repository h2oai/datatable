#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

import os
import _datatable
from .dt import DataTable

_builtin_open = open


def open(dirname):
    if dirname.startswith("~"):
        dirname = os.path.expanduser(dirname)
    if not os.path.isdir(dirname):
        raise ValueError("%s is not a valid directory" % dirname)

    with _builtin_open(os.path.join(dirname, "_info.pdt")) as inp:
        nrows = int(next(inp))
        columns = []
        colnames = []
        for line in inp:
            stype, colname, f = line.strip().split(" ", 3)
            columns.append(os.path.join(dirname, f))
            colnames.append(colname)

    dt = DataTable(_datatable.dt_from_memmap(columns), colnames=colnames)
    assert dt.nrows == nrows, "Wrong number of rows read: %d" % dt.nrows
    return dt
