#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import os
import re

import datatable.lib._datatable as _datatable
from datatable.dt import DataTable
from datatable.fread import fread
from datatable.utils.typechecks import typed

_builtin_open = open



@typed(dt=DataTable, dest=str)
def save(dt, dest):
    """
    Save datatable in binary NFF format.

    :param dt: DataTable to be saved
    :param dest: destination where the datatable should be saved.
    """
    dest = os.path.expanduser(dest)
    if os.path.exists(dest):
        # raise ValueError("Path %s already exists" % dest)
        pass
    else:
        os.makedirs(dest)
    metafile = os.path.join(dest, "_meta.nff")
    with _builtin_open(metafile, "w") as out:
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
            if meta is None:
                meta = ""
            out.write('%s,%s,%s,"%s"\n' % (filename, stype, meta, colname))
            filename = os.path.join(dest, filename)
            _col.save_to_disk(filename)



@typed(path=str)
def open(path):
    cwd = os.getcwd()
    try:
        path = os.path.expanduser(path)
        if not os.path.isdir(path):
            raise ValueError("%s is not a valid directory" % path)
        # os.chdir(path)

        nrows = 0
        metafile = os.path.join(path, "_meta.nff")
        with _builtin_open(metafile) as inp:
            info = []
            for line in inp:
                if line.startswith("#"):
                    info.append(line[1:].strip())
                else:
                    break
            if not (info and info[0].startswith("NFF")):
                raise ValueError("File _meta.nff has invalid format")
            if info[0] == "NFF1":
                assert len(info) == 2
                mm = re.match("nrows\s*=\s*(\d+)", info[1])
                if mm:
                    nrows = int(mm.group(1))
                else:
                    raise ValueError("nrows info not found in line %r" %
                                     info[1])
            else:
                raise ValueError("Unknown NFF format: %s" % info[0])

        f0 = fread(metafile, sep=",",
                   columns=lambda i, name, type: (name, str))
        f1 = f0(select=["filename", "stype", "meta"])
        colnames = f0["colname"].topython()[0]
        _dt = _datatable.datatable_load(f1.internal, nrows, path)
        dt = DataTable(_dt, colnames=colnames)
        assert dt.nrows == nrows, "Wrong number of rows read: %d" % dt.nrows
        return dt
    finally:
        os.chdir(cwd)
