#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import os
import re

import pytest
import datatable
from datatable.utils.typechecks import TValueError

expr_consts = datatable.expr.consts


#-------------------------------------------------------------------------------
# Prepare fixtures & helper functions
#-------------------------------------------------------------------------------

@pytest.fixture()
def c_stypes():
    """
    Create a dictionary whose keys are 3-char old stype codes, and values
    are dictionaries with the following properties:
        * sname (str): the 'ST_*' C name of the enum constant
        * itype (int): integer value of the stype constant
        * stype (str): 3-character string code of the SType
        * code2 (str): 2-character string code of the SType
        * ctype (str): C-type of a single element in this column
        * elemsize (int): size in bytes of each element in this column
        * meta (str): name of the C meta class (or empty string)
        * varwidth (bool): is this a variable-width SType?
        * ltype (str): name of the C enum constant with the LType corresponding
          to the current SType

    This dictionary is made from files "c/types.h" and "c/types.c".
    """
    stypes = {}

    # Load info from types.h file
    file1 = os.path.join(os.path.dirname(__file__), "..", "c", "types.h")
    with open(file1, "r", encoding="utf-8") as f:
        txt1 = f.read()
    mm = re.search(r"typedef enum SType {\s*(.*?),?\s*}", txt1, re.DOTALL)
    assert mm
    txt2 = mm.group(1)
    for name, i in re.findall(r"(\w+)\s*=\s*(\d+)", txt2):
        stypes[name] = {"sname": name, "itype": int(i)}

    # Load info from types.c file
    file2 = os.path.join(os.path.dirname(__file__), "..", "c", "types.c")
    with open(file2, "r", encoding="utf-8") as f:
        txt2 = f.read()
    mm = re.findall(r"STI\((\w+),\s*"
                    r'"(...)",\s*'
                    r'"(..)",\s*'
                    r"(\d+),\s*"
                    r"(?:0|sizeof\((\w+)\)),\s*"
                    r"(\d),\s*"
                    r"(?:0|(\w+)),\s*"
                    r"&?(\w+)\)",
                    txt2)
    for name, code3, code2, elemsize, meta, varwidth, ltype, na in mm:
        stypes[name]["stype"] = code3
        stypes[name]["code2"] = code2
        stypes[name]["elemsize"] = int(elemsize)
        stypes[name]["meta"] = meta
        stypes[name]["varwidth"] = bool(int(varwidth))
        stypes[name]["ltype"] = ltype
        stypes[name]["na"] = na

    # Fill-in ctypes
    for ct in stypes.values():
        stype = ct["stype"]
        ct["ctype"] = "int%d_t" % (int(stype[1]) * 8) if stype[0] == "i" else \
                      "uint%d_t" % (int(stype[1]) * 8) if stype[0] == "u" else \
                      "float" if stype[:2] == "f4" else \
                      "double" if stype[:2] == "f8" else \
                      "void*" if stype == "p8p" else \
                      "char*" if stype == "c#s" else None

    # Re-key to "stype" field
    return {st["stype"]: st for st in stypes.values() if st["stype"] != "---"}


@pytest.fixture()
def c_stypes2(c_stypes):
    """Same as c_stypes, but keyed to 'code2' field."""
    return {v["code2"]: v for v in c_stypes.values()}




#-------------------------------------------------------------------------------
# Test stype enum
#-------------------------------------------------------------------------------

def test_stype():
    from datatable import stype
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
    assert len(stype) == 10


def test_stype_names():
    from datatable import stype
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


def test_stype_values(c_stypes2):
    from datatable import stype
    for st in stype:
        assert st.value == c_stypes2[st.code]["itype"]


def test_stype_sizes(c_stypes2):
    from datatable import stype
    for st in stype:
        assert int(st.code[1:]) == c_stypes2[st.code]["elemsize"]


def test_stype_ctypes():
    from datatable import stype
    import ctypes
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
    with pytest.raises(TValueError):
        print(stype(-1))
    with pytest.raises(TValueError):
        print(stype(0))
    with pytest.raises(TValueError):
        print(stype(["i", "4"]))
    with pytest.raises(TValueError):
        print(stype(1.5))
    with pytest.raises(TValueError):
        print(stype(True))



#-------------------------------------------------------------------------------
# Test ltype enum
#-------------------------------------------------------------------------------

def test_ltype():
    from datatable import ltype
    assert ltype.bool
    assert ltype.int
    assert ltype.real
    assert ltype.time
    assert ltype.str
    assert ltype.obj
    # When new stypes are added, don't forget to update this test suite
    assert len(ltype) == 6


def test_ltype_names():
    from datatable import ltype
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


def test_stype_ltypes(c_stypes2):
    from datatable import stype, ltype
    for st in stype:
        assert st.ltype is ltype(c_stypes2[st.code]["ltype"][3:].lower())


def test_ltype_stypes():
    from datatable import stype, ltype
    assert ltype.bool.stypes == [stype.bool8]
    assert set(ltype.int.stypes) == {stype.int8, stype.int16, stype.int32,
                                     stype.int64}
    assert set(ltype.real.stypes) == {stype.float32, stype.float64}
    assert set(ltype.str.stypes) == {stype.str32, stype.str64}
    assert set(ltype.time.stypes) == set()
    assert set(ltype.obj.stypes) == {stype.obj64}
