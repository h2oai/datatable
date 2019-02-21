#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import os
from datatable.lib import core
from datatable.utils.terminal import term

__all__ = ("write_csv", "CsvWriter")



def write_csv(dt, path="", nthreads=0, hex=False, verbose=False, **kwargs):
    """
    Write the Frame into the provided file in CSV format.

    Parameters
    ----------
    dt: Frame
        Frame object to write into CSV.

    path: str
        Path to the output CSV file that will be created. If the file already
        exists, it will be overwritten. If path is not given, then the Frame
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
    writer = CsvWriter(dt, path, nthreads, hex, verbose, **kwargs)
    return writer.write()



class CsvWriter(object):

    def __init__(self, datatable, path, nthreads=None, hex=False, verbose=False,
                 _strategy="auto"):
        self.datatable = datatable.internal
        self.column_names = datatable.names
        self.nthreads = nthreads
        self.path = path
        self.hex = hex
        self.verbose = verbose
        self._strategy = _strategy
        self._log_newline = False

    def write(self):
        return core.write_csv(self)

    def _vlog(self, message):
        if self._log_newline:
            print("  ", end="")
        self._log_newline = message.endswith("\n")
        print(term.color("bright_black", message), end="", flush=True)
