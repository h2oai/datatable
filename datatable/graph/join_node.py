#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
from .dtproxy import g
from datatable.utils.typechecks import TValueError, TTypeError


__all__ = ("join",)



class join:

    def __init__(self, frame):
        self.joinframe = frame
        if not frame.key:
            raise ValueError("The join frame is not keyed")

    def execute(self, ee):
        dt = ee.dt
        xcols = [None] * len(self.joinframe.key)
        for i, colname in enumerate(self.joinframe.key):
            if colname not in dt._inames:
                raise TValueError("Key column `%s` does not exist in the "
                                  "left Frame" % colname)
            xcols[i] = dt._inames[colname]
            l_ltype = dt.ltypes[xcols[i]]
            r_ltype = self.joinframe.ltypes[i]
            if l_ltype != r_ltype:
                raise TTypeError("Join column `%s` has type %s in the left "
                                 "Frame, and type %s in the right Frame. "
                                 % (colname, l_ltype.name, r_ltype.name))
        jindex = dt.internal.join(ee.rowindex, self.joinframe.internal, xcols)
        ee.joinindex = jindex
        g.set_rowindex(jindex)
