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
import pdb
import re
from docutils import nodes
from docutils.parsers.rst import directives
from docutils.statemachine import StringList
from sphinx.util.docutils import SphinxDirective

from .dtframe_directive import table_node, td_node, th_node


#-------------------------------------------------------------------------------
# Directives
#-------------------------------------------------------------------------------

class ChangelogDirective(SphinxDirective):
    has_content = True
    required_arguments = 0
    optional_arguments = 0
    option_spec = {
        "version": directives.unchanged,
        "released": directives.unchanged,
    }

    def run(self):
        version = self.options.get("version", "")
        sect = nodes.section(ids=["v" + version], classes=["changelog"])
        sect.document = self.state.document

        self._note_release_version(version)
        if version:
            title_text = "Version " + version
        else:
            title_text = "Unreleased"
        sect += nodes.title("", title_text)

        lines = []
        lines += [".. changelog-infobox:: " + title_text]
        lines += ["    :released: " + self.options.get("released", "")]
        lines += [""]
        lines.extend(self.content.data)
        sources = [self.content.items[0]] * 3
        sources.extend(self.content.items)

        cc = ChangelogContent(lines, sources)
        new_content = cc.parse()
        self.state.nested_parse(new_content, self.content_offset, sect,
                                match_titles=True)
        return [sect]

    def _note_release_version(self, versionstr):
        if not hasattr(self.env, "xchangelog"):
            self.env.xchangelog = []
        if versionstr:
            version = tuple(int(p) for p in versionstr.split("."))
        else:
            version = (999,)
        self.env.xchangelog.append({
            "version": version,
            "versionstr": versionstr,
            "doc": self.env.docname,
        })



class ChangelogListDirective(SphinxDirective):
    has_content = True
    required_arguments = 0
    optional_arguments = 0
    option_spec = {}

    def run(self):
        ul = nodes.bullet_list(classes=["changelog-list"])
        ul.document = self.state.document
        self.state.nested_parse(self.content, self.content_offset, ul)
        return [ul]



class ChangelogListItemDirective(SphinxDirective):
    has_content = True
    required_arguments = 1
    optional_arguments = 0
    option_spec = {}

    def run(self):
        tag = self.arguments[0]
        li = nodes.list_item(classes=["changelog-list-item"] + tag.split('.'))
        li.document = self.state.document
        self.state.nested_parse(self.content, self.content_offset, li)
        return [li]



class ChangelogInfoboxDirective(SphinxDirective):
    has_content = False
    required_arguments = 1
    optional_arguments = 0
    final_argument_whitespace = True
    option_spec = {
        "released": directives.unchanged,
    }

    def run(self):
        infobox = table_node(classes=["infobox"])
        tbody = nodes.tbody()
        title_row = nodes.row(classes=["title"])
        title_row += th_node(self.arguments[0], colspan=2)
        tbody += title_row
        if self.options["released"]:
            released_row = nodes.row()
            released_row += td_node("Released:")
            released_row += td_node(self.options["released"])
            tbody += released_row
        tbody += changelog_navigation()  # placeholder node
        infobox += tbody
        return [infobox]


class changelog_navigation(nodes.Element, nodes.General): pass




#-------------------------------------------------------------------------------
# Roles
#-------------------------------------------------------------------------------

def issue_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    url = inliner.document.settings.env.config.changelog_issue_url
    assert url
    node = nodes.inline(classes=["issue"])
    if url is None:
        node += nodes.Text("#" + text)
    else:
        url = url.replace("{issue}", text)
        node += nodes.reference(rawtext, "#" + text, refuri=url)
    return [node], []


def user_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    url = inliner.document.settings.env.config.changelog_user_url
    assert url
    mm = re.fullmatch(r"([\w\.\-]+)|([^<>]+)\s+<([\w\.\-]+)>", text)
    if mm:
        if mm.group(1):
            username = displayname = mm.group(1)
        else:
            displayname = mm.group(2)
            username = mm.group(3)
        url = url.replace("{name}", username)
        node = nodes.inline(classes=["user"])
        node += nodes.reference(rawtext, displayname, refuri=url)
        return [node], []
    else:
        msg = inliner.reporter.error("Invalid content of :user: role: `%s`"
                                     % text, line=lineno)
        prb = inliner.problematic(rawtext, rawtext, msg)
        return [prb], [msg]




#-------------------------------------------------------------------------------
# Content Parser helper
#-------------------------------------------------------------------------------

