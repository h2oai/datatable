#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
from datatable.lib import core
from .llvm import inject_c_code


#===============================================================================
# Base Evaluation Engine class
#===============================================================================

class EvaluationEngine:
    __slots__ = []


def make_engine(engine, dt):
    if engine == "eager":
        return EagerEvaluationEngine()
    if engine == "llvm":
        return LlvmEvaluationEngine()
    if engine is None:
        if dt.nrows < 0:
            return EagerEvaluationEngine()
        else:
            return LlvmEvaluationEngine()
    raise ValueError("Unknown value for parameter `engine`: %r" % (engine,))



#===============================================================================
# "Eager" Evaluation Engine
#===============================================================================

class EagerEvaluationEngine(EvaluationEngine):
    """
    This engine evaluates all expressions "eagerly", i.e. all operations are
    done sequentially, through predefined C functions. For example,
    `f.A + f.B * 2` will be computed in 2 steps: first, column "B" will be
    multiplied by 2 to produce a new temporary column; then column "A" will be
    added with that temporary column; finally the resulting column will be
    converted into a DataTable and returned.
    """
    __slots__ = []



#===============================================================================
# LLVM Evaluation Engine
#===============================================================================

class LlvmEvaluationEngine(EvaluationEngine):
    """
    This engine evaluates all expressions through LLVM. What this means is that
    in order to evaluate expression such as `f.A + f.B * 2` it writes a C
    program for its computation, then compiles that C program with Clang/LLVM,
    then executes it and returns the DataTable produced by the program.

    This engine is more efficient than "Eager" for large datasets, however it
    requires access to Clang+LLVM runtime.
    """

    def __init__(self):
        super().__init__()
        self._result = None
        self._var_counter = 0
        self._functions = {}
        self._global_declarations = ""
        self._extern_declarations = ""
        self._global_names = set()
        self._exported_functions = []
        self._function_pointers = None
        self._nodes = []


    def get_result(self, n) -> int:
        assert n is not None
        if not self._function_pointers:
            cc = self._gen_module()
            self._function_pointers = \
                inject_c_code(cc, self._exported_functions)
        if isinstance(n, str):
            n = self._exported_functions.index(n)
        assert n < len(self._function_pointers)
        return self._function_pointers[n]


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

    def add_node(self, node):
        self._nodes.append(node)

    def make_variable_name(self, prefix="v"):
        self._var_counter += 1
        return prefix + str(self._var_counter)

    def get_dtvar(self, dt):
        varname = "dt" + str(getattr(dt, "_id"))
        if varname not in self._global_names:
            ptr = dt.internal.datatable_ptr
            self.add_global(varname, "void*", "(void*) %dL" % ptr)
        return varname


    def _gen_module(self):
        for node in self._nodes:
            node.generate_c()
        out = _header
        out += "// Extern declarations\n"
        out += self._extern_declarations
        out += "\n\n"
        out += "// Global variables\n"
        out += self._global_declarations
        out += "\n"
        out += "\n\n\n"
        for fnbody in self._functions.values():
            out += fnbody
            out += "\n\n"
        return out


sz_sint, sz_int, sz_lint, sz_llint, sz_sizet = core.get_integer_sizes()
t16 = ("int" if sz_int == 2 else
       "short int" if sz_sint == 2 else "")
t32 = ("int" if sz_int == 4 else
       "long int" if sz_lint == 4 else "")
t64 = ("int" if sz_int == 8 else
       "long int" if sz_lint == 8 else
       "long long int" if sz_llint == 8 else "")
tsz = (t32 if sz_sizet == 4 else
       t64 if sz_sizet == 8 else "")
if not (t16 and t32 and t64 and tsz):
    raise RuntimeError("Invalid integer sizes: short int(%d), int(%d), "
                       "long int(%d), long long int(%d), size_t(%d)"
                       % (sz_sint, sz_int, sz_lint, sz_llint, sz_sizet))

decl_sizes = "\n".join(["typedef signed char int8_t;",
                        "typedef %s int16_t;" % t16,
                        "typedef %s int32_t;" % t32,
                        "typedef %s int64_t;" % t64,
                        "typedef unsigned char uint8_t;",
                        "typedef unsigned %s uint16_t;" % t16,
                        "typedef unsigned %s uint32_t;" % t32,
                        "typedef unsigned %s uint64_t;" % t64,
                        "typedef unsigned %s size_t;" % tsz])

(ptr_dt_malloc,
 ptr_dt_realloc,
 ptr_dt_free,
 ptr_rowindex_from_filterfn32,
 ptr_dt_column_data,
 ptr_dt_unpack_slicerowindex,
 ptr_dt_unpack_arrayrowindex) = core.get_internal_function_ptrs()


_header = """
/**
 * This code is auto-generated by context.py
 **/
// Integer types
%s
#define NULL ((void*)0)

// External functions
typedef void* (*ptr_0)(size_t);
typedef void* (*ptr_1)(void*, size_t);
typedef void (*ptr_2)(void*);
typedef void* (*ptr_3)(void*, int64_t, int);
typedef void* (*ptr_4)(void*, int64_t);
typedef void (*ptr_5)(void*, int64_t*, int64_t*);
typedef void (*ptr_6)(void*, void**);
static ptr_0 dt_malloc = (ptr_0) %dL;
static ptr_1 dt_realloc = (ptr_1) %dL;
static ptr_2 dt_free = (ptr_2) %dL;
static ptr_3 rowindex_from_filterfn32 = (ptr_3) %dL;
static ptr_4 dt_column_data = (ptr_4) %dL;
static ptr_5 dt_unpack_slicerowindex = (ptr_5) %dL;
static ptr_6 dt_unpack_arrayrowindex = (ptr_6) %dL;

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

""" % (decl_sizes,
       ptr_dt_malloc,
       ptr_dt_realloc,
       ptr_dt_free,
       ptr_rowindex_from_filterfn32,
       ptr_dt_column_data,
       ptr_dt_unpack_slicerowindex,
       ptr_dt_unpack_arrayrowindex)


_externs = {
    "ISNA_F4": "static inline int ISNA_F4(float x) "
               "{ return __builtin_isnan(x); }",
    "ISNA_F8": "static inline int ISNA_F8(double x) "
               "{ return __builtin_isnan(x); }",
    "ISNA_I1": "static inline int ISNA_I1(int8_t x) { return x == NA_I1; }",
    "ISNA_I2": "static inline int ISNA_I2(int16_t x) { return x == NA_I2; }",
    "ISNA_I4": "static inline int ISNA_I4(int32_t x) { return x == NA_I4; }",
    "ISNA_I8": "static inline int ISNA_I8(int64_t x) { return x == NA_I8; }",
    "ISNA_U1": "static inline int ISNA_U1(uint8_t x) { return x == NA_U1; }",
    "ISNA_U2": "static inline int ISNA_U2(uint16_t x) { return x == NA_U2; }",
    "ISNA_U4": "static inline int ISNA_U4(uint32_t x) { return x == NA_U4; }",
}
