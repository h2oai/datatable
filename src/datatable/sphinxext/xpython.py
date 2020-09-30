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
# XPython domain is a collection of directives/roles aimed at documenting a
# large python module. The structure of the documentation is such that each
# *object* in the module, including functions, classes, methods, attributes,
# submodules, etc. should have one and only one dedicated documentation page.
#
# Directives
# ----------
#
# "xpy:module" directive declares the current .rst page as the root page for
# documenting a particular submodule <module_name>:
#
#     .. xpy:module:: <module_name>
#
#-------------------------------------------------------------------------------
import docutils
import logging
import re
import sphinx.util.docutils
import warnings
from docutils.nodes import Node, system_message
from typing import List, Dict, Tuple
from sphinx.domains import Domain
from sphinx.domains.python import PythonDomain

logger = logging.getLogger(__name__)



class ModuleDirective(sphinx.util.docutils.SphinxDirective):
    """
    This directive is used to mark a particular .rst page as the page
    that documents a specific python *module*.

    The argument to this directive is the name of the module (the name
    must be fully-qualified). For example:

        .. xpy:module:: datatable.math

    The directive does not generate any user-visible content, but is
    necessary for creating correct API links.
    """
    has_content = False
    required_arguments = 1
    optional_arguments = 0

    def run(self):
        module_name = self.arguments[0]
        xpy_domain = self.env.get_domain('xpy')
        xpy_domain.note_object(objtype = 'module',
                               objname = module_name,
                               docname = self.env.docname)
        xpy_domain.current_context = ("module", module_name)
        return []



class XRefRole(sphinx.util.docutils.ReferenceRole):
    """
    Class that implements rst roles for referencing xpy objects. These
    roles are:

        :xpy:class:`...`
        :xpy:func:`...`
        :xpy:meth:`...`
        :xpy:attr:`...`
        :xpy:mod:`...`
        etc.

    This class creates a `pending_xref` node with the following attributes:

        "refdoc", "refline": document+line where the role is in the source
        "refdomain": the domain name ("xpy")
        "reftype": the full name of the role (without domain), for example
                   "attribute", "method", "function", etc.
        "reftarget": fully-qualified target name, eg. "datatable.Frame.to_csv"

    Inherited properties
    --------------------
    disabled: bool
        A boolean indicates the reference is disabled. The reference
        is disabled if it starts with `!`.

    has_explicit_title: bool
        A boolean indicates the role has explicit title or not. "Explicit"
        references are of the form `title <target>`. For implicit references
        the title and the target are the same.

    title: str
        The link title for the interpreted text.

    target: str
        The link target for the interpreted text.

    name: str
        Role name actually used in the document.

    rawtext: str
        A string containing the entire interpreted text input.

    text: str
        The interpreted text content.

    lineno: int
        The line number where the interpreted text begins.

    content: List[str]
        The directive content for customization (from the "role" directive).
    """

    def run(self) -> Tuple[List[Node], List[system_message]]:
        assert self.name.startswith("xpy:")
        self.rolename = XPythonDomain.translate_type(self.name[4:])
        self.need_parens = self.rolename in ["function", "method"]
        self.allows_dot = self.rolename in ["method", "attribute"]

        node = sphinx.addnodes.pending_xref(self.rawtext)
        classes = ['xref', self.rolename]
        node['refdoc'] = self.env.docname
        node['refline'] = self.lineno
        node['refdomain'] = "xpy"  # Tell Sphinx which domain resolves this ref
        node['reftype'] = self.rolename
        title, target = self.get_title_and_target(node)
        node['reftarget'] = target
        node += docutils.nodes.literal(self.rawtext, title, classes=classes)
        return [node], []


    def get_title_and_target(self, node):
        xpy = self.env.get_domain('xpy')
        title = self.title
        target = self.target
        if self.has_explicit_title:
            qual0 = target.split('.')[0]
            if qual0 == xpy.main_module:
                pass
            elif qual0 == xpy.main_module_abbrev:
                target = xpy.main_module + target[len(qual0):]
            else:
                self.error(f"The target of reference `{self.rawtext}` "
                           f"is not fully-qualified", node)
                return "", ""
        else:
            assert title == target
            mm = re.match(r"^(\.?)((?:\w+\.)*)(\w+)(\(.*\))?$", title)
            if not mm:
                self.error(f"Invalid reference `{self.rawtext}`", node)
                return "", ""
            initialdot = mm.group(1)
            qualifier = mm.group(2)
            fnname = mm.group(3)
            parens = mm.group(4)
            if parens and not self.need_parens:
                self.error(f"Reference `{self.rawtext}` should not have "
                           f"parentheses")
                parens = ""
            if not parens and self.need_parens:
                parens = "()"
            if initialdot and qualifier:
                self.error(f"Reference `{self.rawtext}` should not have both "
                           f"the initial dot and the qualifier", node)
                initialdot = ''
            if initialdot and not self.allows_dot:
                self.error(f"Reference `{self.rawtext}` cannot start with a .", node)
            if initialdot:
                ctx = xpy.current_context
                if ctx is None or ctx[0] != "class":
                    self.error(f"Dot-reference `{self.rawtext}` cannot be used "
                               f"outside of a class context", node)
                else:
                    assert ctx[1].startswith(xpy.main_module)
                    qualifier = ctx[1] + '.'
            if qualifier:
                qual0 = qualifier.split('.')[0]
                if qual0 == xpy.main_module:
                    pass
                elif qual0 == xpy.main_module_abbrev:
                    qualifier = xpy.main_module + qualifier[len(qual0):]
                else:
                    self.error(f"Reference `{self.rawtext}` "
                               f"is not fully-qualified", node)
                    return "", ""
                target = qualifier + fnname
                if initialdot:
                    title = '.' + fnname + parens
                else:
                    title = xpy.main_module_abbrev + \
                            qualifier[len(xpy.main_module):] + fnname + parens
            else:
                ctx = xpy.current_context
                if ctx is None:
                    module = xpy.main_module
                elif ctx[0] == "class":
                    self.error(f"Ambiguous reference `{self.rawtext}` within 'class' "
                               f"context: either use a dot-name to refer to a class "
                               f"method/property, or a qualified name to refer to a "
                               f"global function/object", node)
                    module = "-"
                else:
                    module = ctx[1]
                title = fnname + parens
                target = module + '.' + fnname

        return title, target


    def error(self, msg, node=None):
        self.env.get_domain('xpy').error(msg, node)




