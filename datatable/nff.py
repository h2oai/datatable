#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

# noinspection PyUnresolvedReferences
import _datatable as c
import os
from datatable.dt import DataTable
from datatable.utils.typechecks import typed



@typed(dt=DataTable, dest=str)
def save(dt, dest):
    """
    Save datatable in binary NFF format.

    :param dt: DataTable to be saved
    :param dest: destination where the datatable should be saved.
    """
    dest = os.path.expanduser(dest)
    if os.path.exists(dest):
        raise RuntimeError("Path %s already exists" % dest)
    os.makedirs(dest)
    metafile = os.path.join(dest, "_meta.nff")
    with open(metafile, "w") as outmeta:
        outmeta.write("# NFF1\n")
        outmeta.write("# nrows=%d\n" % dt.nrows)
        outmeta.write('"filename","stype","colname","meta"\n')
        for i in range(dt.ncols):
            filename = "c%d" % i
            stype = dt.stypes[i]
            name = dt.names[i].replace('"', '""')
            meta = ""
            if stype == "i4s":
                meta = '""'  # get meta...
            outmeta.write('"%s","%s","%s",%s\n' % (filename, stype, name, meta))
            fullfile = os.path.join(dest, filename)
            c.write_column_to_file(fullfile, dt.internal, i)
