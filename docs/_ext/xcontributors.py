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
import types
from docutils import nodes
from sphinx.util.docutils import SphinxDirective
from . import xnodes


class UserRepository:
    def __init__(self):
        self._data = None
        self._env = None
        self._fullnames = None

    def clear(self):
        self._data = None
        self._fullnames = None

    def use_env(self, env):
        self._env = env

    @property
    def data(self):
        if not self._data:
            self._data = types.SimpleNamespace()
            self._compute_version_strings(maxsize=7,
                                          newname="next",
                                          oldname="past")
            self._compute_user_scores()
            self._aggregate_scores()
        return self._data

    @property
    def fullnames(self):
        if not self._fullnames:
            self._compute_fullnames()
        return self._fullnames


    def _compute_version_strings(self, maxsize, newname, oldname):
        """
        self._data.sorted_versions = [<version>]   # list of strings
        self._data.doc_versions = {<docname>: <version>}
        """
        env = self._env
        assert env and isinstance(env.xchangelog, list)
        v2_set = set()
        for info in env.xchangelog:
            version_tuple = info["version"]
            v2 = version_tuple[:2]
            v2_set.add(v2)
        short_versions = sorted(v2_set, reverse=True)

        sorted_versions = []
        v2_to_vstr = {}
        for i, v2 in enumerate(short_versions):
            if v2[0] == 999:
                vstr = newname
            elif len(short_versions) > maxsize and i >= maxsize - 1:
                vstr = oldname
            else:
                vstr = "%d.%d" % v2
            v2_to_vstr[v2] = vstr
            if i < maxsize:
                sorted_versions.append(vstr)

        doc_versions = {}
        for info in env.xchangelog:
            docname = info["doc"]
            version_tuple = info["version"]
            vstr = v2_to_vstr[version_tuple[:2]]
            doc_versions[docname] = vstr

        self._data.sorted_versions = sorted_versions
        self._data.doc_versions = doc_versions


    def _compute_fullnames(self):
        """
        self._data.fullnames = {<username>: <fullname>}
        """
        env = self._env
        assert env and env.xcontributors
        fullnames = {}
        for page, pagedata in env.xcontributors.items():
            for kind in ["PRs", "issues"]:
                assert kind in pagedata
                for username, userdata in pagedata[kind].items():
                    _, fullname = userdata
                    if fullname:
                        known_fullname = fullnames.get(username)
                        if known_fullname and fullname != known_fullname:
                            raise ValueError(
                                "User `%s` has different names: `%s` and `%s`"
                                % (username, fullname, known_fullname))
                        fullnames[username] = fullname
        self._fullnames = fullnames


    def _compute_user_scores(self):
        """
        self._data.scores = {<username>:
                                {<version>:
                                    {"PRs": <score>, "issues": <score>}}}
        """
        env = self._env
        assert env and env.xcontributors
        scores = {}
        for page, pagedata in env.xcontributors.items():
            ver = self._data.doc_versions[page]
            for kind in ["PRs", "issues"]:
                assert kind in pagedata
                for username, userdata in pagedata[kind].items():
                    score, _ = userdata
                    if username not in scores:
                        scores[username] = {}
                    if ver not in scores[username]:
                        scores[username][ver] = {"PRs": 0, "issues": 0}
                    scores[username][ver][kind] += score
        self._data.scores = scores


    def _aggregate_scores(self, decay_factor=0.85, issue_weight=0.25):
        """
        self._data.aggregate_scores = {<username>: <total_score>}
        """
        version_weights = {}
        for i, version in enumerate(self._data.sorted_versions):
            version_weights[version] = decay_factor ** i
        total_scores = {}
        for username, userinfo in self._data.scores.items():
            user_score = 0
            for version, scores in userinfo.items():
                assert isinstance(version, str)
                assert version in version_weights
                weight = version_weights[version]
                version_score = scores["PRs"] + issue_weight * scores["issues"]
                user_score += version_score * weight
            total_scores[username] = user_score
        self._data.aggregate_scores = total_scores


    def get_user_list(self):
        users = self.data.aggregate_scores
        return sorted(users.keys(), key=lambda u: (-users[u], u))

    def get_version_list(self):
        return self.data.sorted_versions

    def get_full_name(self, username):
        return self.fullnames.get(username, username)

    def get_user_score(self, username):
        return self.data.aggregate_scores.get(username, 0)

    def get_user_score_in_version(self, username, version):
        scores = self.data.scores[username].get(version, None)
        if scores:
            return (scores["PRs"], scores["issues"])
        else:
            return (0, 0)