class ChangelogContent:
    rx_list_item = re.compile(r"-\[([\w\.]+)\]\s+")
    rx_issue = re.compile(r"[\(\[]#(\d+)[\)\]]")

    def __init__(self, lines, sources):
        self._in_lines = lines
        self._in_sources = sources
        self._out_lines = []
        self._out_sources = []
        self._iline = 0  # index of the next line to parse

    def parse(self):
        while self.line_available():
            self.parse_normal() or self.parse_list()
        return StringList(self._out_lines, items=self._out_sources)


    # All parsers below return True if parsed successfully, and also
    # advance `self._iline`. Otherwise they return False and leave
    # `self._iline` unchanged.

    def parse_normal(self):
        line, src = self.get_line()
        if re.match(self.rx_list_item, line):
            return False
        else:
            self.out_line(line, src)
            self._iline += 1
            return True

    def parse_list(self):
        _, src = self.get_line()
        self.out_line(".. changelog-list::", src)
        self.out_line("", src)
        item_found = True
        while item_found and self.line_available():
            item_found = self.parse_list_item()
        return True

    def parse_list_item(self):
        line, src = self.get_line()
        found_list_item = re.match(self.rx_list_item, line)
        if not found_list_item:
            return False
        match_text = found_list_item.group(0)
        tag = found_list_item.group(1)
        out_indent = " " * 8
        self.out_line("    .. changelog-list-item:: " + tag, src)
        self.out_line("", src)
        self.out_line(out_indent + line[len(match_text):], src)
        self._iline += 1
        common_indent = None  # str
        while self.line_available():
            line, src = self.get_line()
            self._iline += 1
            len_line_indent = len(line) - len(line.lstrip())
            if len_line_indent == len(line):  # empty line
                self.out_line("", src)
                continue
            if len_line_indent == 0:
                self._iline -= 1
                break
            if not common_indent:
                common_indent = line[:len_line_indent]
            if not line.startswith(common_indent):
                raise ValueError("Inconsistent indentation in line %d of `%s`"
                                 % (src[1], src[0]))
            self.out_line(out_indent + line[len(common_indent):], src)
        return True


    # Simple helpers

    def line_available(self):
        return self._iline < len(self._in_lines)

    def get_line(self):
        """Return current line + source without advancing `._iline`."""
        return (self._in_lines[self._iline],
                self._in_sources[self._iline])

    def out_line(self, line, source):
        self._out_lines.append(self.process_line(line))
        self._out_sources.append(source)

    def process_line(self, line):
        if re.search(self.rx_issue, line):
            return re.sub(self.rx_issue, r":issue:`\1`", line)
        else:
            return line



#-------------------------------------------------------------------------------
# Event handlers for Changelog cross-referencing
#-------------------------------------------------------------------------------

# This event is emitted when a source file is removed from the
# environment (including just before it is freshly read), and
# extensions are expected to purge their info about that file.
#
def on_env_purge_doc(app, env, docname):
    if hasattr(env, "xchangelog"):
        env.xchangelog = [entry
                          for entry in env.xchangelog
                          if entry["doc"] != docname]


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
    if not hasattr(other, "xchangelog"):
        return
    if hasattr(env, "xchangelog"):
        env.xchangelog.extend(other.xchangelog)
    else:
        env.xchangelog = other.xchangelog


# Emitted when a doctree has been “resolved” by the environment, that
# is, all references have been resolved and TOCs have been inserted.
# The doctree can be modified in place.
#
def on_doctree_resolved(app, doctree, docname):
    env = app.builder.env
    if not hasattr(env, "xchangelog"):
        return
    env.xchangelog.sort(key=lambda e: e["version"])

    for node in doctree.traverse(changelog_navigation):
        content = []
        index = -1
        for i, entry in enumerate(env.xchangelog):
            if entry["doc"] == docname:
                index = i
                break
        if index != -1 and index + 1 < len(env.xchangelog):
            vnext = env.xchangelog[index + 1]
            url = app.builder.get_relative_uri(docname, vnext['doc'])
            if vnext["versionstr"]:
                text = "Version " + vnext["versionstr"]
            else:
                text = "(unreleased)"
            row = nodes.row()
            row += td_node("Next release:")
            link = nodes.reference("", text, refdocname=vnext["doc"], refuri=url,
                                   internal=True)
            td = td_node()
            td += nodes.inline("", "", link)
            row += td
            content.append(row)
        if index > 0:
            vprev = env.xchangelog[index - 1]
            url = app.builder.get_relative_uri(docname, vprev['doc'])
            row = nodes.row()
            row += td_node("Previous release:")
            link = nodes.reference("", "", refdocname=vprev["doc"], refuri=url,
                                   internal=True)
            link += nodes.Text("Version " + vprev["versionstr"])
            td = td_node()
            td += nodes.inline("", "", link)
            row += td
            content.append(row)

        node.replace_self(content)




#-------------------------------------------------------------------------------
# Extension setup
#-------------------------------------------------------------------------------

def setup(app):
    app.connect("env-purge-doc", on_env_purge_doc)
    app.connect("env-merge-info", on_env_merge_info)
    app.connect("doctree-resolved", on_doctree_resolved)
    app.add_config_value("changelog_issue_url", None, "env")
    app.add_config_value("changelog_user_url", None, "env")
    app.add_directive("changelog", ChangelogDirective)
    app.add_directive("changelog-infobox", ChangelogInfoboxDirective)
    app.add_directive("changelog-list", ChangelogListDirective)
    app.add_directive("changelog-list-item", ChangelogListItemDirective)
    app.add_role("issue", issue_role)
    app.add_role("user", user_role)
    app.add_css_file("changelog.css")
    return {"parallel_read_safe": True,
            "parallel_write_safe": True,
            "env_version": 1}
