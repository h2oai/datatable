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
import re
from docutils import nodes
from docutils.parsers.rst import directives
from sphinx.util.docutils import SphinxDirective
from . import xnodes



#-------------------------------------------------------------------------------
# .. contributors
#-------------------------------------------------------------------------------

class XContributorsDirective(SphinxDirective):
    has_content = True
    required_arguments = 0
    optional_arguments = 0
    option_spec = {}

    def run(self):
        self._parse(self.content.data)
        self._store_env()
        return [contributors_placeholder_node()]


    def _parse(self, lines):
        rx_separator = re.compile(r"\-+")
        rx_contributor = re.compile(r"(?:(\d+)\s+)?"
                                    r"([\w\-]+)"
                                    r"(?:\s+<([^<>]*)>)?")
        self.people = {"PRs": {}, "issues": {}}
        mode = "PRs"
        for line in lines:
            if not line:
                continue
            if re.fullmatch(rx_separator, line):
                mode = "issues"
                continue
            mm = re.fullmatch(rx_contributor, line)
            if not mm:
                raise self.error("Invalid contributor %r" % line)
            amount = int(mm.group(1)) if mm.group(1) else 1
            username = mm.group(2)
            fullname = mm.group(3)
            if username in self.people[mode]:
                raise self.error("Duplicate user %s in ..contributors "
                                 "directive" % username)
            self.people[mode][username] = (amount, fullname)
        if not self.people["PRs"]:
            raise self.error("Missing code contributors")


    def _store_env(self):
        env = self.env
        docname = env.docname
        if not hasattr(env, "xchangelog"):
            env.xchangelog = []
        version = None
        for entry in env.xchangelog:
            if entry["doc"] == docname:
                version = entry["version"]
                assert isinstance(version, tuple)
        if not version:
            raise self.error("..contributors directive must be used in "
                             "conjunction with a ..changelog directive")
        if not hasattr(env, "xcontributors"):
            env.xcontributors = dict()
        if docname in env.xcontributors:
            raise self.error("Only single ..contributors directive is "
                             "allowed on a page")
        env.xcontributors[docname] = self.people



# This node will get replaced with actual content
class contributors_placeholder_node(nodes.Element, nodes.General):
    """
    Temporary node that will be replaced with actual content before
    the page is rendered.
    """

    def resolve(self, record):
        sect = nodes.section(ids=["contributors"], classes=["contributors"])
        sect += nodes.title("", "Contributors")
        sect += nodes.paragraph("", self._prepare_text(record))
        if record["PRs"]:
            sect += nodes.paragraph("", "",
                        nodes.strong("", "Code & documentation contributors:"))
            sect += self._render_contributors(record["PRs"])
        if record["issues"]:
            sect += nodes.paragraph("", "",
                        nodes.strong("", "Issues contributors:"))
            sect += self._render_contributors(record["issues"])

        j = self.parent.parent.children.index(self.parent)
        self.parent.parent.children.insert(j + 1, sect)
        self.replace_self([])

    def _prepare_text(self, record):
        n_contributors_prs = len(record["PRs"])
        n_contributors_issues = len(record["issues"])
        assert n_contributors_prs

        if n_contributors_prs == 1:
            people_prs = "1 person"
        else:
            people_prs = "%d people" % n_contributors_prs
        if n_contributors_issues == 1:
            people_issues = "1 more person"
        else:
            people_issues = "%d more people" % n_contributors_issues

        text = ("This release was created with the help of %s who contributed "
                "code and documentation" % people_prs)
        if n_contributors_issues:
            text += (", and %s who submitted bug reports and feature requests"
                     % people_issues)
        text += "."
        return text

    def _render_contributors(self, people):
        ul = nodes.bullet_list(classes=["changelog-list", "simple"])
        for username, record in people.items():
            fullname = record[1]
            if not fullname:
                fullname = username
            url = "https://github.com/" + username
            link = nodes.reference("", fullname, refuri=url, internal=False)
            li = nodes.list_item(classes=["changelog-list-item", "gh"])
            li += nodes.inline("", "", link)
            ul += li
        return ul





#-------------------------------------------------------------------------------
# Event handlers
#-------------------------------------------------------------------------------

# This event is emitted when a source file is removed from the
# environment (including just before it is freshly read), and
# extensions are expected to purge their info about that file.
#
def on_env_purge_doc(app, env, docname):
    if hasattr(env, "xcontributors"):
        env.xcontributors.pop(docname, None)


# This event is only emitted when parallel reading of documents is
# enabled. It is emitted once for every subprocess that has read some
# documents.
#
# You must handle this event in an extension that stores data in the
# environment in a custom location. Otherwise the environment in the
# main process will not be aware of the information stored in the
# subprocess.
#
def on_env_merge_info(app, env, docnames, other):
    if not hasattr(other, "xcontributors"):
        return
    if hasattr(env, "xcontributors"):
        env.xcontributors.update(other.xcontributors)
    else:
        env.xcontributors = other.xcontributors



# Emitted when a doctree has been “resolved” by the environment, that
# is, all references have been resolved and TOCs have been inserted.
# The doctree can be modified in place.
#
def on_doctree_resolved(app, doctree, docname):
    env = app.builder.env
    if not hasattr(env, "xcontributors"):
        return
    if docname in env.xcontributors:
        for node in doctree.traverse(contributors_placeholder_node):
            node.resolve(env.xcontributors[docname])




#-------------------------------------------------------------------------------
# Extension setup
#-------------------------------------------------------------------------------

def setup(app):
    app.setup_extension("sphinxext.xnodes")
    app.add_directive("contributors", XContributorsDirective)
    app.connect("env-purge-doc", on_env_purge_doc)
    app.connect("env-merge-info", on_env_merge_info)
    app.connect("doctree-resolved", on_doctree_resolved)
    return {"parallel_read_safe": True,
            "parallel_write_safe": True,
            "env_version": 1}
