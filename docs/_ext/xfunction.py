#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2019-2021 H2O.ai
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
        :tests: tests/frame/test-cbind.py
        :cvar: doc_Frame_cbind
        :signature: cbind(*frames)

        content...

    .. xclass: datatable.Frame

The :src: option is required, but it may be "--" to indicate that the object
has no identifiable source (use sparingly!).

The :cvar: option has no effect on the output, but it helps auto-generator
to find which documentation belongs where.
"""
import pathlib
import re
import time
from docutils import nodes
from docutils.parsers.rst import directives
from docutils.statemachine import StringList
from sphinx import addnodes
from sphinx.util import logging
from sphinx.util.docutils import SphinxDirective
from sphinx.util.nodes import make_refnode
from . import xnodes

rx_cc_id = re.compile(r"(?:\w+(?:<\w+(,\w+)*>)?::)*\w+")
rx_py_id = re.compile(r"(?:\w+\.)*\w+")
rx_param = re.compile(r"""
    (?:(\w+)                                        # parameter name
       (?:\s*=\s*(\"[^\"]*\"|'[^']*'|\([^\(\)]*\)|
                  \[[^\[\]]*\]|[^,\[\(\"]*))?       # default value
       |(\*\*?\w*|/|\.\.\.)                         # varags/varkwds
    )\s*(?:,\s*)?                                   # followed by a comma
    |                                               # - or -
    \[,?\s*(\w+),?\s*\]\s*                          # a parameter in square brackets
    """, re.VERBOSE)
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
    The following data is stored by this directive in the environment:

        env.xobject = {
            <docname>: {
                "timestamp": float,    # time of last build of the page
                "sources": List[str],  # names of source files
            }
        }

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
        "noindex": directives.unchanged,
        "qual-type": directives.unchanged,
        "cvar": directives.unchanged,
        "signature": directives.unchanged,
    }

    def __init__(self, *args):
        super().__init__(*args)
        self.src_file = None
        self.src_fnname = None
        self.src_fnname2 = None
        self.src_github_url = None
        self.doc_file = None
        self.doc_var = None
        self.doc_github_url = None
        self.doc_lines = StringList()
        self.test_file = None
        self.tests_github_url = None
        self.qualifier = None
        self.qualifier_reftype = None
        self.signature = None


    def run(self):
        """
        Main function invoked by the Sphinx runtime.
        """
        self._parse_config()
        self._parse_arguments()
        self._init_env()
        self._parse_option_src()
        self._parse_option_doc()
        self._parse_option_tests()
        self._parse_option_settable()
        self._parse_option_deletable()
        self._parse_option_qualtype()
        self._parse_option_signature()
        self._register_title_override()

        if self.src_fnname:
            self.src_github_url = self._locate_fn_source(
                                    self.src_file, self.src_fnname)
        if self.src_fnname2:
            self.src2_github_url = self._locate_fn_source(
                                    self.src_file, self.src_fnname2)
        if not self.signature:
            self._extract_docs_from_source()
            if self.content:
                self.doc_lines.extend(self.content)
        self._parse_docstring()

        return self._generate_nodes()


    def _init_env(self):
        if not hasattr(self.env, "xobject"):
            self.env.xobject = {}
        self.env.xobject[self.env.docname] = dict(
            timestamp = time.time(),
            sources = []
        )
        xpy = self.env.get_domain("xpy")
        if self.name in ["xclass", "xexc"]:
            xpy.current_context = ("class", self.qualifier + self.obj_name)
        if self.name in ["xattr", "xmethod"]:
            assert self.qualifier[-1:] == '.'
            xpy.current_context = ("class", self.qualifier[:-1])
        if self.name in ["xfunction", "xdata"]:
            assert self.qualifier[-1:] == '.'
            xpy.current_context = ("module", self.qualifier[:-1])


    def _register_source_file(self, filename):
        self.env.xobject[self.env.docname]['sources'].append(filename)


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
        if self.name in ["xclass", "xexc", "xdata", "xfunction"]:
            title = self.obj_name
        elif self.name == "xattr":
            title = "." + self.obj_name
        elif self.name == "xmethod":
            title = "." + self.obj_name + "()"
        else:
            self.error("Unknown directive " + self.name)
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
            raise self.error("Option :src: is required for ..%s directive. "
                             "If the object's source cannot be reasonably "
                             "given, write `:src: --`."
                             % self.name)
        src = self.options["src"].strip()
        if src == '--':
            return

        parts = src.split()
        if self.name in ["xdata", "xattr"]:
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
        self._register_source_file(self.src_file)
        for p in parts[1:]:
            if not re.fullmatch(rx_cc_id, p):
                raise self.error("Invalid :src: option in ..%s directive: "
                                 "`%s` is not a valid C++ identifier"
                                 % (self.name, p))
        self.src_fnname = parts[1]
        if len(parts) == 3:
            self.src_fnname2 = parts[2]


    def _parse_option_doc(self):
        """
        Process the nonmandatory option `:doc:`, and extract fields
        `self.doc_file` and `self.doc_var`. If the option is not
        provided, the fields are set to `None`.
        """
        if "doc" not in self.options:
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
            self._register_source_file(self.doc_file)
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
        if "tests" not in self.options:
            return

        testfile = self.options["tests"].strip()
        if not (self.project_root / testfile).is_file():
            raise self.error("Invalid :tests: option in ..%s directive: file "
                             "`%s` does not exist" % (self.name, testfile))
        self.test_file = testfile
        self.tests_github_url = self.permalink(testfile)
        self._register_source_file(self.test_file)


    def _parse_option_settable(self):
        """
        Process the option `:settable:`, which is available for
        `:xattr:` directives only, and indicates that the property
        being documented also supports a setter interface.
        """
        setter = self.options.get("settable", "").strip()

        if setter:
            if not(self.name == "xattr" or self.obj_name == "__setitem__"):
                raise self.error("Option :settable: is not valid for a ..%s "
                                 "directive" % self.name)
            self.setter = setter
        else:
            self.setter = None


    def _parse_option_deletable(self):
        self.deletable = "deletable" in self.options

        if self.deletable and self.name != "xattr":
            raise self.error("Option :deletable: is not valid for a ..%s "
                             "directive" % self.name)


    def _parse_option_qualtype(self):
        reftype = self.options.get("qual-type", "").strip()
        if not reftype:
            reftype = "class" if self.name in ["xmethod", "xattr"] else "module"
        self.qualifier_reftype = reftype


    def _parse_option_signature(self):
        if "signature" not in self.options:
            return
        if "doc" in self.options:
            raise self.error("Option :signature: cannot be used together with "
                             "option :doc:")
        self.signature = self.options['signature'].strip()



    #---------------------------------------------------------------------------
    # Processing: retrieve docstring / function source
    #---------------------------------------------------------------------------

    def permalink(self, filename, line1=None, line2=None):
        pattern = self.permalink_url2 if line1 else \
                  self.permalink_url0
        return pattern.format(filename=filename, line1=line1, line2=line2)


    def _locate_fn_source(self, filename, fnname):
        """
        Find the body of the function `fnname` within the file `filename`.

        If successful, this function returns the URL for the located
        function.
        """
        txt = (self.project_root / filename).read_text(encoding="utf-8")
        lines = txt.splitlines()
        lines = StringList(lines, items=[(filename, i+1)
                                         for i in range(len(lines))])
        try:
            kind = self.name[1:]  # remove initial 'x'
            if filename.endswith(".py"):
                if kind == "data":
                    i, j = locate_python_variable(fnname, lines)
                elif kind in ["class", "function", "method", "attr"]:
                    i, j = locate_python_function(fnname, kind, lines)
                    self.doc_lines = extract_python_docstring(lines[i:j])
                else:
                    raise self.error("Unsupported directive %s for "
                                     "a python source file" % self.name)
            else:
                if kind not in ["class", "function", "method", "attr"]:
                    raise self.error("Unsupported directive %s for "
                                     "a C++ source file" % self.name)
                i, j = locate_cxx_function(fnname, kind, lines)

            return self.permalink(filename, i + 1, j)

        except ValueError as e:
            msg = str(e).replace("<FILE>", "file " + filename)
            raise self.error(msg) from None




    def _extract_docs_from_source(self):
        """
        Find the body of the function's docstring inside the file
        `self.doc_file`. The docstring is expected to be in the
        `const char* {self.doc_var}` variable. An error will be raised if
        the docstring cannot be found.
        """
        if not self.doc_file:
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
        if self.signature:
            signature = self.signature
            body = self.content

        else:
            if (self.name in ["xdata", "xattr", "xclass", "xexc"] or
                    "--" not in self.doc_lines):
                self.parsed_params = []
                self._parse_body(self.doc_lines)
                return

            iddash = self.doc_lines.index("--")
            signature = "\n".join(self.doc_lines.data[:iddash])
            body = self.doc_lines[iddash + 1:]

        if not (signature.startswith(self.obj_name + "(") and
                signature.endswith(")")):
            raise self.error("Unexpected docstring: should have started with "
                             "`%s(` and ended with `)`, instead found %r"
                             % (self.obj_name, signature))
        # strip objname and the parentheses
        signature = signature[(len(self.obj_name) + 1):-1]
        self._parse_parameters(signature)
        self._parse_body(body)


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
          - (varname,)          # optional parameter (in square brackets)
          - "/"                 # separator for pos-only arguments
          - "*"                 # separator for kw-only arguments
          - "*" + varname       # positional varargs
          - "**" + varname      # keyword varargs
          - "..."               # the ellipsis

        Type annotations in the signature are not supported.
        """
        sig = sig.strip()
        i = 0  # Location within the string where we currently parse
        parsed = []
        while i < len(sig):
            mm = re.match(rx_param, sig[i:])
            if mm:
                if mm.group(3) is not None:   # "special" variable
                    parsed.append(mm.group(3))
                elif mm.group(4) is not None:
                    parsed.append((mm.group(4),))
                else:
                    assert mm.group(1) is not None
                    if mm.group(2) is None:  # without default
                        parsed.append(mm.group(1))
                    else:
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
        # TODO: work with the StringList `lines` directly without converting
        #       it into a plain string
        body = "\n".join(lines.data)
        line0 = lines.offset(0) if body else 0  # offset of the first line
        self.parsed_body = self._split_into_sections(body, line0)
        for header, section, linenos in self.parsed_body:
            self._transform_codeblocks(section)
            if header == "Parameters":
                self._transform_parameters(section, linenos)
            # if header == "Examples":
            #     self._transform_examples(section, linenos)


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
                while lines[i].endswith("\\"):
                    lines[i] = lines[i][:-1] + lines[i + 1]
                    del lines[i + 1]
                    del linenos[i + 1]
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
        return i


    #---------------------------------------------------------------------------
    # Rendering into nodes
    #---------------------------------------------------------------------------

    def _generate_nodes(self):
        sect = xnodes.div(classes=["x-function", self.name])
        sect += self._index_node()
        sect += self._generate_signature()
        sect += self._generate_body()
        return [sect]


    def _index_node(self):
        if "noindex" in self.options:
            return []
        targetname = self.qualifier + self.obj_name
        text = self.obj_name
        if self.name == "xmethod":
            text += " (%s method)" % self.qualifier[:-1]
        if self.name == "xfunction":
            text += " (%s function)" % self.qualifier[:-1]
        if self.name == "xattr":
            text += " (%s attribute)" % self.qualifier[:-1]
        inode = addnodes.index(entries=[("single", text, targetname, "", None)])
        return [inode]


    def _generate_signature(self):
        targetname = self.qualifier + self.obj_name
        sig_node = xnodes.div(classes=["sig-container"], ids=[targetname])
        sig_nodeL = xnodes.div(classes=["sig-body"])
        if self.name in ["xclass", "xexc"]:
            sig_nodeL += self._generate_signature_class()
        else:
            self._generate_sigbody(sig_nodeL, "normal")
        sig_node += sig_nodeL
        sig_nodeR = xnodes.div(classes=["code-links"])
        self._generate_siglinks(sig_nodeR)
        sig_node += sig_nodeR

        # Tell Sphinx that this is a target for `:py:obj:` references
        if "noindex" not in self.options:
            self.state.document.note_explicit_target(sig_node)
            domain = self.env.get_domain("xpy")
            domain.note_object(objtype=self.name[1:],  # remove initial 'x'
                               objname=targetname,     # e.g. "datatable.Frame.cbind"
                               docname=self.env.docname)
        out = [sig_node]
        if self.name == "xattr":
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
        assert self.name == "xclass" or self.name == "xexc"
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
            self.qualifier_reftype = "module"
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
                                    reftype=self.qualifier_reftype, refdomain="xpy")
        # Note: `ref` cannot be added directly: docutils requires that
        # <reference> nodes were nested inside <TextElement> nodes.
        node += nodes.generated("", "", ref)
        return node

    def _generate_siglinks(self, node):
        if self.src_github_url:
            node += a_node(href=self.src_github_url, text="source", new=True)
        if self.doc_github_url and self.doc_file != self.src_file:
            node += a_node(href=self.doc_github_url, text="doc", new=True)
        if self.tests_github_url:
            node += a_node(href=self.tests_github_url, text="tests", new=True)


    def _generate_sigmain(self, node, kind):
        square_bracket_functions = ['__getitem__', '__setitem__', '__delitem__']

        if self.obj_name in square_bracket_functions:
            if self.obj_name == "__delitem__":
                node += xnodes.div(nodes.Text("del "), classes=["keyword"])
            node += xnodes.div(nodes.Text("self"), classes=["self", "param"])
        else:
            node += xnodes.div(nodes.Text(self.obj_name), classes=["sig-name"])

        if self.name in ["xdata", "xattr"]:
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
            if self.obj_name in square_bracket_functions:
                node += xnodes.div(nodes.Text("["), classes=["sig-name"])
            else:
                node += nodes.inline("", nodes.Text("("),
                                     classes=["sig-open-paren"])
            params = xnodes.div(classes=["sig-parameters"])
            last_i = len(self.parsed_params) - 1

            # `comma` will be rendered in the beginning on the corresponding
            # parameter, because for an optional `param` it has to go inside
            # the square brackets.
            comma = None

            for i, param in enumerate(self.parsed_params):
                classes = ["param"]

                if param == "self": continue

                if i == last_i:
                    classes += ["final"]

                if isinstance(param, str):
                    if param in ["*", "/", "..."]:
                        classes += ["special"]
                        ref = nodes.Text(param)
                    else:
                        ref = a_node(text=param, href="#" + param.lstrip("*"))

                    # Here and below `comma` is rendered in a separate div,
                    # so that for multiline signatures a new line has less
                    # chances to start with ",".
                    params += xnodes.div(children=[comma], classes=classes)
                    params += xnodes.div(children=[ref], classes=classes)
                else:
                    assert isinstance(param, tuple)
                    if len(param) == 2:
                        param_node = a_node(text=param[0], href="#" + param[0])
                        equal_sign_node = nodes.inline("", nodes.Text("="), classes=["punct"])
                        # Add as nodes.literal, so that Sphinx wouldn't try to
                        # "improve" quotation marks and ...s
                        default_value_node = nodes.literal("", nodes.Text(param[1]),
                                                          classes=["default"])
                        params += xnodes.div(classes=classes, children=[comma])
                        params += xnodes.div(classes=classes, children=[
                                    param_node,
                                    equal_sign_node,
                                    default_value_node,
                                  ])
                    else:
                        assert len(param) == 1
                        params += xnodes.div(classes=classes, children=[
                                    nodes.inline("", nodes.Text("["), classes=["punct"]),
                                    comma,
                                    a_node(text=param[0], href="#" + param[0]),
                                    nodes.inline("", nodes.Text("]"), classes=["punct"])
                                  ])
                comma = nodes.inline("", nodes.Text(", "), classes=["punct"])

            if self.obj_name in square_bracket_functions:
                params += nodes.inline("", nodes.Text(']'), classes=["sig-name"])
            else:
                params += nodes.inline("", nodes.Text(")"),
                                       classes=["sig-close-paren"])
            if self.obj_name == "__setitem__":
                if not getattr(self, "setter"):
                    self.setter = "values"
                params += nodes.inline("", nodes.Text(" = "), classes=["punct"])
                params += xnodes.div(
                            a_node(text=self.setter, href="#" + self.setter),
                            classes=["param"])

            node += params


    def _generate_body(self):
        out = xnodes.div(classes=["x-function-body", "section"])
        for head, lines, linenos in self.parsed_body:
            if head:
                lines = [head, "-"*len(head), ""] + lines
                i0 = linenos[0]
                linenos = [i0-3, i0-2, i0-1] + linenos
            assert len(lines) == len(linenos)
            content = StringList(lines,
                                 items=[(self.doc_file, i) for i in linenos])
            self.state.nested_parse(content, self.content_offset, out,
                                    match_titles=True)
        return out



#-------------------------------------------------------------------------------
# Helper functions
#-------------------------------------------------------------------------------

def locate_python_variable(name, lines):
    """
    Find declaration of a variable `name` within python source `lines`.

    Returns a tuple (istart, iend) such that `lines[istart:iend]` would
    contain the lines where the variable is declared. Variable declaration
    must have the form of an assignment, i.e. "{name} = ...". Other ways
    of declaring a variable are not supported.

    A ValueError is raised if variable declaration cannot be found.

    NYI: detection of multi-line variable declarations, such as dicts,
    lists, multi-line strings, etc.
    """
    assert isinstance(name, str)
    rx = re.compile(r"\s*%s\s*=\s*\S" % name)
    for i, line in enumerate(lines.data):
        if re.match(rx, line):
            return (i, i+1)
    raise ValueError("Could not find variable `%s` in <FILE>" % name)



def locate_python_function(name, kind, lines):
    """
    Find declaration of a class/function `name` in python source `lines`.

    Returns a tuple (istart, iend) such that `lines[istart:iend]` would
    contain the lines where the class/function is declared. The body of
    the class/function starts with "(class|def) {name}..." and ends
    at a line with the indentation level equal or smaller than the
    level of the declaration start.

    A ValueError is raised if variable declaration cannot be found.
    """
    assert kind in ["class", "function", "method", "attr"]
    keyword = "class" if kind == "class" else "def"
    rx_start = re.compile(r"\s*%s\s+%s\b" % (keyword, name))
    indent = None
    istart = None
    iend = -1
    for i, line in enumerate(lines.data):
        unindented_line = line.lstrip()
        if not unindented_line:  # skip blank lines
            continue
        line_indent_level = len(line) - len(unindented_line)
        if istart is None:
            if re.match(rx_start, line):
                indent = line_indent_level
                istart = i
        else:
            if line_indent_level <= indent:
                break
        iend = i

    iend += 1
    if istart is None:
        raise ValueError("Could not find %s `%s` in <FILE>" % (kind, name))

    # Also include @decorations, if any
    while istart > 0:
        line = lines[istart - 1]
        unindented_line = line.lstrip()
        line_indent_level = len(line) - len(unindented_line)
        if line_indent_level == indent and unindented_line[:1] == '@':
            istart -= 1
        else:
            break

    return (istart, iend)


def locate_cxx_function(name, kind, lines):
    if kind == "class":
        rx_start = re.compile(r"(\s*)class (?:\w+::)*" + name + r"\s*")
    else:
        rx_start = re.compile(r"(\s*)"
                              r"(?:static\s+|inline\s+)*"
                              r"(?:[\w:*&<> ]+)\s+" +
                              r"(?:\w+::)*" + name +
                              r"\s*\(.*\)\s*" +
                              r"(?:const\s*|noexcept\s*|override\s*)*" +
                              r"\{\s*")
    n_signature_lines = 5 # number of lines allowed for the function signature
    expect_closing = None
    istart = None
    ifinish = None
    for i, line in enumerate(lines.data):
        if expect_closing:
            if line.startswith(expect_closing):
                ifinish = i + 1
                break
        elif name in line:
            istart = i
            mm = re.match(rx_start, line)
            if mm:
                expect_closing = mm.group(1) + "}"
            else:
                src = "".join(lines[i:i+n_signature_lines])
                mm = re.match(rx_start, src)
                if mm:
                    expect_closing = mm.group(1) + "}"

    if not istart:
        raise ValueError("Could not find %s `%s` in <FILE>" % (kind, name))
    if not expect_closing:
        raise ValueError("Unexpected signature of %s `%s` in <FILE> "
                         "line %d" % (kind, name, istart))
    if not ifinish:
        raise ValueError("Could not locate the end of %s `%s` in <FILE> "
                         "line %d" % (kind, name, istart))
    return (istart, ifinish)


def extract_python_docstring(lines):
    """
    Given a list of lines that contain a class/function definition,
    this function will find the docstring (if any) within those lines
    and return it dedented. The return value is a StringList.

    If there is no docstring, return empty StringList.
    """
    i = 0
    while i < len(lines):  # skip decorators
        uline = lines[i].lstrip()
        if uline.startswith("@"):
            i += 1
        else:
            break
    mm = re.match(r"(class|def)\s+(\w+)\s*", uline)
    if not mm:
        raise ValueError("Unexpected function/class declaration: %r" % uline)
    name = mm.group(2)
    uline = uline[mm.end():]

    if mm.group(1) == "class" and uline == ":":
        i += 1
    elif uline.startswith("("):
        while i + 1 < len(lines):
            i += 1
            if uline.endswith("):"):
                break
            else:
                uline = lines[i]
    else:
        raise ValueError("Unexpected function/class declaration: %r" % lines[i])

    uline = lines[i].lstrip()
    indent0 = len(lines[i]) - len(uline)
    if uline.startswith('"""'):
        end = '"""'
    elif uline.startswith("'''"):
        end = "'''"
    else:
        return StringList()

    res_data = []
    res_items = []
    if len(uline) > 3:
        res_data.append(uline[3:])
        res_items.append(lines.info(i))
    while i + 1 < len(lines):
        i += 1
        line = lines[i][indent0:]
        if end in line:
            line = line[:line.find(end)].strip()
            if line:
                res_data.append(line)
                res_items.append(lines.info(i))
            break
        else:
            res_data.append(line)
            res_items.append(lines.info(i))
    return StringList(res_data, items=res_items)



#-------------------------------------------------------------------------------
# XparamDirective
#-------------------------------------------------------------------------------

class XparamDirective(SphinxDirective):
    has_content = True
    required_arguments = 1
    optional_arguments = 0
    final_argument_whitespace = True

    def run(self):
        self._parse_arguments()
        head = xnodes.div(classes=["xparam-head"])
        types_node = xnodes.div(classes=["types"])
        desc_node = xnodes.div(classes=["xparam-body"])
        root = xnodes.div(head, desc_node)
        for i, param in enumerate(self.params):
            if i > 0:
                head += xnodes.div(", ", classes=["sep"])
            id0 = param.strip("*/()[]")
            if id0 == "...":
                head += xnodes.div("...", classes=["sep"])
                continue
            root = xnodes.div(root, ids=[id0], classes=["xparam-box"])
            if id0 in ["return", "except"]:
                head += xnodes.div(id0, classes=["param", id0])
            else:
                head += xnodes.div(param, classes=["param"])
        types_str = " | ".join("``%s``" % p for p in self.types)
        self.state.nested_parse(StringList([types_str]), self.content_offset,
                                types_node)
        assert isinstance(types_node[0], nodes.paragraph)
        types_node.children = types_node[0].children
        head += types_node
        self.state.nested_parse(self.content, self.content_offset, desc_node)
        return [root]


    def _parse_arguments(self):
        params, args = self.arguments[0].split(":", 1)
        self.params = [p.strip() for p in params.split(",")]
        self.types = []

        rx_separator = re.compile(r"(?:,?\s+or\s+|\s*[,\|]\s*)")
        opening_brackets = "[({'\""
        closing_brackets = "])}'\""
        args = args.strip()
        i0 = 0
        brackets = []
        i = 0
        while i < len(args):  # iterate by characters
            if brackets:
                closing_bracket = brackets[-1]
                if args[i] == closing_bracket:
                    brackets.pop()
                elif closing_bracket in ['"', "'"]:
                    pass
                elif args[i] in opening_brackets:
                    j = opening_brackets.find(args[i])
                    brackets.append(closing_brackets[j])

            elif args[i] in opening_brackets:
                j = opening_brackets.find(args[i])
                brackets.append(closing_brackets[j])
            else:
                mm = re.match(rx_separator, args[i:])
                if mm:
                    self.types.append(args[i0:i])
                    i += mm.end()
                    i0 = i
                    continue
            i += 1
        assert i == len(args)
        assert not brackets
        self.types.append(args[i0:])




#-------------------------------------------------------------------------------
# XversionActionDirective
#-------------------------------------------------------------------------------

class XversionActionDirective(SphinxDirective):
    #
    #  TODO: add this to 'xversion' domain
    #
    has_content = True
    required_arguments = 1
    optional_arguments = 0
    final_argument_whitespace = False

    def run(self):
        version = self.arguments[0].strip()
        target = "/releases/"
        if not target.startswith("v"):
            target += "v"
        target += version
        if len(version.split('.')) <= 2:
            target += ".0"

        assert self.name.startswith("x-version-")
        action = self.name[10:]
        text = "Deprecated since version " if action == "deprecated" else \
               "Added in version " if action == "added" else \
               "Changed in version " if action == "changed" else "?????"
        node = xnodes.div()
        node += nodes.Text(text)
        node += nodes.inline("", "",
            addnodes.pending_xref("", nodes.Text(version),
                refdomain="std", reftype="doc", refexplicit=True,
                reftarget=target))
        node = xnodes.div(node, classes=["x-version", action])

        if self.content:
            node.attributes['classes'].extend(["admonition", "warning"])
            self.state.nested_parse(self.content, self.content_offset, node)
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
# Event handlers
#-------------------------------------------------------------------------------

def get_file_timestamp(root, filename):
    filepath = pathlib.Path(root) / filename
    if filepath.is_file():
        return filepath.stat().st_mtime
    else:
        return 1.0e+300


# https://www.sphinx-doc.org/en/master/extdev/appapi.html#event-env-get-outdated
def on_env_get_outdated(app, env, added, changed, removed):
    if not hasattr(env, "xobject"):
        return []
    root = env.config.xf_project_root
    docs_to_update = []
    for docname, record in env.xobject.items():
        for filename in record['sources']:
            file_time = get_file_timestamp(root, filename)
            if file_time > record['timestamp']:
                docs_to_update.append(docname)
                break
    return docs_to_update


# https://www.sphinx-doc.org/en/master/extdev/appapi.html#event-env-purge-doc
def on_env_purge_doc(app, env, docname):
    if hasattr(env, "xobject"):
        env.xobject.pop(docname, None)


# https://www.sphinx-doc.org/en/master/extdev/appapi.html#event-env-merge-info
def on_env_merge_info(app, env, docnames, other):
    if not hasattr(other, "xobject"):
        return
    if hasattr(env, "xobject"):
        env.xobject.update(other.xobject)
    else:
        env.xobject = other.xobject


# https://www.sphinx-doc.org/en/master/extdev/appapi.html#event-source-read
def on_source_read(app, docname, source):
    assert isinstance(source, list) and len(source) == 1
    txt = source[0]
    mm = re.match(r"\s*\.\. (xfunction|xmethod|xclass|xexc|xdata|xattr):: (.*)",
                  txt)
    if mm:
        kind = mm.group(1)
        name = mm.group(2)
        # qualifier = name[:name.rfind('.')]
        title = re.sub(r"_", "\\_", name)
        if kind in ["xfunction", "xmethod"]:
            title += "()"
        txt = title + "\n" + ("=" * len(title)) + "\n\n" + \
              ".. py:currentmodule:: datatable\n\n" + txt
        source[0] = txt


def on_html_page_context(app, pagename, templatename, context, doctree):
    if pagename in title_overrides:
        context["title"] = title_overrides[pagename]



#-------------------------------------------------------------------------------
# Setup
#-------------------------------------------------------------------------------

def setup(app):
    app.setup_extension("_ext.xnodes")
    app.add_config_value("xf_module_name", None, "env")
    app.add_config_value("xf_project_root", "..", "env")
    app.add_config_value("xf_permalink_url0", "", "env")
    app.add_config_value("xf_permalink_url2", "", "env")
    app.add_css_file("xfunction.css")
    app.add_css_file("xversion.css")
    app.add_js_file("https://use.fontawesome.com/0f455d5fb2.js")
    app.add_directive("xdata", XobjectDirective)
    app.add_directive("xfunction", XobjectDirective)
    app.add_directive("xmethod", XobjectDirective)
    app.add_directive("xclass", XobjectDirective)
    app.add_directive("xexc", XobjectDirective)
    app.add_directive("xattr", XobjectDirective)
    app.add_directive("xparam", XparamDirective)
    app.add_directive("x-version-added", XversionActionDirective)
    app.add_directive("x-version-changed", XversionActionDirective)
    app.add_directive("x-version-deprecated", XversionActionDirective)

    app.connect("env-get-outdated", on_env_get_outdated)
    app.connect("env-purge-doc", on_env_purge_doc)
    app.connect("env-merge-info", on_env_merge_info)
    app.connect("source-read", on_source_read)
    app.connect("html-page-context", on_html_page_context)

    app.add_node(a_node, html=(visit_a, depart_a))
    app.add_role("xparam-ref", xparamref)
    return {"parallel_read_safe": True,
            "parallel_write_safe": True,
            "env_version": 1,
            }