#-------------------------------------------------------------------------------
# XPythonDomain
#-------------------------------------------------------------------------------

class XPythonDomain(Domain):
    """
    An extended Python domain. This domain aims to supplement the
    standard 'py:' domain and provide a richer documentation
    environment.
    """
    main_module = "datatable"
    main_module_abbrev = "dt"
    name = "xpy"
    label = "XPython"

    initial_data = {
        # Dictionary {(object_type, object_name): doc_name}
        "refs": {}
    }
    directives = {
        'module': ModuleDirective,
    }
    roles = {
        'attr':  XRefRole(),
        'class': XRefRole(),
        'data':  XRefRole(),
        'func':  XRefRole(),
        'meth':  XRefRole(),
    }

    def __init__(self, env):
        super().__init__(env)
        self._current_context = None  # Optional[Tuple[str, str]]
        self._current_doc = None      # Optional[str]


    @property
    def refs(self):
        return self.data.setdefault("refs", {})


    def clear_doc(self, docname: str) -> None:
        for key, val in self.refs.items():
            if val == docname:
                del self.refs[key]
                break


    def merge_domaindata(self, docnames: List[str], otherdata: Dict) -> None:
        for key, data in otherdata['refs'].items():
            if data in docnames:
                self.refs[key] = data


    def note_object(self, objtype, objname, docname):
        fulltype = self.translate_type(objtype)
        key = (fulltype, objname)
        if key in self.refs and self.refs[key] != docname:
            raise self.error("Duplicate key %r for document %s" % (key, docname))
        self.refs[key] = docname


    def resolve_xref(self, env, fromdoc, builder, objtype, target, node, childnode):
        targetdoc = self.find_ref(node)
        if not targetdoc:
            return None
        return self.make_node(builder = builder,
                              from_doc = fromdoc,
                              to_doc = targetdoc,
                              childnode = childnode)

    def find_ref(self, node):
        reftype = node.get("reftype")
        reftarget = node.get("reftarget")
        if (reftype, reftarget) in self.refs:
            return self.refs[reftype, reftarget]
        for t, n in self.refs.keys():
            if reftarget == n:
                self.error(f"Object `{reftype}:{reftarget}` not found; "
                           f"did you mean `{t}:{n}`?", node)
                return
        self.error(f"Object `{reftype}:{reftarget}` not found", node)

    def make_node(self, builder, from_doc, to_doc, childnode):
        node = docutils.nodes.reference("", "", internal=True)
        node['refuri'] = builder.get_relative_uri(from_doc, to_doc)
        node += childnode
        return node


    _types_map = {
        "attr":      "attribute",
        "attribute": "attribute",
        "class":     "class",
        "data":      "data",
        "exc":       "exception",
        "exception": "exception",
        "func":      "function",
        "function":  "function",
        "meth":      "method",
        "method":    "method",
        "mod":       "module",
        "module":    "module",
    }

    @staticmethod
    def translate_type(objtype):
        fulltype = XPythonDomain._types_map.get(objtype, None)
        if fulltype is None:
            raise ValueError(f"Unknown object type {objtype}")
        return fulltype

    def warn(self, msg):
        print("\x1B[93mWarning\x1B[33m: " + msg + "\x1B[m")

    def error(self, msg, node=None):
        if node:
            doc = node.get("refdoc")
            line = node.get("refline")
            msg += f" -- at {doc}:{line}"
        else:
            msg += f" -- at {self.env.docname}"
        logger.error(f"\x1B[91mError: {msg}\x1B[m")



    #---------------------------------------------------------------------------
    # Current module / class functionality
    #---------------------------------------------------------------------------

    @property
    def current_context(self):
        if self._current_doc != self.env.docname:
            self._current_context = None
        return self._current_context

    @current_context.setter
    def current_context(self, new_context):
        assert isinstance(new_context, tuple) and len(new_context) == 2
        assert new_context[0] in ("module", "class")

        curr_ctx = self.current_context
        if new_context == curr_ctx:
            return
        if curr_ctx is not None:
            self.error(f"Attempt to redefine context from "
                       f"{curr_ctx[0]}:{curr_ctx[1]} to "
                       f"{new_context[0]}:{new_context[1]} "
                       f"for document {self.env.docname}")
        self._current_context = new_context
        self._current_doc = self.env.docname






#-------------------------------------------------------------------------------
# setup
#-------------------------------------------------------------------------------

def setup(app):
    app.add_domain(XPythonDomain)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