# Singleton instance
users = UserRepository()



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
        return [contributors_placeholder_node(self.env.docname)]


    def _parse(self, lines):
        rx_separator = re.compile(r"\-+")
        rx_contributor = re.compile(r"(?:(\d+)\s+)?"        # contribution count
                                    r"@?([\w\-]+)"          # username
                                    r"(?:\s+<([^<>]*)>)?")  # full name
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

    def __init__(self, docname):
        super().__init__()
        self._docname = docname

    def resolve(self, env):
        record = env.xcontributors[self._docname]
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

        if self.parent.parent is None:
            raise ValueError(
                "..contributors directive should be inside ..changelog directive")
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
        for username in people.keys():
            fullname = users.get_full_name(username)
            url = "https://github.com/" + username
            link = nodes.reference("", fullname, refuri=url, internal=False)
            li = nodes.list_item(classes=["changelog-list-item", "gh"])
            li += nodes.inline("", "", link)
            ul += li
        return ul




#-------------------------------------------------------------------------------
# .. contributors-grid
#-------------------------------------------------------------------------------

class XContributorsGridDirective(SphinxDirective):
    has_content = False
    required_arguments = 0
    optional_arguments = 0
    option_spec = {}

    def run(self):
        return [contributors_grid_placeholder_node()]



class contributors_grid_placeholder_node(nodes.Element, nodes.General):
    def resolve(self):
        out = xnodes.table(classes=["contributors-grid"])
        versions = users.get_version_list()
        row = xnodes.tr(classes=["versions"])
        row += xnodes.td()
        for version in versions:
            row += xnodes.td(version, classes=["version"])
        out += row
        for username in users.get_user_list():
            fullname = users.get_full_name(username)
            score = users.get_user_score(username)
            url = "https://github.com/" + username
            link = nodes.reference("", fullname, refuri=url, internal=False,
                                   reftitle="%.2f" % score)
            row = xnodes.tr()
            row += xnodes.td(nodes.inline("", "", link), classes=["name"])
            for version in versions:
                scores = users.get_user_score_in_version(username, version)
                nprs, nissues = scores
                content = "\u25cf" if nprs else \
                          "\u2022" if nissues else \
                          ""
                classes = ["prs"] if nprs else \
                          ["issues"] if nissues else \
                          []
                details = ""
                if nprs:
                    details += "%d pull request" % nprs
                    if nprs != 1:
                        details += "s"
                if nissues:
                    if details:
                        details += " + "
                    details += "%d issue" % nissues
                    if nissues != 1:
                        details += "s"
                row += xnodes.td(content, title=details, classes=classes)
            out += row
        self.replace_self([out])




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
        users.clear()


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
    users.use_env(env)
    for node in doctree.traverse(contributors_placeholder_node):
        node.resolve(env)
    for node in doctree.traverse(contributors_grid_placeholder_node):
        node.resolve()



#-------------------------------------------------------------------------------
# Extension setup
#-------------------------------------------------------------------------------

def setup(app):
    app.setup_extension("_ext.xnodes")
    app.add_directive("contributors", XContributorsDirective)
    app.add_directive("contributors-grid", XContributorsGridDirective)
    app.connect("env-purge-doc", on_env_purge_doc)
    app.connect("env-merge-info", on_env_merge_info)
    app.connect("doctree-resolved", on_doctree_resolved)
    return {"parallel_read_safe": True,
            "parallel_write_safe": True,
            "env_version": 1}
