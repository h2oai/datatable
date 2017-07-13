#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
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
    with open(metafile, "w") as out:
        out.write("# NFF1\n")
        out.write("# nrows = %d\n" % dt.nrows)
        out.write('filename,stype,meta,colname\n')
        l = len(str(dt.ncols))
        for i in range(dt.ncols):
            filename = "c%0*d" % (l, i + 1)
            colname = dt.names[i].replace('"', '""')
            _col = dt.internal.column(i)
            stype = _col.stype
            meta = _col.meta
            out.write('%s,%s,%s,"%s"\n' % (filename, stype, meta, colname))
            filename = os.path.join(dest, filename)
            _col.save_to_disk(filename)
