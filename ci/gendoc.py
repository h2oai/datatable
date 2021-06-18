#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2021 H2O.ai
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
import time


def generate_documentation(infile, outfile, docfiles):
    variables = read_header_file(infile)
    docstrings = read_documentation_files(docfiles)
    with open(outfile, 'wt') as out:
        write_intro(out)
        for var in variables:
            write_variable(out, var, docstrings.get(var))
        write_outro(out)



def read_header_file(hfile):
    """
    Read the .h file where all documentation constants are declared
    and return the list of all doc-variables declared there.
    """
    rx = re.compile(r"\s*extern\s+const\s+char\*\s+(doc\w+);")
    out = []
    with open(hfile, 'rt') as inp:
        for line in inp:
            mm = re.match(rx, line)
            if mm:
                out.append(mm.group(1))
    return out



def read_documentation_files(docfiles):
    rx_directive = re.compile(
            r"\.\. (xfunction|xmethod|xclass|xattr|xparam|xexc)::")
    rx_option = re.compile(r"(\s+):(\w+):\s*(.*)")
    out = {}
    for docfile in docfiles:
        with open(docfile, 'rt') as inp:
            indent = 0  # indent within the xfunction's body
            xkind = None
            cvar_name = None
            signature = None
            state = "initial"
            in_function_options = False
            in_function_body = False
            content = ""
            for iline, line in enumerate(inp):
                if state == "initial":
                    mm = re.match(rx_directive, line)
                    if mm:
                        xkind = mm.group(1)
                        state = "options"

                elif state == "options":
                    mm = re.match(rx_option, line)
                    if mm:
                        if indent == 0:
                            indent = len(mm.group(1))
                        if indent != len(mm.group(1)):
                            raise Error("Inconsistent indentation in %s's options "
                                        "at %s:L%d" % (xkind, docfile, iline+1))
                        option = mm.group(2)
                        value = mm.group(3).strip()
                        if option == "cvar":
                            cvar_name = value
                        if option == "signature":
                            signature = value
                    elif line.strip() == "":
                        # The body of the function follows after a blank line
                        if not cvar_name: break
                        state = "body"

                elif state == "body":
                    if line.strip() == '':
                        content += '\n'
                    else:
                        # If the indent level of this line is less than
                        # `indent`, then it means we are done reading the
                        # function's body
                        if line[:indent].strip():
                            break
                        content += line[indent:]

            if cvar_name:
                content = content.strip() + '\n'
                if signature:
                    content = signature + "\n--\n\n" + content
                out[cvar_name] = content
    return out



def write_intro(out):
    out.write(
        '//------------------------------------------------------------------------------\n'
        '// Copyright %d H2O.ai\n'
        '//\n'
        '// Permission is hereby granted, free of charge, to any person obtaining a\n'
        '// copy of this software and associated documentation files (the "Software"),\n'
        '// to deal in the Software without restriction, including without limitation\n'
        '// the rights to use, copy, modify, merge, publish, distribute, sublicense,\n'
        '// and/or sell copies of the Software, and to permit persons to whom the\n'
        '// Software is furnished to do so, subject to the following conditions:\n'
        '//\n'
        '// The above copyright notice and this permission notice shall be included in\n'
        '// all copies or substantial portions of the Software.\n'
        '//\n'
        '// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n'
        '// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n'
        '// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n'
        '// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n'
        '// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING\n'
        '// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS\n'
        '// IN THE SOFTWARE.\n'
        '//------------------------------------------------------------------------------\n'
        '// This file is auto-generated by ci/gendoc.py\n'
        '//------------------------------------------------------------------------------\n'
         % time.gmtime().tm_year
    )
    out.write('#include "documentation.h"\n')
    out.write('namespace dt {\n')
    out.write('\n\n')


def write_outro(out):
    out.write('\n\n')
    out.write('}\n')


def write_variable(out, variable, content):
    out.write("const char* %s =" % variable)
    if content:
        out.write('\n')
        out.write('R"(')
        out.write(content)
        out.write(')";\n')
    else:
        out.write(" nullptr;\n")
    out.write('\n\n')
