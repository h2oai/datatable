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
from abc import ABC, abstractmethod
import random
from tests_random.metaframe import MetaFrame


class RandomAttackMethod(ABC):
    """
    All individual random attack methods must derive from this class.

    In order to create a new random attack method X, do the following:
      1. Create a new file "x.py" and define `class X` there. The
         class must inherit from `RandomAttackMethod`;
      2. (Optionally) redefine `weight` property of the class, in
         order to make it more or less likely to get selected during
         a random attack.
      3. Implement the constructor and 3 methods defined by this
         abstract class.
      4. Add your new method into the `OperationsLibrary._methods`
         list below.
    """
    weight = 1

    @abstractmethod
    def __init__(self, context: "EvaluationContext"):
        """
        The constructor is the place where all the decisions
        regarding the intended action will take place. For example,
        when selecting a subset of rows, you need to decide which
        frame to operate on, and which rows to extract. These
        decisions shall be made here in the constructor, and stored
        within the object.

        The `context` variable keeps track of all frames that were
        defined in this run, and any other related information. The
        most common use for context is to get a frame to operate on.

        If you set property `.skipped` to True then the method will
        not be executed any further. Otherwise, the attacker will
        invoke `log_to_console()`, `apply_to_dtframe()` and
        `apply_to_pyframe()` in order.
        """
        self.frame = context.get_any_frame()
        self.skipped = False

    @abstractmethod
    def log_to_console(self):
        """
        This method `print()`s to console the operation that it
        intends to perform. It should generally be runnable code,
        mirroring to what you will be doing in `apply_to_dtframe()`.
        """
        raise NotImplementedError

    @abstractmethod
    def apply_to_dtframe(self):
        """
        Apply the scheduled method to the datatable portion of
        `self.frame`, i.e. to `self.frame.df`.
        """
        raise NotImplementedError

    @abstractmethod
    def apply_to_pyframe(self):
        """
        Apply the scheduled method to the pyre-python part of
        `self.frame`, which consists of fields `.data`, `.names`,
        `.types` and `.nkeys`.
        """
        raise NotImplementedError





class OperationsLibrary:
    _instance = None

    @staticmethod
    def random_action(context):
        lib = OperationsLibrary._instance
        if lib is None:
            lib = OperationsLibrary()
            OperationsLibrary._instance = lib
        Cls = random.choices(population=lib._methods,
                             cum_weights=lib._weights, k=1)[0]
        return Cls(context)


    def __init__(self):
        # The import statements are declared here in order to avoid
        # circular dependency among imports.
        from .cbind_self import CbindSelf
        from .change_nrows import ChangeNrows
        from .slice_rows import SliceRows
        from .slice_columns import SliceColumns
        from .rbind_self import RbindSelf
        from .select_rows_array import SelectRowsArray
        assert not OperationsLibrary._instance

        self._weights = []  # List[float]
        self._methods = [   # List[RandomAttackMethod]
            CbindSelf,
            ChangeNrows,
            SliceColumns,
            SliceRows,
            RbindSelf,
            SelectRowsArray,
        ]
        total_weight = 0
        for meth in self._methods:
            assert issubclass(meth, RandomAttackMethod), \
                f"{meth} is not derived from RandomAttackMethod class"
            assert meth.weight >= 0
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
