#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2021 H2O.ai
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
import datatable as dt
from datatable import Type


#-------------------------------------------------------------------------------
# Test Type class
#-------------------------------------------------------------------------------

def test_type_singletons():
    assert Type.void
    assert Type.bool8
    assert Type.int8
    assert Type.int16
    assert Type.int32
    assert Type.int64
    assert Type.float32
    assert Type.float64
    assert Type.str32
    assert Type.str64
    assert Type.obj64


def test_type_repr():
    assert repr(Type.void) == "Type.void"
    assert repr(Type.bool8) == "Type.bool8"
    assert repr(Type.int8) == "Type.int8"
    assert repr(Type.int16) == "Type.int16"
    assert repr(Type.int32) == "Type.int32"
    assert repr(Type.int64) == "Type.int64"
    assert repr(Type.float32) == "Type.float32"
    assert repr(Type.float64) == "Type.float64"
    assert repr(Type.str32) == "Type.str32"
    assert repr(Type.str64) == "Type.str64"
    assert repr(Type.obj64) == "Type.obj64"


def test_type_names():
    return
    assert Type.void.name == "void"
    assert Type.bool8.name == "bool8"
    assert Type.int8.name == "int8"
    assert Type.int16.name == "int16"
    assert Type.int32.name == "int32"
    assert Type.int64.name == "int64"
    assert Type.float32.name == "float32"
    assert Type.float64.name == "float64"
    assert Type.str32.name == "str32"
    assert Type.str64.name == "str64"
    assert Type.obj64.name == "obj64"



#-------------------------------------------------------------------------------
# Test stype enum
#-------------------------------------------------------------------------------

def test_stype():
    from datatable import stype
    assert stype.void
    assert stype.bool8
    assert stype.int8
    assert stype.int16
    assert stype.int32
    assert stype.int64
    assert stype.float32
    assert stype.float64
    assert stype.str32
    assert stype.str64
    assert stype.obj64
    # When new stypes are added, don't forget to update this test suite
    assert len(stype) == 11


def test_stype_names():
    from datatable import stype
    assert stype.void.name == "void"
    assert stype.bool8.name == "bool8"
    assert stype.int8.name == "int8"
    assert stype.int16.name == "int16"
    assert stype.int32.name == "int32"
    assert stype.int64.name == "int64"
    assert stype.float32.name == "float32"
    assert stype.float64.name == "float64"
    assert stype.str32.name == "str32"
    assert stype.str64.name == "str64"
    assert stype.obj64.name == "obj64"


def test_stype_repr():
    from datatable import stype
    for st in stype:
        assert repr(st) == str(st) == "stype." + st.name


def test_stype_codes():
    from datatable import stype
    assert stype.bool8.code == "b1"
    assert stype.int8.code == "i1"
    assert stype.int16.code == "i2"
    assert stype.int32.code == "i4"
    assert stype.int64.code == "i8"
    assert stype.float32.code == "r4"
    assert stype.float64.code == "r8"
    assert stype.str32.code == "s4"
    assert stype.str64.code == "s8"
    assert stype.obj64.code == "o8"


def test_stype_ctypes():
    from datatable import stype
    import ctypes
    assert stype.void.ctype == None
    assert stype.bool8.ctype == ctypes.c_int8
    assert stype.int8.ctype == ctypes.c_int8
    assert stype.int16.ctype == ctypes.c_int16
    assert stype.int32.ctype == ctypes.c_int32
    assert stype.int64.ctype == ctypes.c_int64
    assert stype.float32.ctype == ctypes.c_float
    assert stype.float64.ctype == ctypes.c_double
    assert stype.str32.ctype == ctypes.c_int32
    assert stype.str64.ctype == ctypes.c_int64
    assert stype.obj64.ctype == ctypes.py_object


def test_stype_struct():
    from datatable import stype
    assert stype.bool8.struct == "b"
    assert stype.int8.struct == "b"
    assert stype.int16.struct == "=h"
    assert stype.int32.struct == "=i"
    assert stype.int64.struct == "=q"
    assert stype.float32.struct == "=f"
    assert stype.float64.struct == "=d"
    assert stype.str32.struct == "=i"
    assert stype.str64.struct == "=q"
    assert stype.obj64.struct == "O"


