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
import pytest
import random
from . import RandomAttackMethod


class ChangeNrows(RandomAttackMethod):
    """
    This operation changes the number of rows in a frame: either
    increases or decreases.
    """

    def __init__(self, context):
        super().__init__(context)
        t = random.random()
        self.frame = context.get_any_frame()
        self.current_nrows = self.frame.nrows
        self.new_nrows = int(self.current_nrows * 10 / (19 * t + 1) + 1)
        if (self.frame.nkeys > 0 and
                self.new_nrows > self.current_nrows):
            self.raises = ValueError
            self.error_message = \
                "Cannot increase the number of rows in a keyed frame"


    def log_to_console(self):
        print(f"{self.frame}.nrows = {self.new_nrows}")


    def apply_to_dtframe(self):
        DT = self.frame.df
        DT.nrows = self.new_nrows


    def apply_to_pyframe(self):
        if self.new_nrows > self.current_nrows:
            append = [None] * (self.new_nrows - self.current_nrows)
            for column in self.frame.data:
                column += append
        elif self.current_nrows > self.new_nrows:
            for column in self.frame.data:
                del column[self.new_nrows:]
