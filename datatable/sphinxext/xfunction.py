#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2019 H2O.ai
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
"""
Sphinx directive to generate documentation for functions/methods
without having to build the module from source.

In order to enable this extension you need it to add it into the
list of all extensions in the "conf.py" file:

    extensions = [
        ...,
        'datatable.sphinxext.dtfunction_directive'
    ]

This makes several directives available for use in the .rst files:

    .. xdata: datatable.math.tau

    .. xfunction: datatable.fread

    .. xmethod: datatable.Frame.cbind
        :src: c/frame/cbind.cc::Frame::cbind
        :doc: c/frame/cbind.cc::doc_cbind

"""
import os
import re
from docutils import nodes
from docutils.parsers.rst import directives
from docutils.statemachine import StringList
from sphinx import addnodes
from sphinx.util import logging
from sphinx.util.docutils import SphinxDirective
from sphinx.util.nodes import make_refnode

logger = logging.getLogger(__name__)

rx_cc_id = re.compile(r"(?:\w+::)*\w+")
rx_py_id = re.compile(r"(?:\w+\.)*\w+")
rx_param = re.compile(r"(?:"
                      r"(\w+)(?:\s*=\s*("
                      r"\"[^\"]*\"|\([^\(\)]*\)|\[[^\[\]]*\]|[^,\[\(\"]*"
                      r"))?"
                      r"|([\*/]|\*\*?\w+)"
                      r")\s*(?:,\s*|$)")
rx_header = re.compile(r"(\-{3,})\s*")
rx_return = re.compile(r"[\[\(]?returns?[\]\)]?")

# Build with pdb on exceptions:
#
#     sphinx-build -P -b html . _build/html
#


#-------------------------------------------------------------------------------
# XobjectDirective
#-------------------------------------------------------------------------------

