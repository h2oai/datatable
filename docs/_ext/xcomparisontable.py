#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2021 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------
from docutils import nodes
from docutils.parsers.rst import directives
from docutils.statemachine import StringList
# from sphinx import addnodes
# from sphinx.util import logging
from sphinx.util.docutils import SphinxDirective
from . import xnodes


def split_stringlist(slist, sep):
    i0 = 0
    for i, line in enumerate(slist):
        if line == sep:
            if i > 0:
                yield slist[i0:i]
            i0 = i + 1
    if i0 < len(slist):
        yield slist[i0:]


class XComparisonTableDirective(SphinxDirective):
    has_content = True
    required_arguments = 0
    optional_arguments = 0
    option_spec = {
        "header1": directives.unchanged_required,
        "header2": directives.unchanged_required,
    }

    def run(self):
        self.header1 = self.options["header1"].strip()
        self.header2 = self.options["header2"].strip()
        tbl = xnodes.table(classes=["x-comparison-table"])
        tbl += xnodes.tr(
            xnodes.th(xnodes.div(self.header1)),
            xnodes.th(xnodes.div(self.header2)),
            classes=["table-header"])
        for header, col1, col2 in self.parse_content():
            classes=["section-header"]
            if header:
                th = xnodes.td(colspan=2)
                self.state.nested_parse(header, 0, th)
                tbl += xnodes.tr(th, classes=classes)
                classes = []
            td1 = xnodes.td()
            td2 = xnodes.td()
            self.state.nested_parse(col1, 0, td1)
            self.state.nested_parse(col2, 0, td2)
            tbl += xnodes.tr(td1, td2, classes=classes)
        return [tbl]

    def parse_content(self):
        for section in split_stringlist(self.content, "===="):
            parts = list(split_stringlist(section, '----'))
            if len(parts) == 3:
                yield parts
            elif len(parts) == 2:
                yield (None, parts[0], parts[1])
            else:
                raise self.error("Wrong number of sub-sections at %s lines %d-%d" %
                                 (section.source(0), section.offset(0), section.offset(-1)))



def setup(app):
    app.setup_extension("_ext.xnodes")
    app.add_directive("x-comparison-table", XComparisonTableDirective)
    app.add_css_file("x-comparison-table.css")
    return {"parallel_read_safe": True,
            "parallel_write_safe": True}

