#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import os
import re

import pytest
from tests import datatable

expr_consts = datatable.expr.consts


#-------------------------------------------------------------------------------
# Prepare fixtures & helper functions
#-------------------------------------------------------------------------------

@pytest.fixture()
def stypes():
    """
    Create a dictionary whose keys are 3-char stype codes (eg. 'i8i' or 'f4r'),
    and values are dictionaries with the following properties:
        * sname (str): the 'ST_*' C name of the enum constant
        * itype (int): integer value of the stype constant
        * stype (str): 3-character string code of the SType
        * ctype (str): C-type of a single element in this column
        * elemsize (int): size in bytes of each element in this column
        * meta (str): name of the C meta class (or empty string)
        * varwidth (bool): is this a variable-width SType?
        * ltype (str): name of the C enum constant with the LType corresponding
          to the current SType
    """
    stypes = {}

    # Load info from types.h file
    file1 = os.path.join(os.path.dirname(__file__), "..", "c", "types.h")
    with open(file1, "r") as f:
        txt1 = f.read()
    mm = re.search(r"typedef enum SType {\s*(.*?),?\s*}", txt1, re.DOTALL)
    assert mm
    txt2 = mm.group(1)
    for name, i in re.findall(r"(\w+)\s*=\s*(\d+)", txt2):
        stypes[name] = {"sname": name, "itype": int(i)}

    # Load info from types.c file
    file2 = os.path.join(os.path.dirname(__file__), "..", "c", "types.c")
    with open(file2, "r") as f:
        txt2 = f.read()
    mm = re.findall(r"STI\((\w+),\s*\"(...)\",\s*(\d+),\s*(?:0|sizeof\((\w+)\))"
                    r",\s*(\d),\s*(?:0|(\w+)),\s*&?(\w+)\)", txt2)
    for name, ctype, elemsize, meta, varwidth, ltype, na in mm:
        stypes[name]["stype"] = ctype
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



#-------------------------------------------------------------------------------
# The tests
#-------------------------------------------------------------------------------

def test_consts_py_nas_map(stypes):
    nas_map = expr_consts.nas_map
    for stype, na in nas_map.items():
        if stype == "p8p" or stype == "c#s":
            assert na == "NULL"
        else:
            assert na == "NA_" + stype[:2].upper()
    assert set(nas_map.keys()) == set(stypes.keys())


def test_consts_py_ctypes_map(stypes):
    ctypes_map = expr_consts.ctypes_map
    for stype, ctype in ctypes_map.items():
        assert stypes[stype]["ctype"] == ctype
    assert set(ctypes_map.keys()) == set(stypes.keys())


def test_consts_py_itypes_map(stypes):
    itypes_map = expr_consts.itypes_map
    for stype, itype in itypes_map.items():
        assert stypes[stype]["itype"] == itype
    assert set(itypes_map.keys()) == set(stypes.keys())

