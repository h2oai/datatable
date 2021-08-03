#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2021 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------
import random
from . import RandomAttackMethod



class SetKeyColumns(RandomAttackMethod):

    def __init__(self, context):
        super().__init__(context)
        ncols = self.frame.ncols
        if ncols:
            nkeys = min(int(random.expovariate(1.0)) + 1, ncols)
            self.keys = random.sample(range(0, ncols), nkeys)
            self.names = [self.frame.names[i] for i in self.keys]
            key_data = [self.frame.data[i] for i in self.keys]
            unique_rows = set(zip(*key_data))
            if len(unique_rows) != self.frame.nrows:
                self.raises = ValueError
                self.error_message = \
                    "Cannot set a key: the values are not unique"
        else:
            self.skipped = True


    def log_to_console(self):
        DT = repr(self.frame)
        print(f"{DT}.keys = {self.names}")


    def apply_to_dtframe(self):
        DT = self.frame.df
        DT.key = self.names


    def apply_to_pyframe(self):
        DF = self.frame
        nonkeys = sorted(set(range(DF.ncols)) - set(self.keys))
        new_column_order = self.keys + nonkeys
        if DF.nrows:
            data = list(zip(*DF.data))
            data.sort(key=lambda x: tuple((x[i] is not None, x[i])
                                          for i in self.keys))
            DF.data = list(map(list, zip(*data)))
        DF.types = [DF.types[i] for i in new_column_order]
        DF.names = [DF.names[i] for i in new_column_order]
        DF.data  = [DF.data[i]  for i in new_column_order]
        DF.nkeys = len(self.keys)
