#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-


nas_map = {
    "i1b": "NA_I8",
    "i1i": "NA_I8",
    "i2i": "NA_I16",
    "i4i": "NA_I32",
    "i8i": "NA_I64",
    "i2r": "NA_I16",
    "i4r": "NA_I32",
    "i8r": "NA_I64",
    "f4r": "NA_F32",
    "f8r": "NA_F64",
    "u4s": "NA_U32",
    "u8s": "NA_U64",
    "c#s": "NULL",
    "u1e": "NA_U8",
    "u2e": "NA_U16",
    "u4e": "NA_U32",
    "i8d": "NA_I64",
    "i8w": "NA_I64",
    "i4t": "NA_I32",
    "i4d": "NA_I32",
    "i2d": "NA_I16",
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
    "u4s": "uint32_t",
    "u8s": "uint64_t",
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
    "i2r": 6,
    "i4r": 7,
    "i8r": 8,
    "f4r": 9,
    "f8r": 10,
    "u4s": 11,
    "u8s": 12,
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


decimal_stypes = {"i2r", "i4r", "i8r"}


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
        for op in [">", ">=", "<", "<=", "==", "!="]:
            ops_rules[(op, st1, st2)] = "i1b"

for st in stypes_ladder:
    ops_rules[("mean", st)] = st if st[-1] == "r" else "f8r"

ops_rules[("+", "i1b", "i1b")] = "i1i"
ops_rules[("-", "i1b", "i1b")] = "i1i"
ops_rules[("*", "i1b", "i1b")] = "i1b"
ops_rules[("/", "i1b", "i1b")] = None
ops_rules[("//", "i1b", "i1b")] = None
ops_rules[("%", "i1b", "i1b")] = None


division_ops = {"//", "/", "%"}
