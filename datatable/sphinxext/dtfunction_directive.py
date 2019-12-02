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

    .. dtdata: datatable.math.tau

    .. dtfunction: datatable.fread

    .. dtmethod: datatable.Frame.cbind
        :src: c/frame/cbind.cc::Frame::cbind
        :doc: c/frame/cbind.cc::doc_cbind


"""
import os
import re
from docutils import nodes
from docutils.parsers.rst import directives
from docutils.parsers.rst import Directive

project_root = ".."
github_root = "https://github.com/h2oai/datatable"
github_commit = "94decf5738e99941a709bfe33ae8aca2c943f35e"
module_name = "datatable"

rx_cc_id = re.compile(r"(?:\w+::)*\w+")
rx_py_id = re.compile(r"(?:\w+\.)*\w+")
rx_param = re.compile(r"(?:"
                      r"(\w+)(?:\s*=\s*("
                      r"\"[^\"]*\"|\([^\(\)]*\)|\[[^\[\]]*\]|[^,\[\(\"]*"
                      r"))?"
                      r"|([\*/]|\*\*?\w+)"
                      r")\s*(?:,\s*|$)")

def file_ok(filename):
    return os.path.isfile(os.path.join(project_root, filename))

def src_permalink(filename, line1, line2):
    return ("%s/blob/%s/%s#L%d-L%d"
            % (github_root, github_commit, filename, line1, line2))


from sphinx.util import logging
logger = logging.getLogger(__name__)


#-------------------------------------------------------------------------------
# DtobjectDirective
#-------------------------------------------------------------------------------

class DtobjectDirective(Directive):
    """
    Fields used by this class:

    self.obj_name -- python name of the object being documented
    self.qualifier -- object's qualifier, this string + obj_name give a fully-
                      qualified object name.
    self.src_file -- name of the file where the code is located
    self.src_fnname -- name of the C++ function (possibly with a namespace)
                       which corresponds to the object being documented
    self.doc_file -- name of the file where the docstring is located
    self.doc_var -- name of the C++ variable containing the docstring

    self.src_line_first -- starting line of the function in self.src_file
    self.src_line_last -- final line of the function in self.src_file
    self.src_github_url -- URL of the function's code on GitHub
    self.doc_text -- text of the object's docstring
    self.doc_github_url -- URL of the docstring on the GitHub

    self.parsed_params -- list of parameters of this function; each entry is
                          either the parameter itself (str), or a tuple of
                          strings (parameter, default_value).
    """
    has_content = False
    optional_arguments = 2
    option_spec = {
        "src": directives.unchanged_required,
        "doc": directives.unchanged,
    }

    def run(self):
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

    def _parse_arguments(self):
        if len(self.arguments) != 1:
            raise self.error("Invalid %s directive: it requires a single "
                             "argument which is the name of the object being "
                             "documented" % self.name)
        fullname = self.arguments[0].strip()
        if not re.fullmatch(rx_py_id, fullname):
            raise self.error("Invalid argument to %s directive: must be a "
                             "valid python indentifier, instead got `%s`"
                             % (self.name, fullname))
        parts = fullname.rsplit('.', maxsplit=1)
        if len(parts) == 1:
            self.obj_name = parts[0]
            self.qualifier = module_name + '.'
        else:
            self.obj_name = parts[1]
            if parts[0].startswith(module_name):
                self.qualifier = parts[0] + '.'
            else:
                self.qualifier = module_name + '.' + parts[0] + '.'


    def _parse_option_src(self):
        src = self.options["src"].strip()
        parts = src.split()
        if len(parts) != 2:
            raise self.error("Invalid :src: option: it must have form "
                             "'filename fnname'")
        if not file_ok(parts[0]):
            raise self.error("Invalid :src: option: file `%s` does not exist"
                             % parts[0])
        if not re.fullmatch(rx_cc_id, parts[1]):
            raise self.error("Invalid :src: option: `%s` is not a valid C++ "
                             "identifier" % parts[1])
        self.src_file = parts[0]
        self.src_fnname = parts[1]


    def _parse_option_doc(self):
        doc = self.options.get("doc", "").strip()
        parts = doc.split()

        if len(parts) == 0:
            self.doc_file = self.src_file
        elif file_ok(parts[0]):
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

    def _locate_sources(self, filename, fnname, docname):
        full_filename = os.path.join(project_root, filename)
        with open(full_filename, "r", encoding="utf-8") as inp:
            lines = list(inp)
        if fnname:
            self._locate_fn_source(filename, fnname, lines)
        if docname:
            self._locate_doc_source(filename, docname, lines)


    def _locate_fn_source(self, filename, fnname, lines):
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
        self.src_github_url = src_permalink(filename, start_line, finish_line)


    def _locate_doc_source(self, filename, docname, lines):
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
        self.doc_github_url = src_permalink(filename, start_line, finish_line)


    #---------------------------------------------------------------------------
    # Processing: parse the docstring
    #---------------------------------------------------------------------------

    def _parse_docstring(self):
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
        signature = signature[(len(self.obj_name) + 1):-1]
        self._parse_parameters(signature)


    def _parse_parameters(self, sig):
        sig = sig.strip()
        logger.info("\nParsing signature: %r" % sig)
        i = 0  # Location within the string where we currently parse
        parsed = []
        while i < len(sig):
            mm = re.match(rx_param, sig[i:])
            # logger.info("%d: %r" % (i, mm))
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
        logger.info("Result = %r" % (parsed,))



    #---------------------------------------------------------------------------
    # Rendering into nodes
    #---------------------------------------------------------------------------

    def _generate_nodes(self):
        return self._generate_signature()


    def _generate_signature(self):
        sig_node = mydiv_node(classes=["sig-container"])
        sig_nodeL = mydiv_node(classes=["sig-body"])
        self._generate_sigbody(sig_nodeL)
        sig_node += sig_nodeL
        sig_nodeR = mydiv_node(classes=["code-links"])
        self._generate_siglinks(sig_nodeR)
        sig_node += sig_nodeR
        return [sig_node]


    def _generate_sigbody(self, node):
        row1 = mydiv_node(classes=["sig-qualifier"])
        row1 += a_node(href="", text=self.qualifier)
        row2 = mydiv_node(classes=["sig-main"])
        self._generate_sigmain(row2)
        node += row1
        node += row2


    def _generate_siglinks(self, node):
        node += a_node(href=self.src_github_url, text="source", new=True)
        if self.doc_file != self.src_file:
            node += a_node(href=self.doc_github_url, text="doc", new=True)


    def _generate_sigmain(self, node):
        div1 = mydiv_node(classes=["sig-name"])
        div1 += nodes.Text(self.obj_name)
        node += div1
        if self.name != "dtdata":
            node += mydiv_node(classes=["sig-open-paren"])
            params = mydiv_node(classes=["sig-parameters"])
            last_i = len(self.parsed_params) - 1
            for i, param in enumerate(self.parsed_params):
                classes = ["param"]
                if param == "self": classes += ["self"]
                if i == last_i: classes += ["final"]
                if isinstance(param, str):
                    p = mydiv_node(classes=classes)
                    p += nodes.Text(param)
                    params += p
                else:
                    assert isinstance(param, tuple)
                    p = mydiv_node(classes=classes)
                    p += nodes.Text(param[0])
                    deflt = mydiv_node(classes=["default"])
                    deflt += nodes.Text(param[1])
                    p += deflt
                    params += p
            params += mydiv_node(classes=["sig-close-paren"])
            node += params



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
    app.add_css_file("dtfunction.css")
    app.add_js_file("https://use.fontawesome.com/0f455d5fb2.js")
    app.add_directive("dtdata", DtobjectDirective)
    app.add_directive("dtfunction", DtobjectDirective)
    app.add_directive("dtmethod", DtobjectDirective)
    app.add_node(mydiv_node, html=(visit_div, depart_div))
    app.add_node(a_node, html=(visit_a, depart_a))
    return {"parallel_read_safe": True, "parallel_write_safe": True}
