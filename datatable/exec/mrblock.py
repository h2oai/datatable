#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from datatable.expr.consts import ctypes_map, itypes_map



class MRBlock(object):

    def __init__(self, mod):
        self._module = mod
        self._var_counter = 0
        self._prologue = []
        self._mainloop = []
        self._filtloop = []
        self._looptail = []
        self._epilogue = []
        self._evaluated_exprs = {}
        self._filter = None
        self._dts = []
        self._nrows = -1
        self._name = "mrb%d" % mod.next_fun_counter()
        self._previous_function = None

    @property
    def function_name(self):
        return self._name


    def make_variable(self, *prefixes):
        """
        Create new unique variable name(s).
        """
        i = str(self._module.next_var_counter())
        if not prefixes:
            prefixes = ["v"]
        if len(prefixes) == 1:
            return prefixes[0] + i
        else:
            return tuple(prefix + i for prefix in prefixes)


    def get_dt_columns(self, dtexpr):
        dt = dtexpr.get_datatable()
        key = "%r.columns" % dt
        if key not in self._evaluated_exprs:
            self._check_datatable_compatibility(dt)
            idt = self._module.use_datatable(dt)
            var = "dt%d_columns" % idt
            self._prologue.append("Column **%s = stack[%d].dt->columns;"
                                  % (var, idt))
            self._evaluated_exprs[key] = var
        return self._evaluated_exprs[key]


    def get_dt_srccolumns(self, dtexpr):
        dt = dtexpr.get_datatable()
        key = "%r.srccolumns" % dt
        if key not in self._evaluated_exprs:
            self._check_datatable_compatibility(dt)
            idt = self._module.use_datatable(dt)
            sdt = "stack[%d].dt" % idt
            srccols_var = "dt%d_srccols" % idt
            indx_var = "j%d" % idt
            rowmapping_type = dt.internal.rowmapping_type
            self._prologue.append("Column **%s = %s->source->columns;"
                                  % (srccols_var, sdt))
            if rowmapping_type == "array":
                rowidx_var = "dt%d_rowindx" % idt
                self._prologue.append(
                    "int64_t *%s = %s->rowmapping->indices;"
                    % (rowidx_var, sdt))
                self._mainloop.append(
                    "int64_t %s = %s[i];" % (indx_var, rowidx_var))
            else:
                step_var = "dt%d_srcstep" % idt
                self._prologue.append(
                    "int64_t %s = %s->rowmapping->slice.step;"
                    % (step_var, sdt))
                self._prologue.append(
                    "int64_t %s = %s->rowmapping->slice.start + row0 * %s;"
                    % (indx_var, sdt, step_var))
                self._looptail.append("%s += %s;" % (indx_var, step_var))
            self._evaluated_exprs[key] = (srccols_var, indx_var)
        return self._evaluated_exprs[key]


    def get_keyvar(self, key):
        return self._evaluated_exprs.get(key, None)

    def add_keyexpr(self, key, var):
        self._evaluated_exprs[key] = var


    def add_prologue_expr(self, expr):
        self._prologue.append(expr)


    def add_mainloop_expr(self, expr):
        if self._filtloop:
            self._filtloop.append(expr)
        else:
            self._mainloop.append(expr)


    def add_epilogue_expr(self, expr):
        self._epilogue.append(expr)


    def set_filter(self, expr):
        if self._filter is not None:
            raise ValueError("Attempt to overwrite existing filter %s"
                             % self._filter)
        self._filter = expr


    def add_filter_output(self, idx):
        """
        :param idx: index of the stack variable where the number of rows in the
            filtered result should be written. Position `idx + 1` in the stack
            is reserved for writing out the rowmapping array.
        """
        assert self._filter is not None, "Adding output to unfiltered function"
        assert self._nrows >= 0
        if self._nrows < 1 << 31:
            stype = "i4i"
            alloc = self._nrows * 4
        else:
            stype = "i8i"
            alloc = self._nrows * 8
        ctype = ctypes_map[stype]
        out_var = "out%d" % (idx + 1)
        self._prologue.append("%s *%s = stack[%d].ptr;"
                              % (ctype, out_var, idx + 1))
        self._filtloop.append("%s[cnt] = i;" % (out_var))
        self._epilogue.append("stack[%d].%s = cnt;" % (idx, stype[:2]))
        return alloc


    def generate_c_code(self):
        if self._nrows < 1 << 31:
            i_type = "int32_t"
        else:
            i_type = "int64_t"
        out = ""
        out += "void %s(Value* stack, int64_t row0, int64_t row1)\n" \
               % self._name
        out += "{\n"
        for expr in self._prologue:
            out += "  %s\n" % expr
        out += "\n"
        out += "  %s cnt = 0;\n" % i_type
        out += "  for (%s i = row0; i < row1; i++) {\n" % i_type
        for expr in self._mainloop:
            out += "    %s\n" % expr
        if self._filter:
            out += "    if (%s) {\n" % self._filter
            for expr in self._filtloop:
                out += "      %s\n" % expr
            out += "      cnt++;\n"
            out += "    }\n"
        for expr in self._looptail:
            out += "    %s\n" % expr
        out += "  }\n"
        for expr in self._epilogue:
            out += "  %s\n" % expr
        out += "}\n"
        return out


    def _check_datatable_compatibility(self, dt):
        if self._nrows == -1:
            self._nrows = dt.nrows
        elif self._nrows != dt.nrows:
            raise ValueError("Incompatible datatable %r in the function that "
                             "iterates over %d rows" % (dt, self._nrows))


    @property
    def previous_function(self):
        if self._previous_function is None:
            self._previous_function = MRBlock(self._module)
            self._module.add_mrblock(self._previous_function)
        return self._previous_function


    def add_stack_variable(self, name):
        return self._module.add_stack_variable(name)
