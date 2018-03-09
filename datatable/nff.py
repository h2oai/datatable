#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import os
import re

import datatable as dt
from datatable.lib import core
# from datatable.fread import Frame
# from datatable.fread import fread
from datatable.utils.typechecks import typed, TValueError, dtwarn

_builtin_open = open



@typed(dest=str, _strategy=str)
def save(self, dest, _strategy="auto"):
    """
    Save Frame in binary NFF format.

    :param dest: destination where the Frame should be saved.
    :param _strategy: one of "mmap", "write" or "auto"
    """
    if _strategy not in ("auto", "write", "mmap"):
        raise TValueError("Invalid parameter _strategy: only 'write' / 'mmap' "
                          "/ 'auto' are allowed")
    dest = os.path.expanduser(dest)
    if os.path.exists(dest):
        # raise ValueError("Path %s already exists" % dest)
        pass
    else:
        os.makedirs(dest)

    if self.internal.isview:
        # Materialize before saving
        self._dt = self.internal.materialize()

    metafile = os.path.join(dest, "_meta.nff")
    with _builtin_open(metafile, "w", encoding="utf-8") as out:
        out.write("# NFF1\n")
        out.write("# nrows = %d\n" % self.nrows)
        out.write('filename,stype,meta,colname\n')
        l = len(str(self.ncols))
        for i in range(self.ncols):
            filename = "c%0*d" % (l, i + 1)
            colname = self.names[i].replace('"', '""')
            _col = self.internal.column(i)
            stype = _col.stype
            meta = _col.meta
            if stype == dt.stype.obj64:
                dtwarn("Column %r of type obj64 was not saved" % self.names[i])
                continue
            if meta is None:
                meta = ""
            out.write('%s,%s,%s,"%s"\n' % (filename, stype.code, meta, colname))
            filename = os.path.join(dest, filename)
            _col.save_to_disk(filename, _strategy)



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
        with _builtin_open(metafile, encoding="utf-8") as inp:
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

        f0 = dt.fread(metafile, sep=",",
                      columns=lambda i, name, type: (name, str))
        f1 = f0(select=["filename", "stype", "meta"])
        colnames = f0["colname"].topython()[0]
        _dt = core.datatable_load(f1.internal, nrows, path)
        df = dt.Frame(_dt, names=colnames)
        assert df.nrows == nrows, "Wrong number of rows read: %d" % df.nrows
        return df
    finally:
        os.chdir(cwd)
