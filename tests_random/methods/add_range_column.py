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
import datatable as dt
from tests_random.utils import random_string
from . import RandomAttackMethod



class AddRangeColumn(RandomAttackMethod):

    def __init__(self, context):
        super().__init__(context)
        start = int(random.expovariate(0.05) - 5)
        step = 0
        while step == 0:
            step = int(1 + random.random() * 3)
        stop = start + step * self.frame.nrows
        self.range = range(start, stop, step)
        self.name = random_string()


    def log_to_console(self):
        DT = repr(self.frame)
        name = repr(self.name)
        print(f"{DT}.cbind(dt.Frame({{{name}: {self.range})}})")


    def apply_to_dtframe(self):
        DT = self.frame.df
        DT.cbind(dt.Frame({self.name: self.range}))


    def apply_to_pyframe(self):
        DF = self.frame
        DF.data += [list(self.range)]
        DF.names += [self.name]
        DF.types += [int]
        DF.dedup_names()
