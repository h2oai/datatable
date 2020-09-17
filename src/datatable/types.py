#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2020 H2O.ai
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
import ctypes
import enum
from datatable.exceptions import ValueError
from datatable.expr.expr import Expr, OpCodes
from datatable.lib import core

__all__ = ("stype", "ltype")


class typed_list(list):
    __slots__ = ["type"]


@enum.unique
class stype(enum.Enum):
    """
    Enumeration of possible "storage" types of columns in a Frame.

    Each column in a Frame is a vector of values of the same type. We call
    this column's type the "stype". Most stypes correspond to primitive C types,
    such as ``int32_t`` or ``double``. However some stypes (corresponding to
    strings and categoricals) have a more complicated underlying structure.

    Notably, :mod:`datatable` does not support arbitrary structures as
    elements of a Column, so the set of stypes is small.
    """
    bool8 = 1
    int8 = 2
    int16 = 3
    int32 = 4
    int64 = 5
    float32 = 6
    float64 = 7
    str32 = 11
    str64 = 12
    obj64 = 21

    def __repr__(self):
        return str(self)

    def __call__(self, arg):
        return Expr(OpCodes.CAST, (arg,), (self,))

    @property
    def code(self):
        """
        Short string representation of the stype, each code has exactly 2 chars.
        """
        return _stype_2_short[self]

    @property
    def ltype(self):
        """
        :class:`ltype` corresponding to this stype. Several stypes may map to
        the same ltype, whereas each stype is described by exactly one ltype.
        """
        return _stype_2_ltype[self]

    @property
    def ctype(self):
        """
        :mod:`ctypes` class that describes the C-level type of each element
        in a column with this stype.

        For non-fixed-width columns (such as `str32`) this will return the ctype
        of only the fixed-width component of that column. Thus,
        ``stype.str32.ctype == ctypes.c_int32``.
        """
        return _stype_2_ctype[self]

    @property
    def dtype(self):
        """
        ``numpy.dtype`` object that corresponds to this stype.
        """
        if not _numpy_init_attempted:
            _init_numpy_transforms()
        return _stype_2_dtype[self]


    @property
    def struct(self):
        """
        :mod:`struct` format string corresponding to this stype.

        For non-fixed-width columns (such as `str32`) this will return the
        format string of only the fixed-width component of that column. Thus,
        ``stype.str32.struct == '=i'``.
        """
        return _stype_2_struct[self]

    @property
    def min(self):
        """
        The smallest finite value that this stype can represent.
        """
        return _stype_extrema.get(self, (None, None))[0]

    @property
    def max(self):
        """
        The largest finite value that this stype can represent.
        """
        return _stype_extrema.get(self, (None, None))[1]


    def __rtruediv__(self, lhs):
        if isinstance(lhs, list):
            res = typed_list(lhs)
            res.type = self
            return res
        return NotImplemented



#-------------------------------------------------------------------------------

@enum.unique
class ltype(enum.Enum):
    """
    Enumeration of possible "logical" types of a column.

    Logical type is the type stripped away from the details of its physical
    storage. For example, ``ltype.int`` represents an integer. Under the hood,
    this integer can be stored in several "physical" formats: from
    ``stype.int8`` to ``stype.int64``. Thus, there is a one-to-many relationship
    between ltypes and stypes.
    """
    bool = 1
    int = 2
    real = 3
    str = 4
    time = 5
    obj = 7

    def __repr__(self):
        return str(self)

    @property
    def stypes(self):
        """
        List of stypes that represent this ltype.

        Parameters
        ----------
        return: List[stype]
        """
        return [k for k, v in _stype_2_ltype.items() if v == self]



#-------------------------------------------------------------------------------

def ___new___(cls, value):
    # We're re-implementing Enum.__new__() method, which is called by the
    # metaclass' `__call__` (for example `stype(5)` or `stype("int64")`).
    # Also called by pickle.
    if isinstance(value, cls):
        return value
    try:
        if value in cls._value2member_map_ and not isinstance(value, bool):
            return cls._value2member_map_[value]
        if not isinstance(value, int) and not _numpy_init_attempted:
            _init_numpy_transforms()
            if value in cls._value2member_map_:
                return cls._value2member_map_[value]
    except TypeError:
        # `value` is not hasheable -- not valid for our enum. Pass-through
        # and raise the ValueError below.
        pass
    raise ValueError("`%r` does not map to any %s" % (value, cls.__name__))


setattr(stype, "__new__", ___new___)
setattr(ltype, "__new__", ___new___)


