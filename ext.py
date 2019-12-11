#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2019 H2O.ai
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
from ci import xbuild

ext = xbuild.Extension()
ext.log = xbuild.Logger3()
ext.name = "_datatable"
ext.build_dir = "build/x"
ext.destination_dir = "datatable/lib/"
ext.add_sources("c/**/*.cc")

# Compile settings
ext.compiler.add_include_dir("c")
ext.compiler.add_default_python_include_dir()
ext.compiler.add_compiler_flag("-std=c++11")
ext.compiler.add_compiler_flag("-fPIC")
ext.compiler.add_compiler_flag("-g3")
ext.compiler.enable_colors()

# Link settings
ext.compiler.add_linker_flag("-bundle")
ext.compiler.add_linker_flag("-undefined", "dynamic_lookup")
ext.compiler.add_linker_flag("-g")
ext.compiler.add_linker_flag("-m64")

# ext.compiler.add_linker_flag("-L/usr/lib")
# ext.compiler.add_linker_flag("-L/usr/local/opt/llvm/lib")
# ext.compiler.add_linker_flag("-lz")
# ext.compiler.add_linker_flag("-L/Library/Frameworks/Python.framework/Versions/3.6/lib")

ext.build()
