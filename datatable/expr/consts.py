#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
from enum import Enum


nas_map = {
    "i1b": "NA_I1",
    "i1i": "NA_I1",
    "i2i": "NA_I2",
    "i4i": "NA_I4",
    "i8i": "NA_I8",
    "i2r": "NA_I2",
    "i4r": "NA_I4",
    "i8r": "NA_I8",
    "f4r": "NA_F4",
    "f8r": "NA_F8",
    "i4s": "NA_I4",
    "i8s": "NA_I8",
    "c#s": "NULL",
    "u1e": "NA_U1",
    "u2e": "NA_U2",
    "u4e": "NA_U4",
    "i8d": "NA_I8",
    "i8w": "NA_I8",
    "i4t": "NA_I4",
    "i4d": "NA_I4",
    "i2d": "NA_I2",
    "p8p": "NULL",
}


ctypes_map = {
    "i1b": "int8_t",
    "i1i": "int8_t",
    "i2i": "int16_t",
    "i4i": "int32_t",
    "i8i": "int64_t",
    "i2r": "int16_t",
    "i4r": "int32_t",
    "i8r": "int64_t",
    "f4r": "float",
    "f8r": "double",
    "i4s": "int32_t",
    "i8s": "int64_t",
    "c#s": "char*",
    "u1e": "uint8_t",
    "u2e": "uint16_t",
    "u4e": "uint32_t",
    "i8d": "int64_t",
    "i8w": "int64_t",
    "i4t": "int32_t",
    "i4d": "int32_t",
    "i2d": "int16_t",
    "p8p": "void*",
}


itypes_map = {
    "i1b": 1,
    "i1i": 2,
    "i2i": 3,
    "i4i": 4,
    "i8i": 5,
    "f4r": 6,
    "f8r": 7,
    "i2r": 8,
    "i4r": 9,
    "i8r": 10,
    "i4s": 11,
    "i8s": 12,
    "c#s": 13,
    "u1e": 14,
    "u2e": 15,
    "u4e": 16,
    "i8d": 17,
    "i8w": 18,
    "i4t": 19,
    "i4d": 20,
    "i2d": 21,
    "p8p": 22,
}


class CStats(Enum):
    min = 0
    max = 1
    sum = 2
    mean = 3
    std_dev = 4
    count_na = 5


stype_bool = {"i1b"}
stype_int = {"i1i", "i2i", "i4i", "i8i"}
stype_float = {"f4r", "f8r"}
stype_decimal = {"i2r", "i4r", "i8r"}
stype_real = stype_float | stype_decimal
stype_numerical = stype_int | stype_real


stypes_ladder = ["i1b", "i1i", "i2i", "i4i", "i8i",
                 "i2r", "i4r", "i8r", "f4r", "f8r"]


ops_rules = {}

for i, st1 in enumerate(stypes_ladder):
    for st2 in stypes_ladder[i:]:
        for op in ["+", "-", "*", "&&", "||"]:
            ops_rules[(op, st1, st2)] = st2
            ops_rules[(op, st2, st1)] = st2

for st1 in stypes_ladder:
    for st2 in stypes_ladder:
        ops_rules[("/", st1, st2)] = "f8r"
        for op in [">", ">=", "<", "<=", "==", "!="]:
            ops_rules[(op, st1, st2)] = "i1b"

for st in stypes_ladder:
    ops_rules[("mean", st)] = st if st[-1] == "r" else "f8r"
    ops_rules[("sd", st)] = st if st[-1] == "r" else "f8r"

ops_rules[("+", "i1b", "i1b")] = "i1i"
ops_rules[("-", "i1b", "i1b")] = "i1i"
ops_rules[("*", "i1b", "i1b")] = "i1b"
ops_rules[("/", "i1b", "i1b")] = None
ops_rules[("//", "i1b", "i1b")] = None
ops_rules[("%", "i1b", "i1b")] = None


division_ops = {"//", "/", "%"}

unary_ops_rules = {}

for st in stype_bool | stype_int:
    unary_ops_rules[("~", st)] = st

for st in stype_int | stype_float:
    unary_ops_rules[("-", st)] = st
    unary_ops_rules[("+", st)] = st
