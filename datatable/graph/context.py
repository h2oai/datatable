#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

import _datatable
from .node import Node
from .llvm import inject_c_code


class RequiresCModule:
    """
    Marker class to indicate a Node that needs a CModule for evaluation.
    """
    def use_cmodule(self, cmod):
        pass


# Perhaps this should be moved into the 'exec' folder
class CModuleNode(Node):
    """
    Replacement for :class:`EvaluationModule`.
    """

    def __init__(self):
        super().__init__()
        self._result = None
        self._var_counter = 0
        self._functions = {}
        self._global_declarations = ""
        self._extern_declarations = ""
        self._initializer_declarations = ""
        self._global_names = set()
        self._exported_functions = []
        self._function_pointers = None


    def get_result(self, n):
        assert n is not None
        if not self._function_pointers:
            cc = self._gen_module()
            self._function_pointers = \
                inject_c_code(cc, self._exported_functions)
        assert n < len(self._function_pointers)
        return self._function_pointers[n]


    def execute(self, mainfn, verbose=False):
        cc = self._gen_module()
        ptrs = inject_c_code(cc, [mainfn])
        if verbose:
            print("C code generated:")
            print("-" * 80)
            print(cc)
            print("-" * 80)
        assert len(ptrs) == 1
        return _datatable.exec_function(ptrs[0])


    @property
    def result(self):
        return self._result


    def has_function(self, name):
        return name in self._functions

    def add_function(self, name, body):
        assert name not in self._global_names
        self._functions[name] = body
        self._global_names.add(name)
        if not body.startswith("static"):
            self._exported_functions.append(name)
            return len(self._exported_functions) - 1
        return None

    def add_global(self, name, ctype, initvalue=None):
        assert name not in self._global_names
        if initvalue is None:
            st = "static %s %s;\n" % (ctype, name)
        else:
            st = "static %s %s = %s;\n" % (ctype, name, initvalue)
        self._global_declarations += st
        self._global_names.add(name)

    def add_extern(self, name):
        if name not in self._global_names:
            self._global_names.add(name)
            self._extern_declarations += _externs[name] + "\n"


    def add_initializer(self, expr):
        self._initializer_declarations += "    %s;\n" % expr

    def make_variable_name(self, prefix="v"):
        self._var_counter += 1
        return prefix + str(self._var_counter)

    def get_dtvar(self, dt):
        varname = "dt" + str(dt._id)
        if varname not in self._global_names:
            ptr = dt.internal.datatable_ptr
            self.add_global(varname, "DataTable*", "(DataTable*) %dL" % ptr)
        return varname


    def _gen_module(self):
        out = _header
        out += "// Extern declarations\n"
        out += self._extern_declarations
        out += "\n\n"
        out += "// Global variables\n"
        out += self._global_declarations
        out += "\n"
        out += "static void init(void) {\n"
        out += self._initializer_declarations
        out += "}\n"
        out += "\n\n\n"
        for fnbody in self._functions.values():
            out += fnbody
            out += "\n\n"
        return out


_header = """
/**
 * This code is auto-generated by context.py
 **/
#include <stdint.h>  // intNN_t, etc.
#include <stdlib.h>  // NULL, size_t, malloc, etc.

typedef int RowIndexType;
typedef int8_t SType;
typedef enum MType { MT_DATA=1, MT_MMAP=2 } __attribute__ ((__packed__)) MType;

typedef struct RowIndex {
    RowIndexType type;
    int64_t length;
    int64_t min, max;
    union {
        int32_t *ind32;
        int64_t *ind64;
        struct { int64_t start, step; } slice;
    };
} RowIndex;

typedef struct Column {
    void   *data;
    void   *meta;
    int64_t nrows;
    size_t  alloc_size;
    int     refcount;
    MType   mtype;
    SType   stype;
} Column;

typedef struct DataTable {
  int64_t nrows, ncols;
  struct RowIndex *rowindex;
  struct Column **columns;
} DataTable;

#define BIN_NAF4 0x7F8007A2u
#define BIN_NAF8 0x7FF00000000007A2ull
typedef union { uint64_t i; double d; } double_repr;
typedef union { uint32_t i; float f; } float_repr;
static inline float _nanf_(void) { float_repr x = { BIN_NAF4 }; return x.f; }
static inline double _nand_(void) { double_repr x = { BIN_NAF8 }; return x.d; }

#define NA_I1  (-128)
#define NA_I2  (-32768)
#define NA_I4  (-2147483647-1)
#define NA_I8  (-9223372036854775807-1)
#define NA_U1  255u
#define NA_U2  65535u
#define NA_U4  4294967295u
#define NA_U8  18446744073709551615u
#define NA_F4  _nanf_()
#define NA_F8  _nand_()

"""


_externs = {
    "ISNA_F4": "static int ISNA_F4(float x) "
               "{ float_repr xx; xx.f = x; return xx.i == BIN_NAF4; }",
    "ISNA_F8": "static int ISNA_F8(double x) "
               "{ double_repr xx; xx.d = x; return xx.i == BIN_NAF8; }",
    "ISNA_I1": "static int ISNA_I1(int8_t x) { return x == NA_I1; }",
    "ISNA_I2": "static int ISNA_I2(int16_t x) { return x == NA_I2; }",
    "ISNA_I4": "static int ISNA_I4(int32_t x) { return x == NA_I4; }",
    "ISNA_I8": "static int ISNA_I8(int64_t x) { return x == NA_I8; }",
    "ISNA_U1": "static int ISNA_U1(uint8_t x) { return x == NA_U1; }",
    "ISNA_U2": "static int ISNA_U2(uint16_t x) { return x == NA_U2; }",
    "ISNA_U4": "static int ISNA_U4(uint32_t x) { return x == NA_U4; }",
}
