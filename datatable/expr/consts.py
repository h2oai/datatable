#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
from enum import Enum
from ..types import stype

nas_map = {
    stype.bool8: "NA_I1",
    stype.int8: "NA_I1",
    stype.int16: "NA_I2",
    stype.int32: "NA_I4",
    stype.int64: "NA_I8",
    # "i2r": "NA_I2",
    # "i4r": "NA_I4",
    # "i8r": "NA_I8",
    stype.float32: "NA_F4",
    stype.float64: "NA_F8",
    stype.str32: "NA_I4",
    stype.str64: "NA_I8",
    # "c#s": "NULL",
    # "u1e": "NA_U1",
    # "u2e": "NA_U2",
    # "u4e": "NA_U4",
    # "i8d": "NA_I8",
    # "i4t": "NA_I4",
    # "i4d": "NA_I4",
    # "i2d": "NA_I2",
    stype.obj64: "NULL",
}


ctypes_map = {
    stype.bool8: "int8_t",
    stype.int8: "int8_t",
    stype.int16: "int16_t",
    stype.int32: "int32_t",
    stype.int64: "int64_t",
    # "i2r": "int16_t",
    # "i4r": "int32_t",
    # "i8r": "int64_t",
    stype.float32: "float",
    stype.float64: "double",
    stype.str32: "int32_t",
    stype.str64: "int64_t",
    # "c#s": "char*",
    # "u1e": "uint8_t",
    # "u2e": "uint16_t",
    # "u4e": "uint32_t",
    # "i8d": "int64_t",
    # "i4t": "int32_t",
    # "i4d": "int32_t",
    # "i2d": "int16_t",
    stype.obj64: "void*",
}


class CStats(Enum):
    min = 0
    max = 1
    sum = 2
    mean = 3
    std_dev = 4
    count_na = 5


stype_bool = {stype.bool8}
stype_int = {stype.int8, stype.int16, stype.int32, stype.int64}
stype_float = {stype.float32, stype.float64}
stype_decimal = set()  # {"i2r", "i4r", "i8r"}
stype_real = stype_float | stype_decimal
stype_numerical = stype_int | stype_real


stypes_ladder = [stype.bool8, stype.int8, stype.int16, stype.int32, stype.int64,
                 # "i2r", "i4r", "i8r",
                 stype.float32, stype.float64]


ops_rules = {}

for i, st1 in enumerate(stypes_ladder):
    for st2 in stypes_ladder[i:]:
        for op in ["+", "-", "*"]:
            ops_rules[(op, st1, st2)] = st2
            ops_rules[(op, st2, st1)] = st2
        if st1 in stype_int and st2 in stype_int:
            ops_rules[("%", st1, st2)] = st2
            ops_rules[("%", st2, st1)] = st2
            ops_rules[("//", st1, st2)] = st2
            ops_rules[("//", st2, st1)] = st2

for st1 in stypes_ladder:
    for st2 in stypes_ladder:
        ops_rules[("/", st1, st2)] = stype.float64
        for op in [">", ">=", "<", "<=", "==", "!="]:
            ops_rules[(op, st1, st2)] = stype.bool8

for st in stypes_ladder:
    ops_rules[("mean", st)] = stype.float64
    ops_rules[("sd", st)] = stype.float64
    ops_rules[("sum", st)] = stype.int64
    ops_rules[("count", st)] = stype.int64
    ops_rules[("min", st)] = st
    ops_rules[("max", st)] = st
    ops_rules[("first", st)] = st

ops_rules[("+", stype.bool8, stype.bool8)] = stype.int8
ops_rules[("-", stype.bool8, stype.bool8)] = stype.int8
ops_rules[("*", stype.bool8, stype.bool8)] = stype.bool8
ops_rules[("&", stype.bool8, stype.bool8)] = stype.bool8
ops_rules[("|", stype.bool8, stype.bool8)] = stype.bool8
ops_rules[("/", stype.bool8, stype.bool8)] = None
ops_rules[("//", stype.bool8, stype.bool8)] = None
ops_rules[("%", stype.bool8, stype.bool8)] = None
ops_rules[("first", stype.str32)] = stype.str32
ops_rules[("first", stype.str64)] = stype.str64
ops_rules[("sum", stype.float32)] = stype.float64
ops_rules[("sum", stype.float64)] = stype.float64
ops_rules[("count", stype.str32)] = stype.int64
ops_rules[("count", stype.str64)] = stype.int64

division_ops = {"//", "/", "%"}

unary_ops_rules = {}

for st in stype_bool | stype_int:
    unary_ops_rules[("~", st)] = st

for st in stype_int | stype_float:
    unary_ops_rules[("-", st)] = st
    unary_ops_rules[("+", st)] = st
    unary_ops_rules[("abs", st)] = st
    unary_ops_rules[("exp", st)] = stype.float64

# Synchronize with OpCode in c/expr/unaryop.cc
unary_op_codes = {
    "isna": 1,
    "-": 2,
    "+": 3,
    "~": 4,
    "!": 4,  # same as '~'
    "abs": 5,
    "exp": 6,
}


# Synchronize with reduceop.cc
reduce_opcodes = {
    "mean": 1,
    "min": 2,
    "max": 3,
    "stdev": 4,
    "first": 5,
    "sum": 6,
    "count": 7,
}


baseexpr_opcodes = {
    "col": 1,
    "binop": 2,
    "literal": 3,
    "unop": 4,
}
