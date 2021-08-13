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


def test_type_cmp():
    assert Type.int8 == Type.int8
    assert Type.int8 != Type.int32
    assert not(Type.int8 == Type.int32)
    assert not(Type.int32 == Type.float32)
    assert not(Type.void == Type.obj64)


def test_type_create_from_strings():
    assert Type("V") == Type.void
    assert Type("bool") == Type.bool8
    assert Type("boolean") == Type.bool8
    assert Type("int") == Type.int64
    assert Type("integer") == Type.int64
    assert Type("float") == Type.float32
    assert Type("double") == Type.float64
    assert Type("<U") == Type.str32
    assert Type("str") == Type.str32
    assert Type("string") == Type.str32
    assert Type("obj") == Type.obj64
    assert Type("object") == Type.obj64


def test_type_create_from_names():
    assert Type("void") == Type.void
    assert Type("bool8") == Type.bool8
    assert Type("int8") == Type.int8
    assert Type("int16") == Type.int16
    assert Type("int32") == Type.int32
    assert Type("int64") == Type.int64
    assert Type("float32") == Type.float32
    assert Type("float64") == Type.float64
    assert Type("str32") == Type.str32
    assert Type("str64") == Type.str64
    assert Type("obj64") == Type.obj64


def test_type_create_from_stypes():
    assert Type(dt.stype.void) == Type.void
    assert Type(dt.stype.bool8) == Type.bool8
    assert Type(dt.stype.int8) == Type.int8
    assert Type(dt.stype.int16) == Type.int16
    assert Type(dt.stype.int32) == Type.int32
    assert Type(dt.stype.int64) == Type.int64
    assert Type(dt.stype.float32) == Type.float32
    assert Type(dt.stype.float64) == Type.float64
    assert Type(dt.stype.str32) == Type.str32
    assert Type(dt.stype.str64) == Type.str64
    assert Type(dt.stype.obj64) == Type.obj64


def test_type_create_from_types():
    assert Type(Type.void) == Type.void
    assert Type(Type.bool8) == Type.bool8
    assert Type(Type.int8) == Type.int8
    assert Type(Type.int16) == Type.int16
    assert Type(Type.int32) == Type.int32
    assert Type(Type.int64) == Type.int64
    assert Type(Type.float32) == Type.float32
    assert Type(Type.float64) == Type.float64
    assert Type(Type.str32) == Type.str32
    assert Type(Type.str64) == Type.str64
    assert Type(Type.obj64) == Type.obj64


def test_type_create_from_python_types():
    assert Type(None) == Type.void
    assert Type(bool) == Type.bool8
    assert Type(int) == Type.int64
    assert Type(float) == Type.float64
    assert Type(str) == Type.str32
    assert Type(object) == Type.obj64


def test_type_create_from_numpy_classes(np):
    assert Type(np.void) == Type.void
    assert Type(np.bool_) == Type.bool8
    assert Type(np.int8) == Type.int8
    assert Type(np.int16) == Type.int16
    assert Type(np.int32) == Type.int32
    assert Type(np.int64) == Type.int64
    assert Type(np.float16) == Type.float32
    assert Type(np.float32) == Type.float32
    assert Type(np.float64) == Type.float64
    assert Type(np.str_) == Type.str32


def test_type_create_from_numpy_dtypes(np):
    assert Type(np.dtype("void")) == Type.void
    assert Type(np.dtype("bool")) == Type.bool8
    assert Type(np.dtype("int8")) == Type.int8
    assert Type(np.dtype("int16")) == Type.int16
    assert Type(np.dtype("int32")) == Type.int32
    assert Type(np.dtype("int64")) == Type.int64
    assert Type(np.dtype("float16")) == Type.float32
    assert Type(np.dtype("float32")) == Type.float32
    assert Type(np.dtype("float64")) == Type.float64
    assert Type(np.dtype("str")) == Type.str32


def test_type_create_invalid():
    msg = "Cannot create Type object from"
    with pytest.raises(ValueError, match=msg):
        Type(0)
    with pytest.raises(ValueError, match=msg):
        Type(0.5)
    with pytest.raises(ValueError, match=msg):
        Type("nothing")
    with pytest.raises(ValueError, match=msg):
        Type(type)
    with pytest.raises(TypeError):
        Type()


def test_type_hashable():
    m = {dt.Type.int32: 'ok', dt.Type.str64: 'yep'}
    assert dt.Type.int32 in m
    assert dt.Type('int32') in m
    assert dt.Type.str64 in m
    assert dt.Type('str64') in m
    assert dt.Type.int64 not in m


def test_type_minmax():
    assert dt.Type.bool8.min is False
    assert dt.Type.bool8.max is True
    assert dt.Type.int8.min == -127
    assert dt.Type.int8.max == 127
    assert dt.Type.int16.min == -32767
    assert dt.Type.int16.max == 32767
    assert dt.Type.int32.min == -(2**31 - 1)
    assert dt.Type.int32.max == +(2**31 - 1)
    assert dt.Type.int64.min == -(2**63 - 1)
    assert dt.Type.int64.max == +(2**63 - 1)
    assert dt.Type.float32.min == float.fromhex("-0x1.fffffep+127")
    assert dt.Type.float32.max == float.fromhex("+0x1.fffffep+127")
    assert dt.Type.float64.min == float.fromhex("-0x1.fffffffffffffp+1023")
    assert dt.Type.float64.max == float.fromhex("+0x1.fffffffffffffp+1023")
    assert dt.Type.void.min is None
    assert dt.Type.void.max is None
    assert dt.Type.str32.min is None
    assert dt.Type.str32.max is None
    assert dt.Type.str64.min is None
    assert dt.Type.str64.max is None
    assert dt.Type.obj64.min is None
    assert dt.Type.obj64.max is None


def test_type_isxxx_properties():
    T = dt.Type
    all_types = [T.void, T.bool8, T.int8, T.int16, T.int32, T.int64, T.float32,
                 T.float64, T.str32, T.str64, T.date32, T.time64, T.obj64,
                 T.arr32(T.void), T.arr64(T.str32)]
    all_properties = ["is_array", "is_boolean", "is_compound", "is_float",
                      "is_integer", "is_numeric", "is_object", "is_string",
                      "is_temporal", "is_void"]
    for ttype in all_types:
        for prop in all_properties:
            assert hasattr(ttype, prop)
            assert isinstance(getattr(ttype, prop), bool)
            msg = ("attribute '%s' of 'datatable.Type' objects is not writable"
                   % prop)
            with pytest.raises(AttributeError, match=msg):
                setattr(ttype, prop, False)
            with pytest.raises(AttributeError, match=msg):
                delattr(ttype, prop)




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
    assert stype.arr32
    assert stype.arr64
    assert stype.date32
    assert stype.time64
    assert stype.obj64
    # When new stypes are added, don't forget to update this test suite
    assert len(stype) == 15


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
    assert stype.arr32.name == "arr32"
    assert stype.arr64.name == "arr64"
    assert stype.date32.name == "date32"
    assert stype.time64.name == "time64"
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
    assert stype.void.ctype is None
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
    if st in (stype.void, stype.str32, stype.str64, stype.obj64, stype.time64,
              stype.date32, stype.arr32, stype.arr64):
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
    assert ltype.invalid
    # When new stypes are added, don't forget to update this test suite
    assert len(ltype) == 8


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
    assert set(ltype.time.stypes) == {stype.time64, stype.date32}
    assert set(ltype.obj.stypes) == {stype.obj64}
