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


class DeleteColumnsArray(RandomAttackMethod):

    def __init__(self, context):
        super().__init__(context)
        # datatable supports a shape of n x 0 for non-zero n's, while
        # python doesn't, so we never remove all the columns from a Frame.
        ncols = self.frame.ncols
        nkeys = self.frame.nkeys
        if ncols <= 1:
            self.skipped = True
        else:
            self.array = random_array(ncols - 1, positive=True)
            set_keys = set(range(nkeys))
            set_delcols = set(self.array)
            self.n_keys_to_remove = len(set_keys.intersection(set_delcols))
            if (self.n_keys_to_remove > 0 and self.n_keys_to_remove < nkeys and
                    self.frame.nrows > 0):
                self.raises = ValueError
                self.error_message = "Cannot delete a column that is a part " \
                                     "of a multi-column key"


    def log_to_console(self):
        DT = repr(self.frame)
        print(f"del {DT}[:, {self.array}]")


    def apply_to_dtframe(self):
        DT = self.frame.df
        del DT[:, self.array]


    def apply_to_pyframe(self):
        DF = self.frame
        ncols = len(DF.data)
        new_column_ids = sorted(set(range(ncols)) - set(self.array))
        DF.data = [DF.data[i] for i in new_column_ids]
        DF.names = [DF.names[i] for i in new_column_ids]
        DF.types = [DF.types[i] for i in new_column_ids]
        DF.nkeys -= self.n_keys_to_remove