def test_stype_instantiate():
    from datatable import stype
    for st in stype:
        assert stype(st) is st
        assert stype(st.value) is st
        assert stype(st.name) is st
        assert stype(st.code) is st
    assert stype(bool) is stype.bool8
    assert stype("b1") is stype.bool8
    assert stype("bool") is stype.bool8
    assert stype("boolean") is stype.bool8
    assert stype(int) is stype.int64
    assert stype("int") is stype.int64
    assert stype("integer") is stype.int64
    assert stype("int8") is stype.int8
    assert stype("int16") is stype.int16
    assert stype("int32") is stype.int32
    assert stype("int64") is stype.int64
    assert stype(float) is stype.float64
    assert stype("real") is stype.float64
    assert stype("float") is stype.float64
    assert stype("float32") is stype.float32
    assert stype("float64") is stype.float64
    assert stype(str) is stype.str64
    assert stype("str") is stype.str64
    assert stype("str32") is stype.str32
    assert stype("str64") is stype.str64
    assert stype(object) is stype.obj64
    assert stype("obj") is stype.obj64
    assert stype("object") is stype.obj64


def test_stype_instantiate_from_numpy(numpy):
    from datatable import stype
    assert stype(numpy.dtype("void")) is stype.void
    assert stype(numpy.dtype("bool")) is stype.bool8
    assert stype(numpy.dtype("int8")) is stype.int8
    assert stype(numpy.dtype("int16")) is stype.int16
    assert stype(numpy.dtype("int32")) is stype.int32
    assert stype(numpy.dtype("int64")) is stype.int64
    assert stype(numpy.dtype("float32")) is stype.float32
    assert stype(numpy.dtype("float64")) is stype.float64
    assert stype(numpy.dtype("str")) is stype.str64
    assert stype(numpy.dtype("object")) is stype.obj64


def test_stype_instantiate_bad():
    from datatable import stype
    with pytest.raises(dt.exceptions.ValueError):
        print(stype(-1))
    with pytest.raises(dt.exceptions.ValueError):
        print(stype(0.01))
    with pytest.raises(dt.exceptions.ValueError):
        print(stype(["i", "4"]))
    with pytest.raises(dt.exceptions.ValueError):
        print(stype(1.5))
    with pytest.raises(dt.exceptions.ValueError):
        print(stype(True))



@pytest.mark.parametrize("st", list(dt.stype))
def test_stype_minmax(st):
    from datatable import stype, ltype
    if st in (stype.void, stype.str32, stype.str64, stype.obj64):
        assert st.min is None
        assert st.max is None
    else:
        vtype = float if st.ltype == ltype.real else \
                bool  if st == stype.bool8 else \
                int
        assert isinstance(st.min, vtype)
        assert isinstance(st.max, vtype)
        F = dt.Frame([st.min, st.max], stype=st)
        assert F.stypes == (st,)
        assert F[0, 0] == st.min
        assert F[1, 0] == st.max
        # Check that they can be assigned too:
        F[0, 0] = st.max
        F[1, 0] = st.min
        assert F[0, 0] == st.max
        assert F[1, 0] == st.min
        if vtype is int:
            # Check that cannot store any larger value
            G = dt.Frame([st.min - 1, st.max + 1], stype=st)
            assert G.stypes == (st,)
            assert G[0, 0] != st.min - 1
            assert G[1, 0] != st.max + 1
        if st == stype.float64:
            import sys
            assert st.max == sys.float_info.max



#-------------------------------------------------------------------------------
# Test ltype enum
#-------------------------------------------------------------------------------

def test_ltype():
    from datatable import ltype
    assert ltype.void
    assert ltype.bool
    assert ltype.int
    assert ltype.real
    assert ltype.time
    assert ltype.str
    assert ltype.obj
    # When new stypes are added, don't forget to update this test suite
    assert len(ltype) == 7


def test_ltype_names():
    from datatable import ltype
    assert ltype.void.name is "void"
    assert ltype.bool.name is "bool"
    assert ltype.int.name is "int"
    assert ltype.real.name is "real"
    assert ltype.time.name is "time"
    assert ltype.str.name is "str"
    assert ltype.obj.name is "obj"


def test_ltype_repr():
    from datatable import ltype
    for lt in ltype:
        assert repr(lt) == str(lt) == "ltype." + lt.name


def test_ltype_stypes():
    from datatable import stype, ltype
    assert ltype.void.stypes == [stype.void]
    assert ltype.bool.stypes == [stype.bool8]
    assert set(ltype.int.stypes) == {stype.int8, stype.int16, stype.int32,
                                     stype.int64}
    assert set(ltype.real.stypes) == {stype.float32, stype.float64}
    assert set(ltype.str.stypes) == {stype.str32, stype.str64}
    assert set(ltype.time.stypes) == set()
    assert set(ltype.obj.stypes) == {stype.obj64}
