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
from tests_random.utils import random_array
from . import RandomAttackMethod


class DeleteRowsArray(RandomAttackMethod):

    def __init__(self, context):
        super().__init__(context)
        self.skipped = (self.frame.nrows == 0)
        if not self.skipped:
            self.array = random_array(self.frame.nrows, positive=True)
            self.array = sorted(set(self.array))


    def log_to_console(self):
        DT = repr(self.frame)
        print(f"del {DT}[{self.array}, :]")


    def apply_to_dtframe(self):
        DT = self.frame.df
        del DT[self.array, :]


    def apply_to_pyframe(self):
        DF = self.frame
        nrows_current = len(DF.data[0])
        index = sorted(set(range(nrows_current)) - set(self.array))
        for i in range(DF.ncols):
            col = DF.data[i]
            DF.data[i] = [col[j] for j in index]
