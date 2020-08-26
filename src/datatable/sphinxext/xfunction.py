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
"""
Sphinx directive to generate documentation for functions/methods
without having to build the module from source.

In order to enable this extension you need it to add it into the
list of all extensions in the "conf.py" file:

    extensions = [
        ...,
        'datatable.sphinxext.xfunction'
    ]

This makes several directives available for use in the .rst files:

    .. xdata: datatable.math.tau

    .. xfunction: datatable.fread

    .. xmethod: datatable.Frame.cbind
        :src: c/frame/cbind.cc Frame::cbind
        :doc: c/frame/cbind.cc doc_cbind
        :tests: tests/frame/test-cbind.py

    .. xclass: datatable.Frame

"""
import pathlib
import re
from docutils import nodes
from docutils.parsers.rst import directives
from docutils.statemachine import StringList
from sphinx import addnodes
from sphinx.util import logging
from sphinx.util.docutils import SphinxDirective
from sphinx.util.nodes import make_refnode
from . import xnodes

logger = logging.getLogger(__name__)

rx_cc_id = re.compile(r"(?:\w+::)*\w+")
rx_py_id = re.compile(r"(?:\w+\.)*\w+")
rx_param = re.compile(r"(?:"
                      r"(\w+)(?:\s*=\s*("
                      r"\"[^\"]*\"|'[^']*'|\([^\(\)]*\)|\[[^\[\]]*\]|[^,\[\(\"]*"
                      r"))?"
                      r"|([\*/]|\*\*?\w+)"
                      r")\s*(?:,\s*|$)")
rx_header = re.compile(r"(\-{3,})\s*")
rx_return = re.compile(r"[\[\(]?returns?[\]\)]?")

# Build with pdb on exceptions:
#
#     sphinx-build -P -b html . _build/html
#

# Store title overrides for individual pages.
# This controls the HTML titles only, i.e. the <title/> element
# inside the <head/> section.
title_overrides = {}



#-------------------------------------------------------------------------------
# XobjectDirective
#-------------------------------------------------------------------------------

