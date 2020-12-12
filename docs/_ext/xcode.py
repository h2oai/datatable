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
#
# When docutils encounters '::' at the end of a paragraph, it converts the
# following paragraph into a literal block, i.e. a nodes.literal_block is
# inserted into the doctree (see docutils/parsers/rst/states.py:L2791).
#
# Similarly, the Sphinx' `.. code-block::` directive inserts a single
# nodes.literal_block into the doctree (possibly wrapped in a container if
# there is a caption). The node will also have a 'language' attribute (see
# sphinx/directives/code.py::CodeBlock class).
#
# When Sphinx renders `literal_block`s into HTML (see
# sphinx/writers/html5.py::HTML5Translator.visit_literal_block), the content
# of the block is highlighted via
#
#     highlighted = self.highlighter.highlight_block(
#         node.rawsource, lang, opts=opts, linenos=linenos,
#         location=(self.builder.current_docname, node.line), **highlight_args
#     )
#
# where `self.highlighter = PygmentsBridge('html', style)`, and `style` is
# taken from the configuration options or from the theme. The PygmentsBridge
# class (sphinx/highlighting.py::PygmentsBridge) allows to set a custom html
# formatter to use in `pygments.highlight(source, lexer, formatter)`, as well
# as custom lexer for every language.
#
#-------------------------------------------------------------------------------
import docutils
import pygments
import pygments.formatter
import pygments.token as tok
import re
from docutils import nodes
from sphinx.util.docutils import SphinxDirective
from pygments.formatters.html import escape_html
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
# XHtmlFormatter
#-------------------------------------------------------------------------------

class XHtmlFormatter(pygments.formatter.Formatter):

    def __init__(self, lang='default', **kwargs):
        super().__init__(**kwargs)
        self._lang = lang.lower()
        if 'msdos' in self._lang:
            self._lang += " bash"

    def filter(self, tokens):
        if 'python' in self._lang:
            yield from self.python_filter(tokens)
        else:
            yield from self.default_filter(tokens)

    def default_filter(self, tokens):
        yield ("Code:start", None)
        for ttype, tvalue in self.mend_tokens(tokens):
            if tvalue:
                yield (ttype, tvalue)
        yield ("Code:end", None)

    def python_filter(self, tokens):
        mode = None
        output = []
        for ttype, tvalue in self.mend_tokens(tokens):
            if ttype == tok.Generic.Prompt:
                if mode == "input_block":
                    continue
                if mode == "output_block":
                    yield ("Output:start", None)
                    yield (tok.Text, "\n".join(output))
                    yield ("Output:end", None)
                    output = []
                mode = "input_block"
                yield ("Input:start", None)
            elif ttype == tok.Generic.Output:
                if mode == "input_block":
                    yield ("Input:end", None)
                mode = "output_block"
                output.append(tvalue)
            else:
                if mode is None:
                    mode = "code_block"
                    yield ("Code:start", None)
                yield (ttype, tvalue)
        if mode == "input_block":
            yield ("Input:end", None)
        if mode == "output_block":
            yield ("Output:start", None)
            yield (tok.Text, "\n".join(output))
            yield ("Output:end", None)
        if mode == "code_block":
            yield ("Code:end", None)


    def mend_tokens(self, tokens):
        stored = None
        for tt, tv in self.merge_tokens(tokens):
            if stored:
                if (stored[0] == tok.String.Affix and tt in tok.String) or \
                   (stored[0] == tok.Operator and stored[1] in '+-' and tt in tok.Number):
                    tv = stored[1] + tv
                else:
                    yield stored
                stored = None
            if (tt, tv) == (tok.Name.Builtin.Pseudo, "Ellipsis"):
                tt = tok.Keyword.Constant
            elif (tt, tv) == (tok.Operator, "..."):
                tt = tok.Keyword.Constant
            elif (tt == tok.String.Affix or
                  (tt == tok.Operator and tv in '+-')):
                stored = tt, tv
                continue
            yield (tt, tv)
        if stored:
            yield stored


    def merge_tokens(self, tokens):
        last_ttype = None
        last_tvalue = ''
        for ttype, tvalue in tokens:
            if ttype in [tok.String.Affix, tok.String.Delimiter,
                         tok.String.Single, tok.String.Double,
                         tok.String.Char, tok.String.Backtick,
                         tok.String.Symbol]:
                ttype = tok.String
            if ttype == last_ttype:
                last_tvalue += tvalue
            else:
                if last_tvalue:
                    yield last_ttype, last_tvalue
                last_ttype = ttype
                last_tvalue = tvalue
        if last_tvalue:
            yield last_ttype, last_tvalue


    def format_unencoded(self, tokenstream, outfile):
        MAP = tok.STANDARD_TYPES
        outfile.write("<div class='xcode'>")
        for ttype, tvalue in self.filter(tokenstream):
            if isinstance(ttype, str):
                if ttype == "Code:start":
                    outfile.write("<code class='%s'>" % self._lang)
                if ttype == "Code:end":
                    outfile.write("</code>")
                if ttype == "Input:start":
                    outfile.write("<code class='%s input'>" % self._lang)
                if ttype == "Input:end":
                    outfile.write("</code>")
                if ttype == "Output:start":
                    outfile.write("<div class='output'>")
                if ttype == "Output:end":
                    outfile.write("</div>")
            else:
                cls = MAP[ttype]
                tvalue = escape_html(tvalue)
                if cls:
                    outfile.write("<span class=%s>%s</span>" % (cls, tvalue))
                else:
                    outfile.write(tvalue)
        outfile.write("</div>")
        # self._original_formatter.format(tokens, outfile)



#-------------------------------------------------------------------------------
# Extension setup
#-------------------------------------------------------------------------------

def my_get_lexer(self, source, lang, opts=None, force=False, location=None):
    """
    Replacement method for PygmentsBridge.get_lexer, which supplies
    argument `lang` to the html formatter.
    """
    lexer = self._get_lexer(source, lang, opts, force, location)
    self.formatter_args["lang"] = lexer.name
    return lexer


def setup(app):
    from sphinx.highlighting import PygmentsBridge
    PygmentsBridge.html_formatter = XHtmlFormatter
    PygmentsBridge._get_lexer = PygmentsBridge.get_lexer
    PygmentsBridge.get_lexer = my_get_lexer

    app.setup_extension("_ext.xnodes")
    app.add_css_file("xcode.css")
    # app.add_directive("xcode", XcodeDirective)
    app.add_node(XcodeRootElement, html=(xnodes.visit_div, xnodes.depart_div))
    return {"parallel_read_safe": True, "parallel_write_safe": True}
