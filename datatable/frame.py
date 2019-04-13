#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
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
import collections
import datatable
import time
import warnings

from datatable.lib import core
from datatable.options import options

__all__ = ("Frame", )



class Frame(core.Frame):
    """
    Two-dimensional column-oriented table of data. Each column has its own name
    and type. Types may vary across columns (unlike in a Numpy array) but cannot
    vary within each column (unlike in Pandas DataFrame).

    Internally the data is stored as C primitives, and processed using
    multithreaded native C++ code.

    This is a primary data structure for datatable module.
    """

    #---------------------------------------------------------------------------
    # Deprecated
    #---------------------------------------------------------------------------

    def topython(self):
        warnings.warn(
            "Method `Frame.topython()` is deprecated (will be removed in "
            "0.9.0), please use `Frame.to_list()` instead",
            category=FutureWarning)
        return self.to_list()

    def topandas(self):
        warnings.warn(
            "Method `Frame.topandas()` is deprecated (will be removed in "
            "0.9.0), please use `Frame.to_pandas()` instead",
            category=FutureWarning)
        return self.to_pandas()

    def tonumpy(self, stype=None):
        warnings.warn(
            "Method `Frame.tonumpy()` is deprecated (will be removed in "
            "0.9.0), please use `Frame.to_numpy()` instead",
            category=FutureWarning)
        return self.to_numpy(stype)

    def scalar(self):
        warnings.warn(
            "Method `Frame.scalar()` is deprecated (will be removed in "
            "0.10.0), please use `Frame[0, 0]` istead",
            category=FutureWarning)
        return self[0, 0]

    def append(self):
        warnings.warn(
            "Method `Frame.append()` is deprecated (will be removed in "
            "0.10.0), please use `Frame.rbind()` instead",
            category=FutureWarning)

    def __call__(self, rows=None, select=None, verbose=False, timeit=False,
                 groupby=None, join=None, sort=None, engine=None
                 ):
        """DEPRECATED, use DT[i, j, ...] instead."""
        warnings.warn(
            "`DT(rows, select, ...)` is deprecated and will be removed in "
            "version 0.9.0. Please use `DT[i, j, ...]` instead",
            category=FutureWarning)
        time0 = time.time() if timeit else 0
        function = type(lambda: None)
        if isinstance(rows, function):
            rows = rows(datatable.f)
        if isinstance(select, function):
            select = select(datatable.f)

        res = self[rows, select,
                   datatable.join(join),
                   datatable.by(groupby),
                   datatable.sort(sort)]
        if timeit:
            print("Time taken: %d ms" % (1000 * (time.time() - time0)))
        return res

    def save(self, path, format="jay", _strategy="auto"):
        warnings.warn(
            "Method `Frame.save()` is deprecated (will be removed in "
            "0.10.0), please use `Frame.to_jay()` instead",
            category=FutureWarning)
        if format == "jay":
            return self.to_jay(path, _strategy=_strategy)
        elif format == "nff":
            from datatable.nff import save_nff
            return save_nff(self, path, _strategy=_strategy)
        else:
            raise ValueError("Unknown `format` value: %s" % format)




#-------------------------------------------------------------------------------
# Global settings
#-------------------------------------------------------------------------------

core._register_function(7, Frame)
core._install_buffer_hooks(Frame)
