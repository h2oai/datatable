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
from . import RandomAttackMethod
from tests_random.utils import assert_equals



class ShallowCopy(RandomAttackMethod):

    def __init__(self, context):
        super().__init__(context)
        context.add_deferred_check(future_frame_check(self.frame))


    def log_to_console(self):
        DT = repr(self.frame)
        print(f"# Scheduled a check of {DT}.copy()")


    def apply_to_dtframe(self):
        pass

    def apply_to_pyframe(self):
        pass



def future_frame_check(frame):
    frame_name = repr(frame)
    frame_deep = frame.df.copy(deep=True)
    frame_shallow = frame.df.copy()
    del frame

    def check():
        print(f"# Checking if {frame_name}.copy() hasn't changed")
        assert_equals(frame_shallow, frame_deep)

    return check
