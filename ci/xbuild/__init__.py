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
import sys
from .compiler import Compiler
from .extension import Extension
from .logger import Logger0, Logger1, Logger2, Logger3
from .wheel import Wheel

__all__ = (
    "Compiler",
    "Extension",
    "Logger0",
    "Logger1",
    "Logger2",
    "Logger3",
    "Wheel",
)


# Enable console virtual terminal sequences on Windows
if sys.platform == "win32":
    import ctypes
    from ctypes import wintypes
    windll = ctypes.LibraryLoader(ctypes.WinDLL)
    set_console_mode = windll.kernel32.SetConsoleMode
    set_console_mode.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    set_console_mode.restype = wintypes.BOOL

    get_std_handle = windll.kernel32.GetStdHandle
    get_std_handle.argtypes = [wintypes.DWORD]
    get_std_handle.restype = wintypes.HANDLE

    STDOUT = get_std_handle(-11)
    set_console_mode(STDOUT, 5)
