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
import copy
import datatable as dt
import random
import warnings
from . import RandomAttackMethod


class CbindSelf(RandomAttackMethod):

    def __init__(self, context):
        super().__init__(context)
        self.mul = random.randint(1, min(5, 100 // (1 + self.frame.ncols)) + 1)
        self.skipped = self.frame.ncols * (1 + self.mul) > 1000


    def log_to_console(self):
        DT = repr(self.frame)
        print(f"{DT}.cbind([{DT}] * {self.mul})")


    def apply_to_dtframe(self):
        DT = self.frame.df
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", dt.exceptions.DatatableWarning)
            DT.cbind([DT] * self.mul)


    def apply_to_pyframe(self):
        DF = self.frame
        newdata = copy.deepcopy(DF.data)
        for i in range(self.mul):
            newdata += copy.deepcopy(DF.data)
        DF.data = newdata
        DF.names *= self.mul + 1
        DF.types *= self.mul + 1
        DF.dedup_names()
