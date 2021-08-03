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
from tests_random.utils import random_slice, repr_slice
from . import RandomAttackMethod



class SortColumns(RandomAttackMethod):

    def __init__(self, context):
        super().__init__(context)
        ncols = self.frame.ncols
        if ncols:
            ncols_sort = min(int(random.expovariate(1.0)) + 1, ncols)
            self.cols_to_sort = random.sample(range(0, ncols), ncols_sort)
        else:
            self.skipped = True


    def log_to_console(self):
        DT = repr(self.frame)
        print(f"{DT} = {DT}.sort({self.cols_to_sort})")


    def apply_to_dtframe(self):
        DT = self.frame.df
        self.frame.df = DT.sort(self.cols_to_sort)


    def apply_to_pyframe(self):
        DF = self.frame
        DF.nkeys = 0
        if DF.nrows:
            data = list(zip(*DF.data))
            data.sort(key=lambda x: tuple(
                (x[i] is not None, x[i]) for i in self.cols_to_sort))
            DF.data = list(map(list, zip(*data)))
