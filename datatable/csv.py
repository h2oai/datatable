#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import os
import datatable.lib._datatable as _datatable
from datatable.utils.terminal import term
_log_color = term.bright_black

__all__ = ("write_csv", "CsvWriter")



def write_csv(dt, path="", nthreads=0, hex=False, verbose=False):
    """
    Write the DataTable into the provided file in CSV format.

    Parameters
    ----------
    path: str
        Path to the output CSV file that will be created. If the file already
        exists, it will be overwritten. If path is not given, then the DataTable
        will be serialized into a string, and that string will be returned.

    nthreads: int
        How many threads to use for writing. The value of 0 means to use all
        available threads. Negative values mean to use that many threads less
        than the maximum available.

    hex: bool
        If True, then all floating-point values will be printed in hex format
        (equivalent to %a format in C `printf`). This format is around 3 times
        faster to write/read compared to usual decimal representation, so its
        use is recommended if you need maximum speed.

    verbose: bool
        If True, some extra information will be printed to the console, which
        may help to debug the inner workings of the algorithm.
    """
    if path.startswith("~"):
        path = os.path.expanduser(path)
    writer = CsvWriter(dt, path, nthreads, hex, verbose)
    return writer.write()



class CsvWriter(object):

    def __init__(self, datatable, path, nthreads=0, hex=False, verbose=False):
        self.datatable = datatable.internal
        self.column_names = datatable.names
        self.nthreads = nthreads
        self.path = path
        self.hex = hex
        self.verbose = verbose
        self._log_newline = False

    def write(self):
        return _datatable.write_csv(self)

    def _vlog(self, message):
        if self._log_newline:
            print("  ", end="")
        self._log_newline = message.endswith("\n")
        print(_log_color(message), end="", flush=True)
