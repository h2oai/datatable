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
from tests_random.metaframe import MetaFrame


class OperationsLibrary:
    _instance = None

    @staticmethod
    def random():
        lib = OperationsLibrary._instance
        if lib is None:
            lib = OperationsLibrary()
            OperationsLibrary._instance = lib
        return random.choices(population=lib._methods,
                              cum_weights=lib._weights, k=1)[0]


    def __init__(self):
        from .cbind_self import CbindSelf
        from .change_nrows import ChangeNrows
        from .slice_rows import SliceRows
        from .slice_columns import SliceColumns
        from .rbind_self import RbindSelf
        from .select_rows_array import SelectRowsArray
        assert not OperationsLibrary._instance

        self._methods = [
            CbindSelf,
            ChangeNrows,
            SliceColumns,
            SliceRows,
            RbindSelf,
            SelectRowsArray,
        ]
        self._weights = []
        total_weight = 0
        for meth in self._methods:
            assert meth.weight >= 0
            assert meth.log_to_console
            assert meth.apply_to_dtframe
            assert meth.apply_to_pyframe
            total_weight += meth.weight
            self._weights.append(total_weight)



class EvaluationContext:

    def __init__(self):
        self._frames = []
        self._unchecked = set()


    def get_any_frame(self):
        if len(self._frames) == 0:
            new_frame = MetaFrame.random()
            self._frames.append(new_frame)
        if len(self._frames) == 1:
            frame = self._frames[0]
        else:
            frame = random.choice(self._frames)
        self._unchecked.add(frame)
        return frame


    def check(self):
        for frame in self._unchecked:
            frame.check()
        self._unchecked.clear()


    def check_all(self):
        for frame in self._frames:
            frame.check()
        self._unchecked.clear()
