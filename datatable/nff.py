#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import os

import datatable as dt
from datatable.lib import core
from datatable.utils.typechecks import TTypeError, TValueError

_builtin_open = open


def open(path):
    if isinstance(path, bytes):
        return core.open_jay(path)
    if not isinstance(path, str):
        raise TTypeError("Parameter `path` should be a string")
    path = os.path.expanduser(path)
    if not os.path.exists(path):
        msg = "Path %s does not exist" % path
        if not path.startswith("/"):
            msg += " (current directory = %s)" % os.getcwd()
        raise TValueError(msg)
    if os.path.isdir(path):
        raise TValueError("Path %s is a directory" % path)
    return core.open_jay(path)