class XobjectDirective(SphinxDirective):
    """
    Fields inherited from base class
    --------------------------------
    self.arguments : List[str]
        List of (whitespace-separated) tokens that come after the
        directive name, for example if the source has ".. xmethod:: A B",
        then this variable will contain ['A', 'B']

    self.block_text : str
        Full text of the directive and its contents, as a single string.

    self.config : sphinx.config.Config
        Global configuration options.

    self.content : StringList
        The content of the directive, i.e. after the directive itself and
        all its options. This is a list of lines.

    self.content_offset : int
        Index of the first line of `self.content` within the source file.

    self.env : sphinx.environment.BuildEnvironment
        Stuff.

    self.name : str
        The name of the directive, such as "xfunction" or "xdata".

    self.options : Dict[str, str]
        Key-value store of all options passed to the directive.


    Fields derived from config/arguments/options
    --------------------------------------------
    self.module_name : str
        The name of the module being documented, in our case it's
        "datatable" (from config variable `xf_module_name`).

    self.project_root : pathlib.Path
        Location of the project's root folder, relative to the folder
        where the documentation is built. The paths specified by the
        `:src:` and `:doc:` options must be relative to this root
        directory (from config variable `xf_project_root`).

    self.permalink_url0 : str
        Pattern to be used for constructing URLs to a file. This string
        should contain a "{filename}" pattern inside. (from config variable
        `xf_permalink_url0`).

    self.permalink_url2 : str
        Pattern to be used for constructing URLs to a file. It is similar
        to `self.permalink_url0`, but also allows selecting a particular
        range of lines within the file. Should contain patterns "{line1}"
        and "{line2}" inside (from config variable `xf_permalink_url2`).

    self.obj_name : str
        Python name (short) of the object being documented.

    self.qualifier : str
        The object's qualifier, such that `qualifier + obj_name` would
        produce a fully-qualified object name.

    self.src_file : str
        Name of the file where the code is located.

    self.src_fnname : str
        Name of the C++ function (possibly with a namespace) which
        corresponds to the object being documented.

    self.src_fnname2 : str | None
        Name of the second C++ function (for setters).

    self.doc_file : str | None
        Name of the file where the docstring is located.

    self.doc_var : str | None
        Name of the C++ variable containing the docstring.

    self.test_file : str | None
        Name of the file where tests are located.

    self.setter : str | None
        Name of the setter variable (for :xdata: directives).


    Parsed from the source files(s)
    -------------------------------
    self.src_github_url : str
        GitHub URL for the function's code.

    self.src2_github_url : str
        GitHub URL for the function's code.

    self.doc_lines : StringList
        List of lines comprising the object's docstring.

    self.doc_github_url : str | None
        URL of the docstring on GitHub

    self.tests_github_url : str
        URL of the test file on GitHub


    Fields parsed from the docstring
    --------------------------------
    self.parsed_params : List[str | Tuple[str, str]]
        List of parameters of this function; each entry is either the
        parameter itself (str), or a tuple `(parameter, default_value)`.


    See also
    --------
    - sphinx/directives/__init__.py::ObjectDescription
    - sphinx/domains/python.py::PyObject

    """
    has_content = True
    required_arguments = 1
    optional_arguments = 0
    final_argument_whitespace = False
    option_spec = {
        "src": directives.unchanged_required,
        "doc": directives.unchanged,
        "tests": directives.unchanged,
        "settable": directives.unchanged,
        "deletable": directives.unchanged,
        "notitle": directives.unchanged,   # FIXME
    }

    def run(self):
        """
        Main function invoked by the Sphinx runtime.
        """
        self._parse_config()
        self._parse_arguments()
        self._parse_option_src()
        self._parse_option_doc()
        self._parse_option_tests()
        self._parse_option_settable()
        self._parse_option_deletable()
        self._register_title_override()

        if self.src_fnname:
            self.src_github_url = self._locate_fn_source(
                                    self.src_file, self.src_fnname)
        if self.src_fnname2:
            self.src2_github_url = self._locate_fn_source(
                                    self.src_file, self.src_fnname2)
        self._extract_docs_from_source()
        if self.content:
            self.doc_lines.extend(self.content)
        self._parse_docstring()

        return self._generate_nodes()


    def _register_title_override(self):
        """
        Normally HTML page's <title/> will be the same as the title
        displayed on the page in an <h1/> element.

        However, due to the fact that the amount of screen space
        available for a title is very small, we want to make the
        title "reversed": first the object name and then the
        qualifier. This way the most important information will
        remain visible longer.
        """
        if self.name == "xclass":
            title = self.obj_name
        elif self.name == "xdata":
            title = "." + self.obj_name
        else:
            title = "." + self.obj_name + "()"
        title += " &ndash; " + self.qualifier
        title_overrides[self.env.docname] = title


    #---------------------------------------------------------------------------
    # Initialization: parse input options
    #---------------------------------------------------------------------------

    def _parse_config(self):
        self.module_name = self.env.config.xf_module_name
        self.project_root = self.env.config.xf_project_root
        self.permalink_url0 = self.env.config.xf_permalink_url0
        self.permalink_url2 = self.env.config.xf_permalink_url2
        assert isinstance(self.module_name, str)
        assert isinstance(self.project_root, str)
        assert isinstance(self.permalink_url0, str)
        assert isinstance(self.permalink_url2, str)
        assert "{filename}" in self.permalink_url0
        assert "{filename}" in self.permalink_url2
        assert "{line1}" in self.permalink_url2
        assert "{line2}" in self.permalink_url2
        self.project_root = pathlib.Path(self.project_root)
        assert self.project_root.is_dir()


    def _parse_arguments(self):
        """
        Process `self.arguments`, verify their validity, and extract fields
        `self.obj_name` and `self.qualifier`.
        """
        assert len(self.arguments) == 1  # checked from required_arguments field
        fullname = self.arguments[0].strip()
        if not re.fullmatch(rx_py_id, fullname):
            raise self.error("Invalid argument to ..%s directive: must be a "
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
        if "src" not in self.options:
            raise self.error("Option :src: is required for ..%s directive"
                             % self.name)
        src = self.options["src"].strip()
        parts = src.split()
        if self.name == "xdata":
            if len(parts) not in [2, 3]:
                raise self.error("Invalid :src: option for ..%s directive: "
                                 "it must have form ':src: filename "
                                 "getter_name [setter_name]'" % self.name)
        else:
            if len(parts) != 2:
                raise self.error("Invalid :src: option for ..%s directive: "
                                 "it must have form ':src: filename %s_name'"
                                 % (self.name, self.name[1:]))

        src = parts[0]
        if not (self.project_root / src).is_file():
            raise self.error("Invalid :src: option in ..%s directive: file "
                             "`%s` does not exist" % (self.name, src))
        self.src_file = src
        for p in parts[1:]:
            if not re.fullmatch(rx_cc_id, p):
                raise self.error("Invalid :src: option in ..%s directive: "
                                 "`%s` is not a valid C++ identifier"
                                 % (self.name, p))
        self.src_fnname = parts[1]
        self.src_fnname2 = None
        if len(parts) == 3:
            self.src_fnname2 = parts[2]


    def _parse_option_doc(self):
        """
        Process the nonmandatory option `:doc:`, and extract fields
        `self.doc_file` and `self.doc_var`. If the option is not
        provided, the fields are set to None.
        """
        if "doc" not in self.options:
            self.doc_file = None
            self.doc_var = None
            return
        doc = self.options["doc"].strip()
        parts = doc.split()
        if len(parts) > 2:
            raise self.error("Invalid :doc: option in ..%s directive: it must "
                             "have form ':doc: filename [docname]'"
                             % self.name)

        if len(parts) == 0:
            self.doc_file = self.src_file
        elif (self.project_root / parts[0]).is_file():
            self.doc_file = parts[0]
        else:
            raise self.error("Invalid :doc: option in ..%s directive: file "
                             "`%s` does not exist" % (self.name, parts[0]))

        if len(parts) <= 1:
            self.doc_var = "doc_" + self.obj_name
        elif re.fullmatch(rx_cc_id, parts[1]):
            self.doc_var = parts[1]
        else:
            raise self.error("Invalid :doc: option in ..%s directive: `%s` "
                             "is not a valid C++ identifier"
                             % (self.name, parts[1]))


    def _parse_option_tests(self):
        """
        Process the optional option `:tests:`, and set the fields
        `self.test_file` and `self.tests_github_url`.
        """
        self.test_file = None
        self.tests_github_url = None
        if "tests" not in self.options:
            return

        testfile = self.options["tests"].strip()
        if not (self.project_root / testfile).is_file():
            raise self.error("Invalid :tests: option in ..%s directive: file "
                             "`%s` does not exist" % (self.name, testfile))
        self.test_file = testfile
        self.tests_github_url = self.permalink(testfile)


    def _parse_option_settable(self):
        """
        Process the option `:settable:`, which is available for
        `:xdata:` directives only, and indicates that the property
        being documented also supports a setter interface.
        """
        setter = self.options.get("settable", "").strip()

        if setter:
            if self.name != "xdata":
                raise self.error("Option :settable: is not valid for a ..%s "
                                 "directive" % self.name)
            self.setter = setter
        else:
            self.setter = None


    def _parse_option_deletable(self):
        self.deletable = "deletable" in self.options

        if self.deletable and self.name != "xdata":
            raise self.error("Option :deletable: is not valid for a ..%s "
                             "directive" % self.name)



    #---------------------------------------------------------------------------
    # Processing: retrieve docstring / function source
    #---------------------------------------------------------------------------

    def permalink(self, filename, line1=None, line2=None):
        pattern = self.permalink_url2 if line1 else \
                  self.permalink_url0
        return pattern.format(filename=filename, line1=line1, line2=line2)


    def _locate_fn_source(self, filename, fnname):
        """
        Find the body of the function `fnname` within the `lines` that
        were read from the file `filename`.

        If successful, this function returns a tuple of the line
        numbers of the start the end of the function.
        """
        txt = (self.project_root / filename).read_text(encoding="utf-8")
        lines = txt.splitlines()
        if self.name == "xclass":
            rx_cc_function = re.compile(r"(\s*)class (?:\w+::)*" + fnname + r"\s*")
        else:
            rx_cc_function = re.compile(r"(\s*)"
                                        r"(?:static\s+|inline\s+)*"
                                        r"(?:[\w:*&<> ]+)\s+" +
                                        fnname +
                                        r"\s*\(.*\)\s*" +
                                        r"(?:const\s*|noexcept\s*|override\s*)*" +
                                        r"\{\s*")
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
        return self.permalink(filename, start_line, finish_line)


    def _extract_docs_from_source(self):
        """
        Find the body of the function's docstring inside the file
        `self.doc_file`. The docstring is expected to be in the
        `const char* {self.doc_var}` variable. An error will be raised if
        the docstring cannot be found.
        """
        if not self.doc_file:
            self.doc_github_url = None
            self.doc_lines = StringList()
            return

        txt = (self.project_root / self.doc_file).read_text(encoding="utf-8")
        lines = txt.splitlines()
        rx_cc_docstring = re.compile(r"\s*(?:static\s+)?const\s+char\s*\*\s*" +
                                     self.doc_var +
                                     r"\s*=\s*(.*?)\s*")
        doc_lines = None
        line1 = None
        line2 = None
        skip_next_line = False
        for i, line in enumerate(lines):
            if skip_next_line:
                skip_next_line = False
            elif doc_lines:
                end_index = line.find(')";')
                if end_index == -1:
                    doc_lines.append(line)
                else:
                    doc_lines.append(line[:end_index])
                    line2 = i + 1
                    break
            elif self.doc_var in line:
                line1 = i + 1
                mm = re.fullmatch(rx_cc_docstring, line)
                if mm:
                    doc_start = mm.group(1)
                    if not doc_start:
                        doc_start = lines[i + 1].strip()
                        skip_next_line = True
                        line1 += 1
                    if doc_start.startswith('"'):
                        if not doc_start.endswith('";'):
                            raise self.error("Unexpected document string: `%s` "
                                             "in file %s line %d"
                                             % (doc_start, self.doc_file, line1))
                        doc_lines = [doc_start[1:-2]]
                        line2 = line1
                        break
                    elif doc_start.startswith('R"('):
                        doc_lines = [doc_start[3:]]
        if doc_lines is None:
            raise self.error("Could not find docstring `%s` in file `%s`"
                             % (self.doc_var, self.doc_file))
        if not line2:
            raise self.error("Docstring `%s` in file `%s` started on line %d "
                             "but did not finish"
                             % (self.doc_var, self.doc_file, line1))

        self.doc_github_url = self.permalink(self.doc_file, line1, line2)
        self.doc_lines = StringList(doc_lines,
                                    items=[(self.doc_file, line1 + i)
                                           for i in range(len(doc_lines))])


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
        if self.name in ["xdata", "xclass"] or "--" not in self.doc_lines:
            self.parsed_params = []
            self._parse_body(self.doc_lines)
            return

        iddash = self.doc_lines.index("--")
        signature = "\n".join(self.doc_lines.data[:iddash])
        if not (signature.startswith(self.obj_name + "(") and
                signature.endswith(")")):
            raise self.error("Unexpected docstring: should have started with "
                             "`%s(` and ended with `)`, instead found %r"
                             % (self.obj_name, signature))
        # strip objname and the parentheses
        signature = signature[(len(self.obj_name) + 1):-1]
        self._parse_parameters(signature)
        self._parse_body(self.doc_lines[iddash + 1:])


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


    def _parse_body(self, lines):
        """
        Parse/transform the body of the function's docstring.
        """
        body = "\n".join(lines.data)
        line0 = lines.offset(0) if body else 0  # offset of the first line
        self.parsed_body = self._split_into_sections(body, line0)
        for header, section, linenos in self.parsed_body:
            self._transform_codeblocks(section)
            if header == "Parameters":
                self._transform_parameters(section, linenos)
            if header == "Examples":
                self._transform_examples(section, linenos)


    def _split_into_sections(self, body, line0):
        body = body.strip()
        parsed = [[]]
        linenos = [[]]
        headers = [None]
        last_line = None
        for i, line in enumerate(body.split("\n")):
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
                linenos[-1].pop()
                parsed.append([])
                linenos.append([])
                headers.append(header)
                last_line = None
            else:
                parsed[-1].append(line)
                linenos[-1].append(i + line0)
                last_line = line
        return list(zip(headers, parsed, linenos))


    def _transform_codeblocks(self, lines):
        fnparams = []
        for p in self.parsed_params:
            if isinstance(p, str):
                # Regular parameter
                fnparams.append(p)
                if p.startswith("*"):
                    # A vararg/varkwd parameter can be referred to in two ways:
                    # either with or without starting *s.
                    fnparams.append(p.lstrip("*"))
            else:
                # Parameter with a default value
                fnparams.append(p[0])
        rx_codeblock = re.compile(
            r"``(.*?)``|"
            r"[:_]`([^`]+)`|"
            r"`([^`]+)`(?!_)"
        )
        def replacefn(match):
            txt3 = match.group(3)
            if match.group(1) or match.group(2):
                return match.group(0)
            elif txt3 in fnparams:
                return ":xparam-ref:`" + txt3 + "`"
            else:
                return "``" + txt3 + "``"

        for i, line in enumerate(lines):
            if '`' in line:
                lines[i] = re.sub(rx_codeblock, replacefn, line)


    def _transform_parameters(self, lines, linenos):
        i = 0
        while i < len(lines):
            line = lines[i]
            if line and line[0] != " ":
                lines[i] = ".. xparam:: " + line
                lines.insert(i + 1, "")
                linenos.insert(i + 1, None)
            i += 1


    def _transform_examples(self, lines, linenos):
        out = []
        i = 0
        while i < len(lines):
            line = lines[i]
            if line.lstrip().startswith(">>> "):
                i = self._parse_example_code(i, lines, out)
            else:
                out.append(line)
                i += 1
        lines[:] = out
        linenos[:] = list(range(len(out)))


    def _parse_example_code(self, i, lines, out):
        line = lines[i]
        indent = line[:len(line) - len(line.lstrip())]
        indent1 = indent + ">>> "
        indent2 = indent + "... "
        assert line.startswith(indent1)
        out.append(".. code-block:: python")
        out.append("")
        while i < len(lines):
            line = lines[i]
            i += 1
            if line == "":
                out.append(line)
                break
            if not line.startswith(indent):
                raise self.error("Unexpected indent in docstring `%s` in "
                                 "file %s" % (self.doc_var, self.doc_file))
            if line.startswith(indent1) or line.startswith(indent2):
                out.append("    " + line[len(indent1):])
            else:
                out.append("")
                i = self._parse_example_output(i - 1, lines, out, indent)
                break
        return i


    def _parse_example_output(self, i, lines, out, indent):
        out0 = len(out)
        out.append(".. code-block:: pycon")
        out.append("")
        while i < len(lines):
            line = lines[i]
            if line == "":
                break
            if not line.startswith(indent):
                raise self.error("Unexpected indent in docstring `%s` in "
                                 "file %s" % (self.doc_var, self.doc_file))
            shortline = line[len(indent):]
            if shortline.startswith(">>> "):
                break
            out.append("    " + shortline)
            i += 1
        if re.fullmatch(r"\s*\[\d+ rows? x \d+ columns?\]", out[-1]):
            frame_lines = [oline[4:] for oline in out[out0+2:]]
            out[out0:] = self._parse_dtframe(frame_lines)
        return i


    def _parse_dtframe(self, lines):
        assert len(lines) >= 4
        if lines[1][0] == "-":
            sep_line = 1
        elif lines[2][0] == "-":
            sep_line = 2
        else:
            raise self.error("Unrecognized format of dtframe")
        # Parse the line that separates the headers from the body of the table.
        # Runs of '-----'s will tell where the boundary of each column is.
        slices = [slice(*match.span())
                   for match in re.finditer(r"(\-+|\+|\.{3}|…)",
                                            lines[sep_line])]
        # Find the index of the "vertical separator" column
        vsep_index = [lines[sep_line][s] for s in slices].index('+')
        nkeys = 0
        if vsep_index > 1 or lines[0][slices[0]].strip():
            nkeys = vsep_index

        # Parse the column names
        row0 = [lines[0][s] for s in slices]
        assert row0[vsep_index] == '|'
        column_names = [name.strip() for name in row0[:nkeys] +
                                                 row0[vsep_index+1:]]

        # Parse the column types
        if sep_line == 2:
            row1 = [lines[1][s] for s in slices]
            assert row1[vsep_index] == '|'
            column_types = [typ.strip(" <>") for typ in row1[:nkeys] +
                                                        row1[vsep_index+1:]]
        else:
            column_types = ["int32"] * len(column_names)

        mm = re.fullmatch(r"\[(\d+) rows? x (\d+) columns?\]", lines[-1])
        nrows = mm.group(1)
        ncols = mm.group(2)
        out = []
        out.append(".. dtframe::")
        out.append("    :names: %r" % (column_names,))
        out.append("    :types: %r" % (column_types,))
        out.append("    :shape: (%s, %s)" % (nrows, ncols))
        out.append("    :nkeys: %d" % nkeys)
        out.append("")
        for line in lines[sep_line+1:-2]:
            rowi = [line[s] for s in slices]
            assert rowi[vsep_index] == '|'
            del rowi[vsep_index]
            out.append("    " + ",".join(x.strip() for x in rowi))
        return out



    #---------------------------------------------------------------------------
    # Rendering into nodes
    #---------------------------------------------------------------------------

    def _generate_nodes(self):
        title_text = self.qualifier + self.obj_name
        sect = nodes.section(ids=[title_text],
                             classes=["x-function", self.name])
        if "notitle" not in self.options:
            h1_text = title_text
            if self.name not in ["xdata", "xclass"]:
                h1_text += "()"
            sect += nodes.title("", h1_text)
        sect += self._index_node()
        sect += self._generate_signature()
        sect += self._generate_body()
        return [sect]


    def _index_node(self):
        targetname = self.qualifier + self.obj_name
        text = self.obj_name
        if self.name == "xmethod":
            text += " (%s method)" % self.qualifier[:-1]
        if self.name == "xfunction":
            text += " (%s function)" % self.qualifier[:-1]
        if self.name == "xdata":
            text += " (%s attribute)" % self.qualifier[:-1]
        inode = addnodes.index(entries=[("single", text, targetname, "", None)])
        return [inode]


    def _generate_signature(self):
        targetname = self.qualifier + self.obj_name
        sig_node = xnodes.div(classes=["sig-container"], ids=[targetname])
        sig_nodeL = xnodes.div(classes=["sig-body"])
        if self.name == "xclass":
            sig_nodeL += self._generate_signature_class()
        else:
            self._generate_sigbody(sig_nodeL, "normal")
        sig_node += sig_nodeL
        sig_nodeR = xnodes.div(classes=["code-links"])
        self._generate_siglinks(sig_nodeR)
        sig_node += sig_nodeR

        # Tell Sphinx that this is a target for `:py:obj:` references
        self.state.document.note_explicit_target(sig_node)
        domain = self.env.get_domain("py")
        domain.note_object(name=targetname,        # e.g. "datatable.Frame.cbind"
                           objtype=self.name[1:],  # remove initial 'x'
                           node_id=targetname)
        out = [sig_node]
        if self.name == "xdata":
            if self.setter:
                out += self._generate_signature_setter()
            if self.deletable:
                out += self._generate_signature_deleter()
        return out


    def _generate_signature_setter(self):
        assert self.setter
        sig_node = xnodes.div(classes=["sig-container"])
        sig_nodeL = xnodes.div(classes=["sig-body"])
        self._generate_sigbody(sig_nodeL, "setter")
        sig_node += sig_nodeL
        sig_nodeR = xnodes.div(classes=["code-links"])
        if self.src_fnname2:
            sig_nodeR += a_node(href=self.src2_github_url, text="source", new=True)
        sig_node += sig_nodeR
        return [sig_node]


    def _generate_signature_deleter(self):
        assert self.deletable
        sig_node = xnodes.div(classes=["sig-container"])
        body = xnodes.div(classes=["sig-body", "sig-main"])
        body += xnodes.div(nodes.Text("del "), classes=["keyword"])
        body += self._generate_qualifier()
        body += xnodes.div(nodes.Text(self.obj_name), classes=["sig-name"])
        sig_node += body
        return [sig_node]


    def _generate_signature_class(self):
        assert self.name == "xclass"
        body = xnodes.div(classes=["sig-main"])
        body += xnodes.div(nodes.Text("class "), classes=["keyword"])
        body += self._generate_qualifier()
        body += xnodes.div(nodes.Text(self.obj_name), classes=["sig-name"])
        return body


    def _generate_sigbody(self, node, kind):
        # When describing a constructor, it is more helpful to a user
        # to present it in the form of actual object construction,
        # i.e. show
        #
        #         Frame(...args)
        #
        # instead of
        #
        #         Frame.__init__(self, ...args)
        #
        if self.obj_name == "__init__":
            qual_parts = self.qualifier.split(".")
            assert qual_parts[-1] == ""
            self.obj_name = qual_parts[-2]
            self.qualifier = ".".join(qual_parts[:-2]) + "."
            assert self.parsed_params[0] == "self"
            del self.parsed_params[0]

        node += self._generate_qualifier()
        row2 = xnodes.div(classes=["sig-main"])
        self._generate_sigmain(row2, kind)
        node += row2


    def _generate_qualifier(self):
        node = xnodes.div(classes=["sig-qualifier"])
        ref = addnodes.pending_xref("", nodes.Text(self.qualifier),
                                    reftarget=self.qualifier[:-1],
                                    reftype="class", refdomain="py")
        # Note: `ref` cannot be added directly: docutils requires that
        # <reference> nodes were nested inside <TextElement> nodes.
        node += nodes.generated("", "", ref)
        return node

    def _generate_siglinks(self, node):
        node += a_node(href=self.src_github_url, text="source", new=True)
        if self.doc_github_url and self.doc_file != self.src_file:
            node += a_node(href=self.doc_github_url, text="doc", new=True)
        if self.tests_github_url:
            node += a_node(href=self.tests_github_url, text="tests", new=True)


    def _generate_sigmain(self, node, kind):
        div1 = xnodes.div(classes=["sig-name"])
        div1 += nodes.Text(self.obj_name)
        node += div1
        if self.name == "xdata":
            if kind == "setter":
                equal_sign_node = nodes.inline("", nodes.Text(" = "), classes=["punct"])
                param_node = xnodes.div(
                    a_node(text=self.setter, href="#" + self.setter),
                    classes=["param"]
                )
                params = xnodes.div(classes=["sig-parameters"],
                                    children=[equal_sign_node, param_node])
                node += params
        else:
            node += nodes.inline("", nodes.Text("("),
                                 classes=["sig-open-paren"])
            params = xnodes.div(classes=["sig-parameters"])
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
                        ref = a_node(text=param, href="#" + param.lstrip("*"))
                    params += xnodes.div(ref, classes=classes)
                else:
                    assert isinstance(param, tuple)
                    param_node = a_node(text=param[0], href="#" + param[0])
                    equal_sign_node = nodes.inline("", nodes.Text("="), classes=["punct"])
                    # Add as nodes.literal, so that Sphinx wouldn't try to
                    # "improve" quotation marks and ...s
                    default_value_node = nodes.literal("", nodes.Text(param[1]),
                                                      classes=["default"])
                    params += xnodes.div(classes=classes, children=[
                                            param_node,
                                            equal_sign_node,
                                            default_value_node
                                         ])
                if i < len(self.parsed_params) - 1:
                    params += nodes.inline("", nodes.Text(", "), classes=["punct"])
            params += nodes.inline("", nodes.Text(")"),
                                   classes=["sig-close-paren"])
            node += params


    def _generate_body(self):
        out = xnodes.div(classes=["x-function-body"])
        for head, lines, linenos in self.parsed_body:
            if head:
                lines = [head, "="*len(head), ""] + lines
                i0 = linenos[0]
                linenos = [i0-3, i0-2, i0-1] + linenos
            assert len(lines) == len(linenos)
            content = StringList(lines,
                                 items=[(self.doc_file, i) for i in linenos])
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
        id0 = self.param.strip("*/()[]")
        root = xnodes.div(classes=["xparam-box"], ids=[id0])
        head = xnodes.div(classes=["xparam-head"])
        if id0 in ["return", "except"]:
            param_node = xnodes.div(classes=["param", id0])
            param_node += nodes.Text(id0)
        else:
            param_node = xnodes.div(classes=["param"])
            param_node += nodes.Text(self.param)
        head += param_node
        types_node = xnodes.div(classes=["types"])
        types_str = " | ".join("``%s``" % p for p in self.types)
        self.state.nested_parse(StringList([types_str]), self.content_offset,
                                types_node)
        assert isinstance(types_node[0], nodes.paragraph)
        types_node.children = types_node[0].children
        head += types_node
        root += head
        desc_node = xnodes.div(classes=["xparam-body"])
        self.state.nested_parse(self.content, self.content_offset, desc_node)
        root += desc_node
        return [root]


    def _parse_arguments(self):
        self.param = self.arguments[0].rstrip(":")
        self.types = []

        rx_separator = re.compile(r"(?:,?\s+or\s+|\s*[,\|]\s*)")
        args = self.arguments[1]
        i0 = 0
        bracket_level = 0
        i = 0
        while i < len(args):  # iterate by characters
            if args[i] in "[({'\"":
                bracket_level += 1
            elif args[i] in "\"'})]":
                bracket_level -= 1
            elif bracket_level == 0:
                mm = re.match(rx_separator, args[i:])
                if mm:
                    self.types.append(args[i0:i])
                    i += mm.end()
                    i0 = i
                    continue
            i += 1
        assert i == len(args)
        self.types.append(args[i0:])




#-------------------------------------------------------------------------------
# XversionaddedDirective
#-------------------------------------------------------------------------------

class XversionaddedDirective(SphinxDirective):
    has_content = False
    required_arguments = 1
    optional_arguments = 0
    final_argument_whitespace = False

    def run(self):
        version = self.arguments[0].strip()

        node = xnodes.div(classes=["x-version-added"])
        node += nodes.Text("New in version ")
        node += nodes.inline("", "",
            addnodes.pending_xref("", nodes.Text(version),
                refdomain="std", reftype="doc", refexplicit=True,
                reftarget="/releases/"+version))
        return [node]



#-------------------------------------------------------------------------------
# :xparam-ref: role
#-------------------------------------------------------------------------------

def xparamref(name, rawtext, text, lineno, inliner, options={}, content=[]):
    node = a_node(href="#" + text.lstrip("*"), classes=["xparam-ref"])
    node += nodes.literal("", "", nodes.Text(text))
    return [node], []




#-------------------------------------------------------------------------------
# Nodes
#-------------------------------------------------------------------------------

class a_node(nodes.Element, nodes.Inline):
    def __init__(self, href, text="", new=False, **attrs):
        super().__init__(**attrs)
        self.href = href
        self.new = new
        if text:
            self += nodes.Text(text)

def visit_a(self, node):
    attrs = {"href": node.href}
    if node.new:
        attrs["target"] = "_new"
    self.body.append(self.starttag(node, "a", **attrs).strip())

def depart_a(self, node):
    self.body.append("</a>")



#-------------------------------------------------------------------------------
# Setup
#-------------------------------------------------------------------------------
def fix_html_titles(app, pagename, templatename, context, doctree):
    if pagename in title_overrides:
        context["title"] = title_overrides[pagename]


def setup(app):
    app.setup_extension("sphinxext.xnodes")
    app.add_config_value("xf_module_name", None, "env")
    app.add_config_value("xf_project_root", "..", "env")
    app.add_config_value("xf_permalink_url0", "", "env")
    app.add_config_value("xf_permalink_url2", "", "env")
    app.add_css_file("xfunction.css")
    app.add_js_file("https://use.fontawesome.com/0f455d5fb2.js")
    app.add_directive("xdata", XobjectDirective)
    app.add_directive("xfunction", XobjectDirective)
    app.add_directive("xmethod", XobjectDirective)
    app.add_directive("xclass", XobjectDirective)
    app.add_directive("xparam", XparamDirective)
    app.add_directive("xversionadded", XversionaddedDirective)
    app.add_node(a_node, html=(visit_a, depart_a))
    app.add_role("xparam-ref", xparamref)
    app.connect("html-page-context", fix_html_titles)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
