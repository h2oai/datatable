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



class unrepr_str(str):
    """
    This is a helper class for KeyError, to be used like this:

        >>> raise KeyError(_str("column A not found"))
        Traceback (most recent call last):
          File "<stdin>", line 1, in <module>
        KeyError: column A not found

    The reason this helper class is needed at all is because by
    default Python will try to repr the message of the exception:

        >>> raise KeyError("column A not found")
        Traceback (most recent call last):
          File "<stdin>", line 1, in <module>
        KeyError: 'column A not found'

    This was a deliberate (although arguably bad) choice in the early
    days of Python. With this class we attempt to mitigate this design
    choice by providing a "fake" repr function that returns the
    wrapped string.
    """
    def __init__(self, msg):
        self.msg = msg

    def __repr__(self):
        return self.msg
