#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .node import Node



class IteratorNode(Node):
    """
    Base class for nodes generating C functions that loop through rows of a dt.

    A newly instantiated `IteratorNode` object does not generate useful code by
    itself. Instead, it is more like a "template" class that needs to be
    populated with some substance code.

    The IteratorNode will generate a C function with the following structure:

        int `fnname`(int64_t row0, int64_t row1, `extras`) {
            `preamble`
            for (int64_t i = row0; i < row1; i++) {
                `mainloop`
            }
            `epilogue`
            return 0;
        }

    This C function iterates through rows from `row0` to `row1` performing
    actions specified by the `mainloop`. This function returns 0 if it runs
    successfully, or 1 if there were any errors.
    """

    def __init__(self):
        self._preamble = []
        self._mainloop = []
        self._epilogue = []
        self._fnname = None
        self._extraargs = None
        self._keyvars = {}
        self._var_counter = 0
        self._nrows = 1
        self._context = None



    #---- User interface -------------------------------------------------------

    def addto_preamble(self, expr):
        """Add a new line of code to the 'preamble' section."""
        self._preamble.append(expr)

    def addto_mainloop(self, expr):
        """Add a new line of code to the 'main loop' section."""
        self._mainloop.append(expr)

    def addto_epilogue(self, expr):
        """Add a new line of code to the 'epilogue' section."""
        self._epilogue.append(expr)

    def get_keyvar(self, key):
        return self._keyvars.get(key, None)

    def make_keyvar(self, key=None, prefix="v", exact=False):
        if exact:
            v = prefix
        else:
            self._var_counter += 1
            v = prefix + "_" + str(self._var_counter)
        if key:
            assert key not in self._keyvars
            self._keyvars[key] = v
        return v

    def get_dtvar(self, dt):
        return self._context.get_dtvar(dt)

    def add_extern(self, name):
        self._context.add_extern(name)

    def check_num_rows(self, nrows):
        if self._nrows == 1:
            self._nrows = nrows
        else:
            assert nrows == 1 or nrows == self._nrows

    @property
    def nrows(self):
        return self._nrows


    #---- Node interface -------------------------------------------------------

    def generate_c(self, context):
        if self._fnname is not None:
            return self._fnname
        self._context = context
        self._pregenerate(context)
        args = "int64_t row0, int64_t row1"
        if self._extraargs:
            args += ", " + self._extraargs
        fn = ("static int {func}({args}) {{\n"
              "    {preamble}\n"
              "    for (int64_t i = row0; i < row1; i++) {{\n"
              "        {mainloop}\n"
              "    }}\n"
              "    {epilogue}\n"
              "    return 0;\n"
              "}}\n").format(func=self._fnname,
                             args=args,
                             preamble="\n    ".join(self._preamble),
                             mainloop="\n        ".join(self._mainloop),
                             epilogue="\n    ".join(self._epilogue),
                             )
        context.add_function(self._fnname, fn)
        return self._fnname



    #---- Private/protected ----------------------------------------------------

    def _pregenerate(self, context):
        self._fnname = context.make_variable_name("iterfn")




#===============================================================================

class FilterNode(IteratorNode):

    def __init__(self, expr):
        super().__init__()
        self._filter_expr = expr

    def _pregenerate(self, context):
        self._fnname = context.make_variable_name("filter")
        self._extraargs = "int32_t *out, int32_t *n_outs"
        v = self._filter_expr.value(inode=self)
        self.addto_preamble("int64_t j = 0;")
        self.addto_mainloop("if (%s) {" % v)
        self.addto_mainloop("    out[j++] = i;")
        self.addto_mainloop("}")
        self.addto_epilogue("*n_outs = j;")
