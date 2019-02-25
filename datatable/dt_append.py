#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable as dt


def rbind(*frames, force=False, bynames=True):
    r = dt.Frame()
    r.rbind(*frames, force=force, bynames=bynames)
    return r


def cbind(*frames, force=False):
    r = dt.Frame()
    r.cbind(*frames, force=force)
    return r

