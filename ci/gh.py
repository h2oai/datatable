#!/usr/bin/env python
# -*- coding: utf-8 -*-
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
# This script requires module `PyGithub`
#
# Also, you need a personal access token to use github API.
# See https://docs.github.com/en/github/authenticating-to-github/keeping-your-account-and-data-secure/creating-a-personal-access-token
# for details. The scope for the token should be "repo" and all sub-categories.
#-------------------------------------------------------------------------------
import argparse
import getpass
import github
import json
import re
import time
from github.GithubException import (
    BadCredentialsException,
    RateLimitExceededException,
    UnknownObjectException
)

rx_attribution = re.compile(r"^_?"
                            r"(?:Attribute[\s\-][tT]o|Originally posted by)"
                            r":?\s+@([\w\-]+)",
                            re.MULTILINE)


def progress_bar(current, total, progress_bar_size=50):
    if current is None:
        print("\r" + " "*(progress_bar_size + 10) + "\r", end="")
        return
    if not(0 <= current <= total):
        import pdb
        pdb.set_trace()
    assert 0 <= current <= total
    n_chars_done = round(progress_bar_size * current / total)
    n_chars_todo = progress_bar_size - n_chars_done
    pbar = "\r["
    pbar += "#" * n_chars_done
    pbar += "-" * n_chars_todo
    pbar += "] %.1f%%   " % (100 * current / total)
    print(pbar, end="")


def connect(auth=None):
    """
    Connect to Github and return the connection object.

    Parameters
    ----------
    auth: bool | None
        Do we need to authenticate to access the repository? If True,
        then a login/password will be prompted. If False, the
        repository must be public or otherwise an exception will be
        raised. If None, then the method will first attempt to access
        the repository publicly, and if that fails fall back to
        authentication.
    """
    g = None
    if auth:
        import os
        token = os.environ.get("GITHUB_AUTH_TOKEN")
        while not g:
            if not token:
                token = getpass.getpass("Enter your GitHub access token: ")
            try:
                g = github.Github(auth=github.Auth.Token(token))
                g.rate_limiting
            except BadCredentialsException:
                print("Invalid token, please try again")
                token = None
    else:
        g = github.Github()
    print("Connected to GitHub\n"
          "  requests remaining = %d\n"
          "  requsts limit = %d" % g.rate_limiting)
    print("  limit resets at %s" %
        time.strftime("%Y-%m-%d %H:%M:%S",
                      time.localtime(g.rate_limiting_resettime)))
    return g


def get_repo(github, repository_name):
    """
    Return a Github.Repository object given the repository name.

    Parameters
    ----------
    github: github.MainClass.Github

    repository_name: str
        The name of the repository, including the organization. For
        example: "h2oai/datatable".
    """
    assert isinstance(repository_name, str)
    try:
        return github.get_repo(repository_name)
    except UnknownObjectException:
        raise KeyError("Repository %s not found"
                       % repository_name) from None


def issue_and_pr_authors(repo, args):
    """
    Return the dict of authors for all closed PRs/issues in the
    repository, grouped by the milestone.

    Returns a dictionary of the form

        {<milestone>: {"PRs": {<author>: <count>},
                       "issues": {<author>: <count>}}}

    """
    assert isinstance(repo, github.Repository.Repository)
    attrs = {"state": "closed"}
    if args.milestone:
        for milestone in repo.get_milestones(state="all"):
            if milestone.title == args.milestone:
                attrs["milestone"] = milestone
                break
        if not attrs["milestone"]:
            raise KeyError("Milestone `%s` not found" % args.milestone)
    prs = repo.get_issues(**attrs)
    count = prs.totalCount
    out = {}
    for i, issue in enumerate(prs):
        progress_bar(i + 1, count)
        milestone = issue.milestone.title if issue.milestone else ""
        if issue.pull_request:
            if not issue.as_pull_request().merged:
                continue  # skip PRs that were closed without merging
            kind = "PRs"
        else:
            kind = "issues"
        mm = re.search(rx_attribution, issue.body) if issue.body else None
        if mm:
            author = mm.group(1)
        else:
            author = issue.user.login

        if milestone not in out:
            out[milestone] = {"PRs": {}, "issues": {}}
        if author not in out[milestone][kind]:
            out[milestone][kind][author] = 0
        out[milestone][kind][author] += 1

    # Clear the progress bar
    progress_bar(None, None)
    return out



def cmd_milestones(repo):
    assert isinstance(repo, github.Repository.Repository)
    print("==== Milestones in %s ====" % repo.full_name)
    for milestone in repo.get_milestones(state="all"):
        print("    " + milestone.title)


def cmd_authors(repo, args):
    assert isinstance(repo, github.Repository.Repository)
    print("==== Authors in %s ====" % repo.full_name)
    out = issue_and_pr_authors(gdt, args)
    for milestone in sorted(out.keys()):
        print("~~~~ Milestone: `%s` ~~~~" % milestone)
        for kind in ["PRs", "issues"]:
            print("    -- %s --" % kind)
            authors = out[milestone][kind]
            sorted_authors = sorted(authors.keys(),
                                    key=lambda k: -authors[k])
            for author in sorted_authors:
                count = authors[author]
                print("    %-3d @%s" % (count, author))
        print()




if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='Gather GitHub properties of a repository'
    )
    parser.add_argument("cmd", metavar="CMD",
        choices=["milestones", "authors"],
        help="What information to return: `milestones` produces the list "
             "of milestones in the repository, `authors` reports the count "
             "of closed issues/PRs per each author, grouped by milestone"
        )
    parser.add_argument("--auth", action="store_true",
        help="Use authentication when connecting to the repository")
    parser.add_argument("--repository", default="h2oai/datatable",
        help="Full name of the GitHub repository, including the organization")
    parser.add_argument("--milestone", default=None,
        help="Only consider issues belonging to the specified milestone.")
    args = parser.parse_args()

    try:
        gh = connect(auth=args.auth)
        gdt = get_repo(gh, args.repository)
        if args.cmd == "milestones":
            cmd_milestones(gdt)
        if args.cmd == "authors":
            cmd_authors(gdt, args=args)
    except RateLimitExceededException as e:
        print()
        msg = "RuntimeError: Rate limit exceeded, consider using argument --auth."
        msg = "\x1b[3;91m" + msg + "\x1b[m"
        msg += "\n  Status: %r" % (e.status,)
        msg += "\n  Data: %s" % (json.dumps(e.data),)
        raise SystemExit(msg)
