#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------



class IteratorNode(object):
    """
    Base class for nodes generating C functions that loop through rows of a dt.

    A newly instantiated `IteratorNode` object does not generate useful code by
    itself. Instead, it is more like a "template" class that needs to be
    populated with some substance code.

    The IteratorNode will generate a C function with the following structure:

        int `fnname`(int64_t row0, int64_t row1, `extras`) {
            `preamble`
            for (int64_t ii = row0; ii < row1; ii++) {
                int64_t i = /* function of ii */;
                `mainloop`
            }
            `epilogue`
            return 0;
        }

    This C function iterates through rows from `row0` to `row1` performing
    actions specified by the `mainloop`. This function returns 0 if it runs
    successfully, or 1 if there were any errors.

    As the function iterates over the rows of a datatable, two iterator
    variables are defined: `i` and `ii`. The former gives row indices within
    the `Column`s of the datatable, the latter is the row index within the
    datatable. For a regular DataTable, `i` and `ii` are the same; whereas for
    a view DataTable they are different: `ii` is the index within the target
    DataTable, and `i` is the index within the parent.
    """

    def __init__(self, dt, cmod=None, name=None):
        self._dt = dt
        self._preamble = []
        self._mainloop = []
        self._epilogue = []
        self._fnname = name
        if cmod and not name:
            self._fnname = cmod.make_variable_name("iter")
        self._extraargs = None
        self._keyvars = {}
        self._var_counter = 0
        self._nrows = 1
        self._cnode = cmod
        self._fnidx = None

    def get_result(self):
        self._fnidx = self.generate_c()
        return self._cnode.get_result(self._fnidx)

    def use_cmodule(self, cmod):
        self._cnode = cmod


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

    def set_extra_args(self, args):
        self._extraargs = args

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
        return self._cnode.get_dtvar(dt)

    def add_extern(self, name):
        self._cnode.add_extern(name)

    def check_num_rows(self, nrows):
        if self._nrows == 1:
            self._nrows = nrows
        else:
            assert nrows == 1 or nrows == self._nrows

    @property
    def nrows(self):
        return self._nrows

    @property
    def fnname(self):
        return self._fnname


    def generate_c(self):
        args = "int64_t row0, int64_t row1"
        if self._extraargs:
            args += ", " + self._extraargs
        iexpr = "i = ii;"
        if self._dt.internal.isview:
            target = self._cnode.get_dtvar(self._dt)
            ritype = self._dt.internal.rowindex_type
            if ritype == "slice":
                self.addto_preamble("int64_t ristart, ristep;")
                self.addto_preamble("dt_unpack_slicerowindex(%s, "
                                    "&ristart, &ristep);" % target)
                self.addto_preamble("i = ristart - ristep;")
                iexpr = "i += ristep;"
            elif ritype == "arr32":
                self.addto_preamble("int32_t *riarr;")
                self.addto_preamble("dt_unpack_arrayrowindex(%s, "
                                    "(void**) &riarr);" % target)
                iexpr = "i = (int64_t) riarr[ii];"
            else:
                self.addto_preamble("int64_t *riarr;")
                self.addto_preamble("dt_unpack_arrayrowindex(%s, "
                                    "(void**) &riarr);" % target)
                iexpr = "i = riarr[ii];"

        fn = ("int {func}({args}) {{\n"
              "    int64_t i;\n"
              "    {preamble}\n"
              "    for (int64_t ii = row0; ii < row1; ii++) {{\n"
              "        {iexpr}\n"
              "        {mainloop}\n"
              "    }}\n"
              "    {epilogue}\n"
              "    return 0;\n"
              "}}\n").format(func=self._fnname,
                             args=args,
                             iexpr=iexpr,
                             preamble="\n    ".join(self._preamble),
                             mainloop="\n        ".join(self._mainloop),
                             epilogue="\n    ".join(self._epilogue),
                             )
        return self._cnode.add_function(self._fnname, fn)



#===============================================================================

class MapNode(IteratorNode):

    def __init__(self, dt, exprs):
        super().__init__(dt, name="map")
        self._exprs = exprs
        self._rowindex = None

    def use_rowindex(self, ri):
        self._rowindex = ri

    def add_expression(self, expr):
        self._exprs.append(expr)

    def generate_c(self):
        self._fnname = self._cnode.make_variable_name("map")
        self._extraargs = "void **out"
        for i, expr in enumerate(self._exprs):
            ctype = expr.ctype
            value = expr.value(inode=self)
            self.addto_preamble("{ctype} *out{i} = ({ctype}*) (out[{i}]);\n"
                                .format(ctype=ctype, i=i))
            self.addto_mainloop("out{i}[ii] = {value};\n"
                                .format(i=i, value=value))
        # Call the super method
        return IteratorNode.generate_c(self)