class XobjectDirective(SphinxDirective):
    """
    Fields used by this class:

    ==[ Derived from config/arguments/options ]==
        self.module_name -- config variable xf_module_name
        self.project_root -- config variable xf_project_root
        self.permalink_fn -- config variable xf_permalink_fn
        self.obj_name -- python name of the object being documented
        self.qualifier -- object's qualifier, this string + obj_name give a
                          fully-qualified object name.
        self.src_file -- name of the file where the code is located
        self.src_fnname -- name of the C++ function (possibly with a namespace)
                           which corresponds to the object being documented
        self.doc_file -- name of the file where the docstring is located
        self.doc_var -- name of the C++ variable containing the docstring

    ==[ Parsed from the source files(s) ]==
        self.src_line_first -- starting line of the function in self.src_file
        self.src_line_last -- final line of the function in self.src_file
        self.src_github_url -- URL of the function's code on GitHub
        self.doc_text -- text of the object's docstring
        self.doc_github_url -- URL of the docstring on the GitHub

    ==[ Parsed from the docstring ]==
        self.parsed_params -- list of parameters of this function; each entry
                              is either the parameter itself (str), or a tuple
                              of strings (parameter, default_value).

    See also:
        sphinx/directives/__init__.py::ObjectDescription
        sphinx/domains/python.py::PyObject
    """
    has_content = False
    required_arguments = 1
    optional_arguments = 0
    final_argument_whitespace = False
    option_spec = {
        "src": directives.unchanged_required,
        "doc": directives.unchanged,
    }

    def run(self):
        self._parse_config()
        self._parse_arguments()
        self._parse_option_src()
        self._parse_option_doc()

        if self.doc_file == self.src_file:
            self._locate_sources(self.src_file, self.src_fnname, self.doc_var)
        else:
            self._locate_sources(self.src_file, self.src_fnname, None)
            self._locate_sources(self.doc_file, None, self.doc_var)
        self._parse_docstring()
        return self._generate_nodes()


    #---------------------------------------------------------------------------
    # Initialization: parse input options
    #---------------------------------------------------------------------------

    def _parse_config(self):
        self.module_name = self.env.config.xf_module_name
        self.project_root = self.env.config.xf_project_root
        self.permalink_fn = self.env.config.xf_permalink_fn
        assert isinstance(self.module_name, str)
        assert isinstance(self.project_root, str)
        assert self.permalink_fn is None or callable(self.permalink_fn)


    def _parse_arguments(self):
        """
        Process `self.arguments`, verify their validity, and extract fields
        `self.obj_name` and `self.qualifier`.
        """
        assert len(self.arguments) == 1  # checked from required_arguments field
        fullname = self.arguments[0].strip()
        if not re.fullmatch(rx_py_id, fullname):
            raise self.error("Invalid argument to %s directive: must be a "
                             "valid python indentifier, instead got `%s`"
                             % (self.name, fullname))
        parts = fullname.rsplit('.', maxsplit=1)
        if len(parts) == 1:
            self.obj_name = parts[0]
            self.qualifier = self.module_name + '.'
        else:
            self.obj_name = parts[1]
            if parts[0].startswith(self.module_name):
                self.qualifier = parts[0] + '.'
            else:
                self.qualifier = self.module_name + '.' + parts[0] + '.'


    def _parse_option_src(self):
        """
        Process the required option `:src:`, and extract fields
        `self.src_file` and `self.src_fnname`.
        """
        src = self.options["src"].strip()
        parts = src.split()
        if len(parts) != 2:
            raise self.error("Invalid :src: option: it must have form "
                             "'filename fnname'")
        if not os.path.isfile(os.path.join(self.project_root, parts[0])):
            raise self.error("Invalid :src: option: file `%s` does not exist"
                             % parts[0])
        if not re.fullmatch(rx_cc_id, parts[1]):
            raise self.error("Invalid :src: option: `%s` is not a valid C++ "
                             "identifier" % parts[1])
        self.src_file = parts[0]
        self.src_fnname = parts[1]


    def _parse_option_doc(self):
        """
        Process the nonmandatory option `:doc:`, and extract fields
        `self.doc_file` and `self.doc_var`. If the option is not
        provided, the fields are set to their default values.
        """
        doc = self.options.get("doc", "").strip()
        parts = doc.split()

        if len(parts) == 0:
            self.doc_file = self.src_file
        elif os.path.isfile(os.path.join(self.project_root, parts[0])):
            self.doc_file = parts[0]
        else:
            raise self.error("Invalid :doc: option: file `%s` does not exist"
                             % parts[0])

        if len(parts) <= 1:
            self.doc_var = "doc_" + self.obj_name
        elif re.fullmatch(rx_cc_id, parts[1]):
            self.doc_var = parts[1]
        else:
            raise self.error("Invalid :doc: option: `%s` is not a valid C++ "
                             "identifier" % parts[1])

        if len(parts) > 2:
            raise self.error("Invalid :doc: option: it must have form "
                             "'filename [docname]'")


    #---------------------------------------------------------------------------
    # Processing: retrieve docstring / function source
    #---------------------------------------------------------------------------

    def _locate_sources(self, filename, funcname, docname):
        """
        Locate either the function body or the documentation string or
        both in the source file `filename`.

        See :meth:`_locate_fn_source` and :meth:`_locate_doc_source`
        for details.
        """
        full_filename = os.path.join(self.project_root, filename)
        with open(full_filename, "r", encoding="utf-8") as inp:
            lines = list(inp)
        if funcname:
            self._locate_fn_source(filename, funcname, lines)
        if docname:
            self._locate_doc_source(filename, docname, lines)


    def _locate_fn_source(self, filename, fnname, lines):
        """
        Find the body of the function `fnname` within the `lines` that
        were read from the file `filename`.

        If successful, this function sets properties
        `self.src_line_first`, `self.src_line_last`, and
        `self.src_github_url`.
        """
        rx_cc_function = re.compile(r"(\s*)"
                                    r"(?:static\s+|inline\s+)*"
                                    r"(?:void|oobj|py::oobj)\s+" +
                                    fnname +
                                    r"\s*\(.*\)\s*\{\s*")
        expect_closing = None
        start_line = None
        finish_line = None
        for i, line in enumerate(lines):
            if expect_closing:
                if line.startswith(expect_closing):
                    finish_line = i + 1
                    break
            elif fnname in line:
                start_line = i + 1
                mm = re.match(rx_cc_function, line)
                if mm:
                    expect_closing = mm.group(1) + "}"
                else:
                    mm = re.match(rx_cc_function, line + lines[i+1])
                    if mm:
                        expect_closing = mm.group(1) + "}"
        if not start_line:
            raise self.error("Could not find function `%s` in file `%s`"
                             % (fnname, filename))
        if not expect_closing:
            raise self.error("Unexpected signature of function `%s` "
                             "in file `%s` line %d"
                             % (fnname, filename, start_line))
        if not finish_line:
            raise self.error("Could not locate the end of function `%s` "
                             "in file `%s` line %d"
                             % (fnname, filename, start_line))

        self.src_line_first = start_line
        self.src_line_last = finish_line
        if self.permalink_fn:
            self.src_github_url = self.permalink_fn(filename, start_line,
                                                    finish_line)


    def _locate_doc_source(self, filename, docname, lines):
        """
        Find the body of the function's docstring within the `lines`
        of file `filename`. The docstring is expected to be in the
        `const char* {docname}` variable. An error will be raised if
        the docstring cannot be found.

        Upon success, this function creates variable `self.doc_text`
        containing the text of the docstring, and also the property
        `self.doc_github_url`.
        """
        rx_cc_docstring = re.compile(r"\s*(?:static\s+)?const\s+char\s*\*\s*" +
                                     docname +
                                     r"\s*=\s*(.*?)\s*")
        doc_text = None
        start_line = None
        finish_line = None
        line_added = False
        for i, line in enumerate(lines):
            if line_added:
                line_added = False
            elif doc_text:
                end_index = line.find(')";')
                if end_index == -1:
                    doc_text += line
                else:
                    doc_text += line[:end_index]
                    finish_line = i + 1
                    break
            elif docname in line:
                start_line = i + 1
                mm = re.fullmatch(rx_cc_docstring, line)
                if mm:
                    doc_start = mm.group(1)
                    if not doc_start:
                        doc_start = lines[i + 1].strip()
                        line_added = True
                    if doc_start.startswith('"'):
                        if doc_start.endswith('";'):
                            doc_text = doc_start[1:-2]
                            finish_line = start_line + line_added
                            break
                        else:
                            raise self.error("Unexpected document string: `%s` "
                                             "in file %s line %d"
                                             % (doc_start, filename, i + 1))
                    elif doc_start.startswith('R"('):
                        doc_text = doc_start[3:] + "\n"
        if doc_text is None:
            raise self.error("Could not find docstring `%s` in file `%s`"
                             % (docname, filename))
        if not finish_line:
            raise self.error("Docstring `%s` in file `%s` started on line %d "
                             "but did not finish"
                             % (docname, filename, start_line))

        self.doc_text = doc_text.strip()
        if self.permalink_fn:
            self.doc_github_url = self.permalink_fn(filename, start_line,
                                                    finish_line)



    #---------------------------------------------------------------------------
    # Processing: parse the docstring
    #---------------------------------------------------------------------------

    def _parse_docstring(self):
        """
        Split the docstring into two parts: the signature and the
        body (the parts should be separated with a "--" line). After
        that, defers parsing of each part to :meth:`_parse_parameters`
        and :meth:`_parse_body` respectively.
        """
        tmp = self.doc_text.split("--\n", 1)
        if len(tmp) == 1:
            raise self.error("Docstring for `%s` does not contain '--\\n'"
                             % self.obj_name)
        signature = tmp[0].strip()
        if not (signature.startswith(self.obj_name + "(") and
                signature.endswith(")")):
            raise self.error("Unexpected docstring: should have started with "
                             "`%s(` and ended with `)`, instead found %r"
                             % (self.obj_name, signature))
        # strip objname and the parentheses
        signature = signature[(len(self.obj_name) + 1):-1]
        self._parse_parameters(signature)
        self._parse_body(tmp[1])


    def _parse_parameters(self, sig):
        """
        Parse the *signature* part of the docstring, and extract the
        list of parameters, together with their default values. The
        extracted parameters are stored in the `self.parsed_params`
        variable.

        The entries in the `self.parsed_params` list can be any of
        the following:
          - varname             # regular parameter
          - (varname, default)  # parameter with default
          - "/"                 # separator for pos-only arguments
          - "*"                 # separator for kw-only arguments
          - "*" + varname       # positional varargs
          - "**" + varname      # keyword varargs

        Type annotations in the signature are not supported.
        """
        sig = sig.strip()
        i = 0  # Location within the string where we currently parse
        parsed = []
        while i < len(sig):
            mm = re.match(rx_param, sig[i:])
            if mm:
                if mm.group(1) is None:  # "special" variable
                    assert mm.group(3)
                    parsed.append(mm.group(3))
                elif mm.group(2) is None:  # without default
                    assert mm.group(1)
                    parsed.append(mm.group(1))
                else:
                    assert mm.group(1) and mm.group(2)
                    parsed.append( (mm.group(1), mm.group(2)) )
                assert mm.pos == 0
                i += mm.end()
            else:
                raise self.error("Cannot parse signature string %r at "
                                 "position %d; already parsed: %r"
                                 % (sig, i, parsed))
        self.parsed_params = parsed


    def _parse_body(self, body):
        """
        Parse/transform the body of the function's docstring.
        """
        self.parsed_body = self._split_into_sections(body)
        for header, section in self.parsed_body:
            self._transform_codeblocks(section)
            if header == "Parameters":
                self._transform_parameters(section)


    def _split_into_sections(self, body):
        body = body.strip()
        parsed = [[]]
        headers = [None]
        last_line = None
        for line in  body.split("\n"):
            mm = rx_header.fullmatch(line)
            if mm:
                header = last_line.strip()
                if len(header) != len(mm.group(1)):
                    raise self.error("Incorrect header `%s` in a doc string "
                                     "for %s: it has %d characters, while the "
                                     "underline has %d characters"
                                     % (header, self.obj_name, len(header),
                                        len(mm.group(1))))
                parsed[-1].pop()
                parsed.append([])
                headers.append(header)
                last_line = None
            else:
                parsed[-1].append(line)
                last_line = line
        return list(zip(headers, parsed))


    def _transform_codeblocks(self, lines):
        rx_codeblock = re.compile(
            r"``([^`]+)``|"
            r":`([^`]+)`|"
            r"`([^`]+)`"
        )
        def replacefn(match):
            if match.group(1) or match.group(2):
                return match.group(0)
            else:
                return "`" + match.group(0) + "`"

        for i, line in enumerate(lines):
            if '`' in line:
                lines[i] = re.sub(rx_codeblock, replacefn, line)


    def _transform_parameters(self, lines):
        i = 0
        while i < len(lines):
            line = lines[i]
            if line and line[0] != " ":
                lines[i] = ".. xparam:: " + line
                lines.insert(i + 1, "")
            i += 1



    #---------------------------------------------------------------------------
    # Rendering into nodes
    #---------------------------------------------------------------------------

    def _generate_nodes(self):
        title_text = self.qualifier + self.obj_name
        sect = nodes.section(ids=[title_text], classes=["x-function"])
        sect += nodes.title("", self.obj_name)
        sect += self._index_node(title_text)
        sect += self._generate_signature(title_text)
        sect += self._generate_body()
        return [sect]


    def _index_node(self, targetname):
        text = self.obj_name
        if self.name == "xmethod":
            text += " (%s method)" % self.qualifier[:-1]
        if self.name == "xfunction":
            text += " (%s function)" % self.qualifier[:-1]
        if self.name == "xdata":
            text += " (%s attribute)" % self.qualifier[:-1]
        inode = addnodes.index(entries=[("single", text, targetname, "", None)])
        return [inode]


    def _generate_signature(self, targetname):
        sig_node = mydiv_node(classes=["sig-container"], ids=[targetname])
        sig_nodeL = mydiv_node(classes=["sig-body"])
        self._generate_sigbody(sig_nodeL)
        sig_node += sig_nodeL
        sig_nodeR = mydiv_node(classes=["code-links"])
        self._generate_siglinks(sig_nodeR)
        sig_node += sig_nodeR

        # Tell Sphinx that this is a target for `:py:obj:` references
        self.state.document.note_explicit_target(sig_node)
        inv = self.env.domaindata["py"]["objects"]
        inv[targetname] = (self.env.docname, self.name[1:])
        return [sig_node]


    def _generate_sigbody(self, node):
        row1 = mydiv_node(classes=["sig-qualifier"])
        ref = addnodes.pending_xref("", nodes.Text(self.qualifier),
                                    reftarget=self.qualifier[:-1],
                                    reftype="class", refdomain="py")
        # Note: `ref` cannot be added directly: docutils requires that
        # <reference> nodes were nested inside <TextElement> nodes.
        row1 += nodes.generated("", "", ref)
        node += row1

        row2 = mydiv_node(classes=["sig-main"])
        self._generate_sigmain(row2)
        node += row2


    def _generate_siglinks(self, node):
        node += a_node(href=self.src_github_url, text="source", new=True)
        if self.doc_file != self.src_file:
            node += a_node(href=self.doc_github_url, text="doc", new=True)


    def _generate_sigmain(self, node):
        div1 = mydiv_node(classes=["sig-name"])
        div1 += nodes.Text(self.obj_name)
        node += div1
        if self.name != "xdata":
            node += mydiv_node(classes=["sig-open-paren"])
            params = mydiv_node(classes=["sig-parameters"])
            last_i = len(self.parsed_params) - 1
            for i, param in enumerate(self.parsed_params):
                classes = ["param"]
                if param == "self": classes += ["self"]
                if param == "*" or param == "/": classes += ["special"]
                if i == last_i: classes += ["final"]
                if isinstance(param, str):
                    if param in ["self", "*", "/"]:
                        ref = nodes.Text(param)
                    else:
                        ref = a_node(text=param, href="#" + param)
                    params += mydiv_node("", ref, classes=classes)
                else:
                    assert isinstance(param, tuple)
                    ref = a_node(text=param[0], href="#" + param[0])
                    p = mydiv_node("", ref, classes=classes)
                    deflt = mydiv_node(classes=["default"])
                    deflt += nodes.Text(param[1])
                    p += deflt
                    params += p
            params += mydiv_node(classes=["sig-close-paren"])
            node += params


    def _generate_body(self):
        out = mydiv_node(classes=["x-function-body"])
        for head, lines in self.parsed_body:
            if head:
                lines = [head, "="*len(head), ""] + lines
            content = StringList(lines)
            self.state.nested_parse(content, self.content_offset, out,
                                    match_titles=True)
        return out




