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
import random


class RbindSelf:
    weight = 1

    def __init__(self, context):
        self.frame = context.get_any_frame()
        self.skipped = (self.frame.nkeys > 0) and (self.frame.nrows > 0)
        self.mul = random.randint(1, min(5, 1000 // (1 + self.frame.nrows)) + 1)


    def log_to_console(self):
        DT = repr(self.frame)
        out = f"{DT}.rbind([{DT}] * {self.mul})"
        if self.skipped:
            out = "# SKIPPED: " + out
        print(out)


    def apply_to_dtframe(self):
        if self.skipped:
            msg = "Cannot rbind to a keyed frame"
            with pytest.raises(ValueError, match=msg):
                DT.rbind([DT] * self.mul)
        else:
            DT = self.frame.df
            DT.rbind([DT] * self.mul)


    def apply_to_pyframe(self):
        if not self.skipped:
            DF = self.frame
            for i in range(DF.ncols):
                DF.data[i] *= self.mul + 1
