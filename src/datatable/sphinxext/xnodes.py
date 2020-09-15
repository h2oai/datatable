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
This is a helper module that provides several nodes that are useful
for building HTML content, yet they are not provided by the standard
docutils / Sphinx.
"""
from docutils import nodes

__all__ = (
    "table",
    "td",
    "th",
    "tr",
)



# Parent class for all x-nodes. It provides a simpler constructor
# API compared to `nodes.Element`: the children of a node can be
# specified both as a parameter, and as a vararg-sequence. For
# example:
#
#     tr(classes=["example-row"], children=[
#            td("Cell 1"), td("Cell 2")
#        ])
#
class xElement(nodes.Element):
    def __init__(self, *args, rawtext="", children=[], **attributes):
        assert isinstance(children, list)
        if args:
            assert not children
            children = [a for a in args if a is not None]
        for i, child in enumerate(children):
            if isinstance(child, str):
                children[i] = nodes.Text(child)
        super().__init__(rawtext, *children, **attributes)



# xnodes.table()
#   Similar to nodes.table, this node is used to build an HTML table.
#   However, unlike the docutils' `table`, this node is suitable for
#   structural tables (i.e. table used as a scaffolding for the
#   content).
#
class table(xElement, nodes.General): pass

def visit_table(self, node):
    self.body.append(self.starttag(node, "table"))

def depart_table(self, node):
    self.body.append("</table>")



# xnodes.tr()
#   Similar to nodes.row, this is a node that will be rendered as a
#   <tr> HTML element. Unlike the docutils' row, this node will not
#   be embellished with odd/even row classes.
#
class tr(xElement, nodes.Part): pass

def visit_tr(self, node):
    self.body.append(self.starttag(node, "tr"))

def depart_tr(self, node):
    self.body.append("</tr>")



# xnodes.td()
# xnodes.th()
#   Node that renders into a <td> / <th> HTML element, supports
#   attributes `colspan` and `rowspan`.
#
class th(xElement, nodes.Part): pass
class td(xElement, nodes.Part): pass

def visit_tdth(self, node):
    attrs = {}
    for key in ["rowspan", "colspan", "title"]:
        if key in node.attributes:
            attrs[key] = node.attributes[key]
    tag = node.__class__.__name__[1:]
    self.body.append(self.starttag(node, tag, suffix="", **attrs))

def depart_tdth(self, node):
    tag = node.__class__.__name__[1:]
    self.body.append("</%s>" % tag)



# xnodes.div()
#   Simple structural element node, corresponds to <div> in HTML.
#
class div(xElement, nodes.General): pass

def visit_div(self, node):
    # Note: Sphinx's `.starttag()` adds a newline after the tag, which causes
    # problem if the content of the div is a text node
    self.body.append(self.starttag(node, "div").strip())

def depart_div(self, node):
    self.body.append("</div>")




#-------------------------------------------------------------------------------
# Extension setup
#-------------------------------------------------------------------------------

def setup(app):
    # Set custom names for the node classes, otherwise Sphinx
    # complains that the node with same name already registered.
    table.__name__ = "xtable"
    tr.__name__ = "xtr"
    th.__name__ = "xth"
    td.__name__ = "xtd"
    div.__name__ = "xdiv"
    app.add_node(table, html=(visit_table, depart_table))
    app.add_node(tr, html=(visit_tr, depart_tr))
    app.add_node(td, html=(visit_tdth, depart_tdth))
    app.add_node(th, html=(visit_tdth, depart_tdth))
    app.add_node(div, html=(visit_div, depart_div))
