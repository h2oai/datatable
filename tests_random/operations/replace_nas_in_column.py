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
import re
from  datatable import f
from tests_random.utils import random_string
from . import RandomAttackMethod


class ReplaceNAsInColumn(RandomAttackMethod):

    def __init__(self, context):
        super().__init__(context)
        self.icol = random.randint(0, self.frame.ncols - 1)
        coltype = self.frame.types[self.icol]
        if coltype is bool:
            self.replacement_value = random.choice([True, False])
        elif coltype is int:
            self.replacement_value = random.randint(-100, 100)
        elif coltype is float:
            self.replacement_value = random.random() * 1000
        elif coltype is str:
            self.replacement_value = random_string()
        elif coltype is None:
            self.skipped = True
        else:
            raise RuntimeError(f"Unknown type {coltype}")
        self.raises = (self.icol < self.frame.nkeys)


    def log_to_console(self):
        DT = repr(self.frame)
        repl = repr(self.replacement_value)
        print(f"{DT}[f[{self.icol}] == None, f[{self.icol}]] = {repl}")


    def apply_to_dtframe(self):
        DT = self.frame.df
        icol = self.icol
        if self.raises:
            msg = f'Cannot change values in a key column {self.names[icol]}'
            msg = re.escape(msg)
            with pytest.raises(ValueError, match=msg):
                DT[f[icol] == None, f[icol]] = self.replacement_value
        else:
            DT[f[icol] == None, f[icol]] = self.replacement_value


    def apply_to_pyframe(self):
        if self.raises:
            return
        DF = self.frame
        column = DF.data[self.icol]
        for i, value in enumerate(column):
            if value is None:
                column[i] = self.replacement_value
