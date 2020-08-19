#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2020 H2O.ai
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
import pygments
import pygments.formatter
import pygments.token as tok
import re
from docutils import nodes
from sphinx.util.docutils import SphinxDirective
from . import xnodes


class XcodeRootElement(xnodes.div, nodes.FixedTextElement):
    """
    Inherit from FixedTextElement so that 'smartquotes' utility
    wouldn't try to convert quotation marks and such.
    """
    def __init__(self, cls):
        super().__init__(classes=["xcode", cls])




class SphinxFormatter(pygments.formatter.Formatter):

    _shell_token_map = {
        tok.Comment.Hashbang: tok.Comment,
        tok.Comment.Single:   tok.Comment,
        tok.Generic.Output:   tok.Generic.Output,
        tok.Generic.Prompt:   tok.Generic.Prompt,
        tok.Keyword:          tok.Keyword,
        tok.Name.Builtin:     tok.Keyword,
        tok.Name.Variable:    tok.Name.Variable,
        tok.Number:           tok.Text,
        tok.Operator:         tok.Text,
        tok.Punctuation:      tok.Text,
        tok.String.Backtick:  tok.String,
        tok.String.Double:    tok.String,
        tok.String.Escape:    tok.String,
        tok.String.Interpol:  tok.Name.Variable,
        tok.String.Single:    tok.String,
        tok.String:           tok.String,
        tok.Text:             tok.Text,
    }

    def __init__(self, lang):
        super().__init__()
        self._lang = lang
        if lang == "console":
            self._token_map = SphinxFormatter._shell_token_map
        else:
            raise ValueError("Unknown lex language %r" % (lang,))
        self._nodegen = self.build_node_generator()


    def build_node_generator(self):
        def class_applicator(cls):
            return lambda v: nodes.inline("", v, classes=[cls])

        nodegen = {tok.Text: nodes.Text}
        for ttype in self._token_map.values():
            if ttype in nodegen: continue
            cls = str(ttype).split('.')[-1].lower()
            nodegen[ttype] = class_applicator(cls)
        return nodegen


    def merge_tokens(self, tokenstream):
        tokmap = self._token_map
        last_value = None
        last_ttype = None
        for ttype, value in tokenstream:
            if ttype in tokmap:
                ttype = tokmap[ttype]
                if ttype == last_ttype:
                    last_value += value
                else:
                    if last_value:
                        yield last_ttype, last_value
                    last_ttype = ttype
                    last_value = value
            else:
                raise ValueError("Unknown token type %r in %r formatter"
                                 % (ttype, self._lang))
        if last_value:
            yield last_ttype, last_value


    def format(self, tokenstream, outfile):
        out = XcodeRootElement(self._lang)
        for ttype, value in self.merge_tokens(tokenstream):
            out += self._nodegen[ttype](value)
        outfile.result = out




#-------------------------------------------------------------------------------
# Xcode Directive
#-------------------------------------------------------------------------------

class XcodeDirective(SphinxDirective):
    has_content = True
    required_arguments = 0
    optional_arguments = 1
    final_argument_whitespace = False
    option_spec = {}

    def run(self):
        self.parse_arguments()
        lang = self._lang
        code = "\n".join(self.content.data)
        if lang in ["shell", "console", "bash"]:
            lexer = pygments.lexers.get_lexer_by_name("console")
            lang = "console"
        elif lang in ["winshell", "shell-windows", "winconsole", "doscon"]:
            lexer = pygments.lexers.get_lexer_by_name("doscon")
            lang = "console"
        else:
            lexer = pygments.lexers.get_lexer_by_name(lang)

        if lang == "console":
            formatter = SphinxFormatter("console")
            # Update prompt regexp so that it would include the whitespace too
            tail = r")(.*\n?)"
            ps1 = lexer._ps1rgx.pattern
            assert ps1.endswith(tail)
            lexer._ps1rgx = re.compile(ps1[:-len(tail)] + r"\s*" + tail)

        outfile = type("", (object,), dict(result=None))()
        pygments.highlight(code, lexer, formatter, outfile)
        return [outfile.result]


    def parse_arguments(self):
        assert len(self.arguments) <= 1
        lang = self.arguments[0] if self.arguments else "python"
        self._lang = lang




#-------------------------------------------------------------------------------
# Extension setup
#-------------------------------------------------------------------------------

def setup(app):
    app.setup_extension("sphinxext.xnodes")
    app.add_css_file("xcode.css")
    app.add_directive("xcode", XcodeDirective)
    app.add_node(XcodeRootElement, html=(xnodes.visit_div, xnodes.depart_div))
    return {"parallel_read_safe": True, "parallel_write_safe": True}
