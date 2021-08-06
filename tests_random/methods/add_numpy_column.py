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
import numpy as np
import random
import datatable as dt
from tests_random.utils import random_type, random_column, random_names
from . import RandomAttackMethod



class AddNumpyColumn(RandomAttackMethod):

    def __init__(self, context):
        super().__init__(context)
        # Numpy has no concept of "void" column (at least, not similar to ours),
        # so avoid that random type:
        coltype = random_type(allow_void=False)
        mfraction = random.random()
        data, mmask = random_column(
            self.frame.nrows, coltype,
            missing_fraction=mfraction, missing_nones=False
        )

        # On Linux/Mac numpy's default int type is np.int64,
        # while on Windows it is np.int32. Here we force it to be
        # np.int64 for consistency.
        np_dtype = np.int64 if coltype == int else np.dtype(coltype)
        np_data = np.ma.array(data, mask=mmask, dtype=np_dtype)

        # Save random numpy arrays to make sure they don't change with
        # munging. Arrays that are not saved here will be eventually deleted
        # by Python, in such a case we also test datatable behaviour.
        if random.random() > 0.5:
            context.add_deferred_check(deferred_nparray_check(np_data))

        for i in range(len(data)):
            if mmask[i]:
                data[i] = None

        self.column_name = random_names(1)[0]
        self.column_type = coltype
        self.np_dtype = np_dtype
        self.np_data = np_data
        self.np_mask = mmask
        self.py_data = data


    def log_to_console(self):
        DT = repr(self.frame)
        colname = repr(self.column_name)
        coldata = repr(self.py_data)
        colmask = repr(self.np_mask)
        dtype = repr(self.np_dtype)
        print(f"nparray = np.ma.array({coldata}, mask={colmask}, dtype={dtype})")
        print(f"{DT}.cbind(nparray, names=[{colname}])")


    def apply_to_dtframe(self):
        DT = self.frame.df
        DT.cbind(dt.Frame(self.np_data.T, names=[self.column_name]))


    def apply_to_pyframe(self):
        DF = self.frame
        DF.data += [self.py_data]
        DF.types += [self.column_type]
        DF.names += [self.column_name]
        DF.dedup_names()



def deferred_nparray_check(array):
    """
    This function will keep a copy-by-reference of the original `array`,
    as well as a deep copy of the same array. When the `check()`
    function will finally run, we will compare if those two are still
    equal (i.e. did datatable change the data it wasn't supposed to?)
    """
    array_copy = copy.deepcopy(array)
    def check():
        print("# Checking if source nparray hasn't changed")
        assert len(array) == len(array_copy)
        assert np.array_equal(array, array_copy)

    return check
