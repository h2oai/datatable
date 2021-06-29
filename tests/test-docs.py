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
#
# This file does some basic checks of the documentation files for
# possible errors.
#
#-------------------------------------------------------------------------------
import os
import pathlib
import pytest
import re
import subprocess
import sys

# First .parent removes the file name, second goes up to the root level
ROOT_PATH = pathlib.Path(__file__).parent.parent
API_PATH = ROOT_PATH / "docs" / "api"


@pytest.mark.skipif(not API_PATH.is_dir(), reason="API docs folder doesn't exist")
def test_xfunction_paths():
    """
    This test looks at all `.. xfunction::` directives and checks
    that the paths specified for `:src:` / `:doc:` / `:tests:`
    options are valid.
    """
    re_path = re.compile(r"\s+:(?:src|docs?|tests?): (\S*)")
    for filepath in API_PATH.glob("**/*.rst"):
        text = filepath.read_text(encoding="utf-8")
        outside = False
        for i, line in enumerate(text.split("\n")):
            if outside:
                if (line.startswith(".. xfunction::") or
                    line.startswith(".. xmethod::") or
                    line.startswith(".. xdata::")):
                        outside = False
            else:
                mm = re.match(re_path, line)
                if mm and mm.group(1) != "--":
                    fullpath = ROOT_PATH / mm.group(1)
                    assert fullpath.is_file(), (
                           "Path %s does not exist, found on line %d of %s"
                           % (fullpath, i + 1, filepath))


@pytest.mark.skipif(not API_PATH.is_dir(), reason="API docs folder doesn't exist")
def test_documentation_h():
    sys.path.append(str(ROOT_PATH / "ci"))
    import gendoc
    del sys.path[-1]
    variables = gendoc.read_header_file(ROOT_PATH / "src/core/documentation.h")
    docstrings = gendoc.read_documentation_files(API_PATH.glob("**/*.rst"))
    for var in variables:
        assert var in docstrings, ("Variable %s declared in documentation.h "
                                   "not found as a :cvar: among *.rst files"
                                   % var)
    for var in docstrings.keys():
        assert var in variables, ("Variable %s is declared as a cvar, but "
                                  "not in the documentation.h file")



@pytest.mark.skipif("DT_BUILD_DOCS" not in os.environ,
                    reason="Environment variable DT_BUILD_DOCS not set")
def test_build_docs():
    """
    This test builds all the docs using Sphinx, and then verifies that
    there were no errors/warnings in the process.

    Since building the documentation takes a considerable amount of time
    and requires additional libraries to be installed, this test will
    only run if enabled via the environment variable DT_BUILD_DOCS.
    """
    proc = subprocess.run(
        ["sphinx-build", "-q", "-E", ".", "_build"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd="docs")
    stdout = proc.stdout.decode('utf-8')
    stderr = proc.stderr.decode('utf-8')
    if stderr or proc.returncode:
        raise RuntimeError(f"There were errors building the documentation "
                           f"(ret.code={proc.returncode}):\n{stderr}")
