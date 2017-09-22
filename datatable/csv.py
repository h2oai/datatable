#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import _datatable
import os


def write_csv(dt, path=""):
    """
    Write the DataTable into the provided file in CSV format.
    """
    if path.startswith("~"):
        path = os.path.expanduser(path)
    writer = CsvWriter(dt, path)
    return writer.write()



class CsvWriter(object):

    def __init__(self, datatable, path):
        self.datatable = datatable.internal
        self.column_names = datatable.names
        self.nthreads = 0
        self.path = path

    def write(self):
        return _datatable.write_csv(self)
