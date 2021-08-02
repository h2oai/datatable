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
from datatable import f
from tests_random.utils import random_array
from . import RandomAttackMethod


class SelectRowsWithBooleanColumn(RandomAttackMethod):

    def __init__(self, context):
        super().__init__(context)
        bool_columns = [i for i in range(self.frame.ncols)
                        if self.frame.types[i] is bool]
        if self.frame.ncols <= 1 or not bool_columns:
            self.skipped = True
        else:
            self.filter_column = random.choice(bool_columns)


    def log_to_console(self):
        DT = repr(self.frame)
        print(f"{DT} = {DT}[f[{self.filter_column}], :]")


    def apply_to_dtframe(self):
        DT = self.frame.df
        self.frame.df = DT[f[self.filter_column], :]


    def apply_to_pyframe(self):
        DF = self.frame
        icol = self.filter_column
        filter_col = DF.data[icol]
        assert DF.types[icol] is bool
        DF.data = [[value for i, value in enumerate(column) if filter_col[i]]
                   for column in DF.data]
        DF.nkeys = 0
