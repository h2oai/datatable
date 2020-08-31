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
# Simple Sphinx directive that can be used like this:
#
#     .. ref-context:: datatable.Frame
#
#     Method :meth:`.to_csv()` allows to create CSV files ...
#
# What this directive does is that it treats all references
# encountered after this directive as belonging to the specified
# module/class, and resolves those references accordingly.
#
#-------------------------------------------------------------------------------
from docutils import nodes
from sphinx import addnodes
from sphinx.util.docutils import SphinxDirective



#-------------------------------------------------------------------------------
# ref-context directive + node
#-------------------------------------------------------------------------------

class RefContextDirective(SphinxDirective):
    has_content = False
    required_arguments = 1
    optional_arguments = 0

    def run(self):
        context = self.arguments[0]
        return [ref_context_node(context)]


class ref_context_node(nodes.Node):
    def __init__(self, context):
        super().__init__()
        self.children = ()  # needed for traversal by docutils
        self.context = context

def noop(self, node):   # visit/depart node
    pass



#-------------------------------------------------------------------------------
# Resolve references
#-------------------------------------------------------------------------------

# See https://www.sphinx-doc.org/en/master/extdev/appapi.html#event-doctree-read
def on_doctree_read(app, doctree):
    for ctx_node in doctree.traverse(ref_context_node):
        context = ctx_node.context
        for ref_node in ctx_node.traverse(addnodes.pending_xref, siblings=True):
            if ref_node.get('refexplicit') or ref_node['reftype'] == 'ref':
                continue
            target = ref_node['reftarget']
            if "(" in target:
                target = target[:target.find("(")]
            if target[:1] == '.':
                target = target[1:]
            if "." in target:
                prefix = target[:target.rfind('.')]
                if context.endswith(prefix):
                    target = target[len(prefix) + 1:]
                else:
                    continue
            target = context + "." + target
            ref_node['reftarget'] = target




#-------------------------------------------------------------------------------
# setup
#-------------------------------------------------------------------------------

def setup(app):
    app.add_node(ref_context_node, html=(noop, noop))
    app.connect("doctree-read", on_doctree_read)
    app.add_directive("ref-context", RefContextDirective)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
