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
      4. Import the "x.py" file at the bottom of this script.
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

        In addition, you can set `self.raises` to an error class
        (such as `ValueError`) and `self.error_message` to the
        corresponding error message. In this case the attacker will
        expect that calling `apply_to_dtframe()` will produce the
        corresponding error with the matching message. The method
        `apply_to_pyframe()` will not be called.
        """
        self.frame = context.get_any_frame()
        self.skipped = False
        self.raises = None
        self.error_message = None

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

    @classmethod
    def __init_subclass__(cls):
        super().__init_subclass__()
        OperationsLibrary.register_attack_method(cls)




class OperationsLibrary:
    _methods = []  # List[float]
    _weights = []  # List[RandomAttackMethod]

    @staticmethod
    def random_action(context):
        Lib = OperationsLibrary
        Cls = random.choices(population=Lib._methods,
                             cum_weights=Lib._weights, k=1)[0]
        return Cls(context)

    @staticmethod
    def register_attack_method(cls):
        assert issubclass(cls, RandomAttackMethod), \
                f"{cls} is not derived from RandomAttackMethod class"
        Lib = OperationsLibrary
        current_total_weight = Lib._weights[-1] if Lib._weights else 0
        Lib._methods.append(cls)
        Lib._weights.append(cls.weight + current_total_weight)

    def __init__(self):
        raise RuntimeError("OperationsLibrary should not be instantiated")



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




#-------------------------------------------------------------------------------
# Individual attack methods
#-------------------------------------------------------------------------------

# These are imported here in order to break circular dependence between
# scripts.
import tests_random.operations.add_range_column
import tests_random.operations.cbind_self
import tests_random.operations.change_nrows
import tests_random.operations.delete_columns_array
import tests_random.operations.delete_rows_array
import tests_random.operations.rbind_self
import tests_random.operations.replace_nas_in_column
import tests_random.operations.select_rows_array
import tests_random.operations.select_rows_with_boolean_column
import tests_random.operations.set_key_columns
import tests_random.operations.slice_columns
import tests_random.operations.slice_rows
import tests_random.operations.sort_columns