#-------------------------------------------------------------------------------
# XparamDirective
#-------------------------------------------------------------------------------

class XparamDirective(SphinxDirective):
    has_content = True
    required_arguments = 2
    optional_arguments = 0
    final_argument_whitespace = True

    def run(self):
        self._parse_arguments()
        root = mydiv_node(classes=["xparam-box"])
        head = mydiv_node(classes=["xparam-head"])
        if re.fullmatch(rx_return, self.param):
            param_node = mydiv_node(classes=["param", "return"], ids=["return"])
            param_node += nodes.Text("(return)")
        else:
            id0 = self.param.strip("*/")
            param_node = mydiv_node(classes=["param"], ids=[id0])
            param_node += nodes.Text(self.param)
        head += param_node
        types_node = mydiv_node(classes=["types"])
        types_str = " | ".join("``%s``" % p for p in self.types)
        self.state.nested_parse(StringList([types_str]), self.content_offset,
                                types_node)
        assert isinstance(types_node[0], nodes.paragraph)
        types_node.children = types_node[0].children
        head += types_node
        root += head
        desc_node = mydiv_node(classes=["xparam-body"])
        self.state.nested_parse(self.content, self.content_offset, desc_node)
        root += desc_node
        return [root]


    def _parse_arguments(self):
        self.param = self.arguments[0].rstrip(":")
        self.types = re.split(r"(?:,?\s+or\s+|\s*[,\|]\s*)", self.arguments[1])




