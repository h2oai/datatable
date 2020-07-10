#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2019-2020 H2O.ai
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
import re
from docutils import nodes
from sphinx.util.docutils import SphinxDirective
from . import xnodes

rx_shell_program = re.compile(r"[\w\.\/\-]+")



class XcodeDirective(SphinxDirective):
    has_content = True
    required_arguments = 0
    optional_arguments = 1
    final_argument_whitespace = False
    option_spec = {}

    def run(self):
        self.parse_arguments()
        if self._lang == "shell":
            return self.process_lang_shell()


    def parse_arguments(self):
        assert len(self.arguments) <= 1
        lang = self.arguments[0] if self.arguments else "python"
        if lang not in ["shell", "python", "text"]:
            raise self.error("Invalid language in ..xcode directive: `%s`. "
                             "Only 'shell', 'python' and 'text' are recognized."
                             % lang)
        self._lang = lang


    def process_lang_shell(self):
        out_node = xnodes.div(classes=["xcode", "shell"])
        for line in self.content.data:
            prompt = ""
            comment = ""
            if line[:2] == "$ ":
                prompt = line[:2]
                line = line[2:]
            if line[1:5] == ":\\> ":
                prompt = line[:5]
                line = line[5:]
            if prompt:
                out_node += nodes.inline("", prompt, classes=["prompt"])
            mm_program = re.match(rx_shell_program, line)
            if mm_program:
                iend = mm_program.end()
                out_node += nodes.inline("", line[:iend], classes=["program"])
                line = line[iend:]
            if "#" in line:
                istart = line.find("#")
                comment = line[istart:]
                line = line[:istart]
            out_node += nodes.Text(line)
            if comment:
                out_node += nodes.inline("", comment, classes=["comment"])
            out_node += nodes.Text("\n")
        return [out_node]



def setup(app):
    app.setup_extension("sphinxext.xnodes")
    app.add_css_file("xcode.css")
    app.add_directive("xcode", XcodeDirective)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
