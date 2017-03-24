#!/usr/bin/env python
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import re
import subprocess
from llvmlite import binding

import _datatable as c
from datatable.expr import DataTableExpr

__all__ = ("select_by_expression", )


def select_by_expression(expr):
    assert isinstance(expr, DataTableExpr)
    gen = CodeGenerator(expr)
    cc = gen._make_c_function()
    llvm = fix_clang_llvm(c_to_llvm(cc))
    compile_llvmir(_engine, llvm)
    fn_ptr = _engine.get_function_address(gen._function_name)
    return c.select_with_filter(expr._src._dt, fn_ptr)




def create_execution_engine():
    """
    Create an ExecutionEngine suitable for JIT code generation on
    the host CPU.  The engine is reusable for an arbitrary number of
    modules.
    """
    # Initialization...
    binding.initialize()
    binding.initialize_native_target()
    binding.initialize_native_asmprinter()
    # Create a target machine representing the host
    target = binding.Target.from_default_triple()
    target_machine = target.create_target_machine()
    # And an execution engine with an empty backing module
    backing_mod = binding.parse_assembly("")
    engine = binding.create_mcjit_compiler(backing_mod, target_machine)
    return engine

_engine = create_execution_engine()



def c_to_llvm(code):
    proc = subprocess.Popen(args=["clang", "-x", "c", "-S", "-emit-llvm",
                                  "-o", "-", "-"],
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    out, err = proc.communicate(input=code.encode())
    if err:
        raise RuntimeError("LLVM compilation error:\n" + err.decode())
    return out.decode()


def fix_clang_llvm(llvm):
    return re.sub(r"(load|getelementptr inbounds) (\%?[\w.]+\**)\*",
                  r"\1 \2, \2*", llvm)


def compile_llvmir(engine, code):
    m = binding.parse_assembly(code)
    m.verify()
    engine.add_module(m)
    engine.finalize_object()
    return m



class CodeGenerator(object):
    _counter = 23

    def __init__(self, expr):
        self._root_expr = expr
        self._function_name = "h2o_swrlkdh%d" % CodeGenerator._counter
        CodeGenerator._counter += 359

        # Helper state variables used during code generation
        self._declare_pow = False   # is pow() function used?
        self._make_srcvars = False  # are dt->source variables needed?
        self._index_to_select = "i"
        self._srcindices = expr._src._dt.view_colnumbers
        self._colvars = {}


    def _make_c_function(self):
        res = self._expr_to_c(self._root_expr)
        if self._make_srcvars:
            rowindex_type = self._root_expr._src._dt.rowindex_type
            assert rowindex_type == "slice" or rowindex_type == "array"

        # File headers
        out = ""
        if self._declare_pow:
            out += "double pow(double x, double y);\n"
        out += _datastructures

        # Function body
        out += ("int64 %s(DataTable *dt, int64 *out, int64 rowfr, int64 rowto) "
                "{\n" % self._function_name)
        out += "  int64 *outptr = out;\n"
        out += "  Column *columns = dt->columns;\n"
        if self._make_srcvars:
            out += "  Column *srccolumns = dt->src->columns;\n"
            if rowindex_type == "array":
                out += "  int64 *rowindices = dt->rowmapping->indices;\n"
            else:
                out += "  int64 slice_start = dt->rowmapping->slice.start;\n"
                out += "  int64 slice_step = dt->rowmapping->slice.step;\n"
        for expr in self._colvars.values():
            out += "  %s;\n" % expr.declaration
        out += "  for (int64 i = rowfr; i < rowto; i++) {\n"
        if self._make_srcvars:
            if rowindex_type == "array":
                out += "    int64 j = rowindices[i];\n"
            else:
                out += "    int64 j = slice_start + i * slice_step;\n"
        out += "    if (%s) {\n" % res
        out += "      *outptr++ = %s;\n" % self._index_to_select
        out += "    }\n"
        out += "  }\n"
        out += "  return (outptr - out);\n"
        out += "}\n"
        return out

    def _expr_to_c(self, expr):
        if not isinstance(expr, DataTableExpr):
            return str(expr)
        dag = expr._dag
        head = dag[0]
        if head == "col":
            idx = dag[1]
            assert isinstance(idx, int)
            v = self._colvars.get(idx)
            if not v:
                v = CodeColumn(expr._src, idx, self._srcindices)
                if v.srcindex is not None:
                    self._make_srcvars = True
                self._colvars[idx] = v
            return v.usage
        if head in _ops_to_c:
            lhs = self._expr_to_c(dag[1])
            rhs = self._expr_to_c(dag[2])
            return "(%s %s %s)" % (lhs, _ops_to_c[head], rhs)
        if head == "pow" or head == "rpow":
            lhs = self._expr_to_c(dag[1])
            rhs = self._expr_to_c(dag[2])
            self._declare_pow = True
            return "pow(%s, %s)" % (lhs, rhs)
        raise ValueError("Unknown expression: %r" % expr)


class CodeColumn(object):
    def __init__(self, dt, idx, srcindices):
        self.index = idx
        self.coltype = dt.types[idx]
        self.dtype = _dtypes_map[self.coltype]
        self.srcindex = srcindices[idx] if srcindices else None
        self.varname = "col%ddata" % idx
        datasource = "columns" if self.srcindex is None else "srccolumns"
        self.declaration = ("%s *%s = %s[%d].data"
                            % (self.dtype, self.varname, datasource, idx))
        self.usage = self.varname + ("[i]" if self.srcindex is None else "[j]")


_datastructures = """
enum RowMappingType {RI_ARRAY, RI_SLICE};
enum ColType {DT_AUTO, DT_DOUBLE, DT_LONG, DT_STRING, DT_BOOL, DT_OBJECT};
typedef long long int int64;
typedef struct RowMapping {
    enum RowMappingType type;
    int64 length;
    union {
        int64* indices;
        struct { int64 start, step; } slice;
    };
} RowMapping;
typedef struct Column {
    void* data;
    enum ColType type;
    int64 srcindex;
} Column;
typedef struct DataTable {
  int64 nrows, ncols;
  struct DataTable *source;
  struct RowMapping *rowmapping;
  struct Column *columns;
} DataTable;


"""

_dtypes_map = {
    "int": "int64",
    "real": "double",
    "bool": "char",
    "str": "char*",
    "obj": "PyObject*",
}

_ops_to_c = {
    "and": "&&",     "rand": "&&",
    "or": "||",      "ror": "||",
    "add": "+",      "radd": "+",
    "sub": "-",      "rsub": "-",
    "mul": "*",      "rmul": "*",
    "mod": "%",      "rmod": "%",
    "truediv": "/",  "truediv": "/",    # for now
    "floordiv": "/", "rfloordiv": "/",  # for now
    "lshift": "<<",  "rlshift": "<<",
    "rshift": ">>",  "rrshift": ">>",
    "eq": "==",
    "ne": "!=",
    "lt": "<",
    "gt": ">",
    "le": "<=",
    "ge": ">=",
}