_stype_2_short = {
    stype.bool8: "b1",
    stype.int8: "i1",
    stype.int16: "i2",
    stype.int32: "i4",
    stype.int64: "i8",
    stype.float32: "r4",
    stype.float64: "r8",
    stype.str32: "s4",
    stype.str64: "s8",
    stype.obj64: "o8",
}

_stype_2_ltype = {
    stype.bool8: ltype.bool,
    stype.int8: ltype.int,
    stype.int16: ltype.int,
    stype.int32: ltype.int,
    stype.int64: ltype.int,
    stype.float32: ltype.real,
    stype.float64: ltype.real,
    stype.str32: ltype.str,
    stype.str64: ltype.str,
    stype.obj64: ltype.obj,
}

_stype_2_ctype = {
    stype.bool8: ctypes.c_int8,
    stype.int8: ctypes.c_int8,
    stype.int16: ctypes.c_int16,
    stype.int32: ctypes.c_int32,
    stype.int64: ctypes.c_int64,
    stype.float32: ctypes.c_float,
    stype.float64: ctypes.c_double,
    stype.str32: ctypes.c_int32,
    stype.str64: ctypes.c_int64,
    stype.obj64: ctypes.py_object,
}

_stype_extrema = {
    stype.bool8: (False, True),
    stype.int8: (-127, 127),
    stype.int16: (-32767, 32767),
    stype.int32: (-2147483647, 2147483647),
    stype.int64: (-9223372036854775807, 9223372036854775807),
    stype.float32: (float.fromhex("-0x1.fffffep+127"),
                    float.fromhex("0x1.fffffep+127")),
    stype.float64: (float.fromhex("-0x1.fffffffffffffp+1023"),
                    float.fromhex("0x1.fffffffffffffp+1023")),
}

_numpy_init_attempted = False
_stype_2_dtype = {}

def _init_numpy_transforms():
    global _numpy_init_attempted
    global _stype_2_dtype
    _numpy_init_attempted = True
    try:
        import numpy as np
        _stype_2_dtype = {
            stype.bool8: np.dtype("bool"),
            stype.int8: np.dtype("int8"),
            stype.int16: np.dtype("int16"),
            stype.int32: np.dtype("int32"),
            stype.int64: np.dtype("int64"),
            stype.float32: np.dtype("float32"),
            stype.float64: np.dtype("float64"),
            stype.str32: np.dtype("object"),
            stype.str64: np.dtype("object"),
            stype.obj64: np.dtype("object"),
        }
        _init_value2members_from([
            (np.dtype("bool"), stype.bool8),
            (np.dtype("int8"), stype.int8),
            (np.dtype("int16"), stype.int16),
            (np.dtype("int32"), stype.int32),
            (np.dtype("int64"), stype.int64),
            (np.dtype("float32"), stype.float32),
            (np.dtype("float64"), stype.float64),
            (np.dtype("str"), stype.str64),
            (np.dtype("object"), stype.obj64),
        ])
    except ImportError:
        pass


_stype_2_struct = {
    stype.bool8: "b",
    stype.int8: "b",
    stype.int16: "=h",
    stype.int32: "=i",
    stype.int64: "=q",
    stype.float32: "=f",
    stype.float64: "=d",
    stype.str32: "=i",
    stype.str64: "=q",
    stype.obj64: "O",
}


def _additional_stype_members():
    for v in stype:
        yield (v.name, v)
    for st, code in _stype_2_short.items():
        yield (code, st)
    yield (bool, stype.bool8)
    yield ("bool", stype.bool8)
    yield ("boolean", stype.bool8)
    yield (int, stype.int64)
    yield ("int", stype.int64)
    yield ("integer", stype.int64)
    yield (float, stype.float64)
    yield ("float", stype.float64)
    yield ("real", stype.float64)
    yield (str, stype.str64)
    yield ("str", stype.str64)
    yield ("string", stype.str64)
    yield (object, stype.obj64)
    yield ("obj", stype.obj64)
    yield ("object", stype.obj64)
    yield ("object64", stype.obj64)

    # "old"-style stypes
    yield ("i1b", stype.bool8)
    yield ("i1i", stype.int8)
    yield ("i2i", stype.int16)
    yield ("i4i", stype.int32)
    yield ("i8i", stype.int64)
    yield ("f4r", stype.float32)
    yield ("f8r", stype.float64)
    yield ("i4s", stype.str32)
    yield ("i8s", stype.str64)
    yield ("p8p", stype.obj64)



def _init_value2members_from(iterator):
    for k, st in iterator:
        assert isinstance(st, stype)
        stype._value2member_map_[k] = st
        ltype._value2member_map_[k] = st.ltype

_init_value2members_from(_additional_stype_members())

core._register_function(2, stype)
core._register_function(3, ltype)