#-------------------------------------------------------------------------------
# Nodes
#-------------------------------------------------------------------------------

class mydiv_node(nodes.General, nodes.Element): pass

def visit_div(self, node):
    # Note: Sphinx's `.starttag()` adds a newline after the tag, which causes
    # problem if the content of the div is a text node
    self.body.append(self.starttag(node, "div").strip())

def depart_div(self, node):
    self.body.append("</div>")



class a_node(nodes.Element):
    def __init__(self, href, text="", new=False, **attrs):
        super().__init__(**attrs)
        self.href = href
        self.new = new
        if text:
            self += nodes.Text(text)

def visit_a(self, node):
    html = "<a href=\"%s\"" % node.href
    if node.new:
        html += " target=_new"
    html += ">"
    self.body.append(html)

def depart_a(self, node):
    self.body.append("</a>")



#-------------------------------------------------------------------------------
# Setup
#-------------------------------------------------------------------------------

def setup(app):
    app.add_config_value("xf_module_name", None, "env")
    app.add_config_value("xf_project_root", "..", "env")
    app.add_config_value("xf_permalink_fn", None, "env")
    app.add_css_file("xfunction.css")
    app.add_js_file("https://use.fontawesome.com/0f455d5fb2.js")
    app.add_directive("xdata", XobjectDirective)
    app.add_directive("xfunction", XobjectDirective)
    app.add_directive("xmethod", XobjectDirective)
    app.add_directive("xparam", XparamDirective)
    app.add_node(mydiv_node, html=(visit_div, depart_div))
    app.add_node(a_node, html=(visit_a, depart_a))
    return {"parallel_read_safe": True, "parallel_write_safe": True}
