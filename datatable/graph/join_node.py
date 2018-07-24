#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
from datatable.lib import core
from .dtproxy import g

__all__ = ("join",)



class join:

    def __init__(self, frame):
        self.frame = frame
        if not frame.key:
            raise ValueError("The join frame is not keyed")

    def execute(self, ee):
        dt = ee.dt
        xcols = [None] * len(self.frame.key)
        for i, colname in enumerate(self.frame.key):
            if colname not in dt._inames:
                raise ValueError("Key column `%s` does not exist in the "
                                 "left Frame" % colname)
            xcols[i] = dt._inames[colname]
            l_ltype = dt.ltypes[xcols[i]]
            r_ltype = self.frame.ltypes[i]
            if l_ltype != r_ltype:
                raise TypeError("Join column `%s` has type %s in the left "
                                "Frame, and type %s in the right Frame. "
                                % (colname, l_ltype, r_ltype))
        jindex = core.join_frame(dt.internal, self.frame.internal,
                                 ee.rowindex, xcols)
        ee.joinindex = jindex
        g.set_rowindex(jindex)
