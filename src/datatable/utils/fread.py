#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2018-2020 H2O.ai
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
import enum
import glob
import os
import pathlib
import re
import shutil
import sys
import tempfile
import warnings

from datatable.lib import core
from datatable.exceptions import TypeError, ValueError, IOError, IOWarning
from datatable.utils.misc import normalize_slice, normalize_range
from datatable.utils.misc import plural_form as plural
from datatable.utils.misc import backticks_escape as escape
from datatable.types import stype, ltype
from datatable.xls import read_xls_workbook
from datatable.xlsx import read_xlsx_workbook


_url_regex = re.compile(r"(?:https?|ftp|file)://")
_glob_regex = re.compile(r"[\*\?\[\]]")




class TempFiles:
    def __init__(self, tempdir=None, logger=None):
        self._logger = logger
        self._tempfiles = []
        self._tempdir = tempdir
        self._tempdir_own = False

    def add(self, file):
        self._tempfiles.append(file)

    def create_temp_file(self):
        newfile = tempfile.mktemp(dir=self.tempdir)
        self._tempfiles.append(newfile)
        return newfile

    def __del__(self):
        for f in self._tempfiles:
            try:
                if self._logger:
                    self._logger.debug("Removing temporary file %s" % f)
                os.remove(f)
            except OSError as e:
                if self._logger:
                    self._logger.warning("Failed to remove a temporary file: %r" % e)
        if self._tempdir_own:
            shutil.rmtree(self._tempdir, ignore_errors=True)
        self._tempfiles = []
        self._tempdir_own = False

    @property
    def tempdir(self):
        if self._tempdir is None:
            self._tempdir = tempfile.mkdtemp()
            self._tempdir_own = True
        return self._tempdir





#-------------------------------------------------------------------------------
# Resolvers
#-------------------------------------------------------------------------------

def _resolve_source_any(src, tempfiles):
    logger = tempfiles._logger
    is_str = isinstance(src, str)
    if is_str or isinstance(src, bytes):
        # If there are any control characters (such as \n or \r) in the
        # text of `src`, then its type is "text".
        if len(src) >= 4096:
            if logger:
                logger.debug("Input is a string of length %d, "
                             "treating it as raw text" % len(src))
            return _resolve_source_text(src)
        else:
            fn = ord if is_str else int
            for ch in src:
                ccode = fn(ch)
                if ccode < 0x20:
                    if logger:
                        logger.debug("Input contains '\\x%02X', "
                                     "treating it as raw text" % ccode)
                    return _resolve_source_text(src)
            if is_str and re.match(_url_regex, src):
                if logger:
                    logger.debug("Input is a URL.")
                return _resolve_source_url(src, tempfiles)
            elif is_str and re.search(_glob_regex, src):
                if logger:
                    logger.debug("Input is a glob pattern.")
                return _resolve_source_list_of_files(glob.glob(src), tempfiles)
            else:
                if logger:
                    logger.debug("Input is assumed to be a file name.")
                return _resolve_source_file(src, tempfiles)
    elif isinstance(src, _pathlike) or hasattr(src, "read"):
        return _resolve_source_file(src, tempfiles)
    elif isinstance(src, (list, tuple)):
        return _resolve_source_list(src, tempfiles)
    else:
        raise TypeError("Unknown type for the first argument in fread: %r"
                        % type(src))


def _resolve_source_text(text):
    if not isinstance(text, (str, bytes)):
        raise TypeError("Invalid parameter `text` in fread: expected "
                        "str or bytes, got %r" % type(text))
    # src, file, fileno, text, result
    return ("<text>", None, None, text), None


def _resolve_source_file(file, tempfiles):
    logger = tempfiles._logger
    if isinstance(file, _pathlike):
        # `_pathlike` contains (str, bytes), and on Python 3.6 also
        # os.PathLike interface
        file = os.path.expanduser(file)
        file = os.fsdecode(file)
    elif hasattr(file, "read") and callable(file.read):
        out_src = None
        out_fileno = None
        out_text = None
        # A builtin `file` object, or something similar. We check for the
        # presence of `fileno` attribute, which will allow us to provide a
        # more direct access to the underlying file.
        # noinspection PyBroadException
        try:
            if sys.platform == "win32":
                raise Exception("Do not use file descriptors on Windows")
            # .fileno can be either a method, or a property
            # The implementation of .fileno may raise an exception too
            # (indicating that no file descriptor is available)
            fd = file.fileno
            if callable(fd):
                fd = fd()
            if not isinstance(fd, int) or fd <= 0:
                raise Exception
            out_fileno = fd
        except Exception:
            # Catching if: file.fileno is not defined, or is not an integer,
            # or raises an error, or returns a closed file descriptor
            rawtxt = file.read()
            out_text = rawtxt
        file = getattr(file, "name", None)
        if not isinstance(file, (str, bytes)):
            out_src = "<file>"
        elif isinstance(file, bytes):
            out_src = os.fsdecode(file)
        else:
            out_src = file
        return (out_src, None, out_fileno, out_text), None
    else:
        raise TypeError("Invalid parameter `file` in fread: expected a "
                        "str/bytes/PathLike, got %r" % type(file))
    # if `file` is not str, then `os.path.join(file, "..")` below will fail
    assert isinstance(file, str)
    if not os.path.exists(file):
        # File does not exist -- search up the tree for the first file that
        # does. This will allow us to provide a better error message to the
        # user; also if the first path component that exists is a file (not
        # a folder), then the user probably tries to specify a file within
        # an archive -- and this is not an error at all!
        xpath = os.path.abspath(file)
        ypath = xpath
        while not os.path.exists(xpath):
            xpath = os.path.abspath(os.path.join(xpath, ".."))
        ypath = ypath[len(xpath):]
        if os.path.isfile(xpath):
            return _resolve_archive(xpath, ypath, tempfiles)
        else:
            raise ValueError("File %s`%s` does not exist"
                             % (escape(xpath), escape(ypath)))
    if not os.path.isfile(file):
        raise ValueError("Path `%s` is not a file" % escape(file))
    return _resolve_archive(file, None, tempfiles)


def _resolve_source_list_of_files(files_list, tempfiles):
    files = []
    for s in files_list:
        if s is None: continue
        entry = _resolve_source_file(s, tempfiles)
        files.append(entry)
    # src, file, fileno, text, result
    return (None, None, None, None), files


def _resolve_source_list(srcs_list, tempfiles):
    out = []
    for s in srcs_list:
        if s is None: continue
        entry = _resolve_source_any(s, tempfiles)
        out.append(entry)
    # src, file, fileno, text, result
    return (None, None, None, None), out


def _resolve_archive(filename, subpath, tempfiles):
    logger = tempfiles._logger
    ext = os.path.splitext(filename)[1]
    if subpath and subpath[0] in ["/", "\\"]:
        subpath = subpath[1:]

    out_file = None
    out_text = None
    out_result = None
    # TODO: file extarction should be lazy
    if ext == ".zip":
        import zipfile
        with zipfile.ZipFile(filename) as zf:
            # MacOS is found guilty of adding extra files into the Zip archives
            # it creates. The files are hidden, and in the directory __MACOSX/.
            # We remove those files from the list, since they are not real user
            # files, and have an unknown binary format.
            zff = [name for name in zf.namelist()
                   if not(name.startswith("__MACOSX/") or name.endswith("/"))]
            if subpath:
                if subpath in zff:
                    filename = os.path.join(filename, subpath)
                    zff = [subpath]
                else:
                    raise IOError("File `%s` does not exist in archive `%s`"
                                   % (subpath, filename))
            extracted_files = []
            for zf_file in zff:
                if logger:
                    logger.debug("Extracting %s/%s to temporary directory %s"
                                 % (filename, zf_file, tempfiles.tempdir))
                newfile = zf.extract(zf_file, path=tempfiles.tempdir)
                srcname = os.path.join(filename, zf_file)
                tempfiles.add(newfile)
                extracted_files.append(((srcname, newfile, None, None), None))

            if len(extracted_files) == 1:
                out_file = extracted_files[0][0][1]
            else:
                return (None, None, None, None), extracted_files

    elif filename.endswith(".tar.gz") or filename.endswith(".tgz"):
        import tarfile
        zf = tarfile.open(filename, mode="r:gz")
        zff = [entry.name for entry in zf.getmembers() if entry.isfile()]
        if subpath:
            if subpath in zff:
                filename = os.path.join(filename, subpath)
                zff = [subpath]
            else:
                raise IOError("File `%s` does not exist in archive `%s`"
                              % (subpath, filename))
        extracted_files = []
        for entryname in zff:
            if logger:
                logger.debug("Extracting %s/%s to temporary directory %s"
                             % (filename, entryname, tempfiles.tempdir))
            newfile = tempfiles.create_temp_file()
            with zf.extractfile(entryname) as inp, open(newfile, "wb") as out:
                out.write(inp.read())
            srcname = os.path.join(filename, entryname)
            extracted_files.append(((srcname, newfile, None, None), None))
        if len(extracted_files) == 1:
            out_file = extracted_files[0][0][1]
        else:
            return (None, None, None, None), extracted_files

    elif ext == ".gz":
        import gzip
        zf = gzip.GzipFile(filename, mode="rb")
        if logger:
            logger.debug("Extracting %s into memory" % filename)
        out_text = zf.read()
        if logger:
            logger.debug("Extracted: size = %d" % len(out_text))

    elif ext == ".bz2":
        import bz2
        with bz2.open(filename, mode="rb") as zf:
            if logger:
                logger.debug("Extracting %s into memory" % filename)
            out_text = zf.read()
            if logger:
                logger.debug("Extracted: size = %d" % len(out_text))

    elif ext == ".xz":
        import lzma
        with lzma.open(filename, mode="rb") as zf:
            if logger:
                logger.debug("Extracting %s into memory" % filename)
            out_text = zf.read()
            if logger:
                logger.debug("Extracted: size = %d" % len(out_text))

    elif ext == ".xlsx":
        out_result = read_xlsx_workbook(filename, subpath)
        if subpath:
            filename = os.path.join(filename, subpath)


    elif ext == ".xls":
        out_result = read_xls_workbook(filename, subpath)
        if subpath:
            filename = os.path.join(filename, subpath)

    elif ext == ".jay":
        out_result = core.open_jay(filename)

    else:
        out_file = filename
    # src, file, fileno, text, result
    return (filename, out_file, None, out_text), out_result


def _resolve_source_cmd(cmd):
    import subprocess
    if not isinstance(cmd, str):
        raise TypeError("Invalid parameter `cmd` in fread: expected str, "
                        "got %r" % type(cmd))
    proc = subprocess.Popen(cmd, shell=True,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    msgout, msgerr = proc.communicate()
    ret = proc.returncode
    if ret:
        msgerr = msgerr.decode("utf-8", errors="replace").strip()
        raise ValueError("Shell command returned error code %r: `%s`"
                         % (ret, msgerr))
    else:
        # src, file, fileno, text, result
        return (cmd, None, None, msgout), None


# see `Source_Url::read()` instead
def _resolve_source_url(url, tempfiles, reporthook=None):
    assert url is not None
    import urllib.request
    targetfile = tempfiles.create_temp_file()
    encoded_url = urllib.parse.quote(url, safe=":/%")
    urllib.request.urlretrieve(encoded_url, filename=targetfile,
                               reporthook=reporthook)
    # src, file, fileno, text, result
    return (url, targetfile, None, None), None




#-------------------------------------------------------------------------------
# Process `columns` argument
#-------------------------------------------------------------------------------

def _override_columns(colspec, coldescs):
    if isinstance(colspec, (slice, range)):
        return _apply_columns_slice(colspec, coldescs)
    if isinstance(colspec, set):
        return _apply_columns_set(colspec, coldescs)
    if isinstance(colspec, (list, tuple)):
        return _apply_columns_list(colspec, coldescs)
    if isinstance(colspec, dict):
        return _apply_columns_dict(colspec, coldescs)
    if isinstance(colspec, (type, stype, ltype)):
        newcs = {colspec: slice(None)}
        return _apply_columns_dict(newcs, coldescs)
    if callable(colspec):
        return _apply_columns_function(colspec, coldescs)
    raise RuntimeError("Unknown colspec: %r" % colspec)  # pragma: no cover



def _apply_columns_slice(colslice, colsdesc):
    n = len(colsdesc)

    if isinstance(colslice, slice):
        start, count, step = normalize_slice(colslice, n)
    else:
        t = normalize_range(colslice, n)
        if t is None:
            raise ValueError("Invalid range iterator for a file with "
                             "%d columns: %r" % (n, colslice))
        start, count, step = t
    if step <= 0:
        raise ValueError("Cannot use slice/range with negative step "
                         "for column filter: %r" % colslice)

    colnames = [None] * count
    coltypes = [rtype.rdrop.value] * n
    for j in range(count):
        i = start + j * step
        colnames[j] = colsdesc[i].name
        coltypes[i] = rtype.rauto.value
    return (colnames, coltypes)


def _apply_columns_set(colset, colsdesc):
    n = len(colsdesc)
    # Make a copy of the `colset` in order to check whether all the
    # columns requested by the user were found, and issue a warning
    # otherwise.
    requested_cols = colset.copy()
    colnames = []
    coltypes = [rtype.rdrop.value] * n
    for i in range(n):
        colname = colsdesc[i][0]
        if colname in colset:
            requested_cols.discard(colname)
            colnames.append(colname)
            coltypes[i] = rtype.rauto.value
    if requested_cols:
        warnings.warn("Column(s) %r not found in the input file"
                      % list(requested_cols), category=IOWarning)
    return (colnames, coltypes)


def _apply_columns_list(collist, colsdesc):
    n = len(colsdesc)
    nn = len(collist)
    if n != nn:
        raise ValueError("Input contains %s, whereas `columns` "
                         "parameter specifies only %s"
                         % (plural(n, "column"), plural(nn, "column")))
    colnames = []
    coltypes = [rtype.rdrop.value] * n
    for i in range(n):
        entry = collist[i]
        if entry is None or entry is False:
            pass
        elif entry is True or entry is Ellipsis:
            colnames.append(colsdesc[i].name)
            coltypes[i] = rtype.rauto.value
        elif isinstance(entry, str):
            colnames.append(entry)
            coltypes[i] = rtype.rauto.value
        elif isinstance(entry, (stype, ltype, type)):
            colnames.append(colsdesc[i].name)
            coltypes[i] = _rtypes_map[entry].value
        elif isinstance(entry, tuple):
            newname, newtype = entry
            if newtype not in _rtypes_map:
                raise ValueError("Unknown type %r used as an override "
                                 "for column %r" % (newtype, newname))
            colnames.append(newname)
            coltypes[i] = _rtypes_map[newtype].value
        else:
            raise TypeError("Entry `columns[%d]` has invalid type %r"
                            % (i, entry.__class__.__name__))
    return (colnames, coltypes)


def _apply_columns_dict(colsdict, colsdesc):
    default_entry = colsdict.get(..., ...)
    colnames = []
    coltypes = [rtype.rdrop.value] * len(colsdesc)
    new_entries = {}
    for key, val in colsdict.items():
        if isinstance(key, (type, stype, ltype)):
            if isinstance(val, str):
                val = [val]
            if isinstance(val, slice):
                val = [colsdesc[i].name
                       for i in range(*val.indices(len(colsdesc)))]
            if isinstance(val, range):
                val = [colsdesc[i].name for i in val]
            if isinstance(val, (list, tuple, set)):
                for entry in val:
                    if not isinstance(entry, str):
                        raise TypeError(
                            "Type %s in the `columns` parameter should map"
                            " to a string or list of strings (column names)"
                            "; however it contains an entry %r"
                            % (key, entry))
                    if entry in colsdict:
                        continue
                    new_entries[entry] = key
            else:
                raise TypeError(
                    "Unknown entry %r for %s in `columns`" % (val, key))
    if new_entries:
        colsdict = {**colsdict, **new_entries}
    for i, desc in enumerate(colsdesc):
        name = desc.name
        entry = colsdict.get(name, default_entry)
        if entry is None:
            pass  # coltype is already "drop"
        elif entry is Ellipsis:
            colnames.append(name)
            coltypes[i] = rtype.rauto.value
        elif isinstance(entry, str):
            colnames.append(entry)
            coltypes[i] = rtype.rauto.value
        elif isinstance(entry, (stype, ltype, type)):
            colnames.append(name)
            coltypes[i] = _rtypes_map[entry].value
        elif isinstance(entry, tuple):
            newname, newtype = entry
            colnames.append(newname)
            coltypes[i] = _rtypes_map[newtype].value
            assert isinstance(newname, str)
            if not coltypes[i]:
                raise ValueError("Unknown type %r used as an override "
                                 "for column %r" % (newtype, newname))
        else:
            raise TypeError("Unknown value %r for column '%s' in "
                            "columns descriptor" % (entry, name))
    return (colnames, coltypes)


def _apply_columns_function(colsfn, colsdesc):
    res = colsfn(colsdesc)
    return _override_columns(res, colsdesc)




#-------------------------------------------------------------------------------
# Helper classes
#-------------------------------------------------------------------------------

# os.PathLike interface was added in Python 3.6
_pathlike = (str, bytes, os.PathLike)




#-------------------------------------------------------------------------------

# Corresponds to RT enum in "reader_parsers.h"
class rtype(enum.Enum):
    rdrop    = 0
    rauto    = 1
    rbool    = 2
    rint     = 3
    rint32   = 4
    rint64   = 5
    rfloat   = 6
    rfloat32 = 7
    rfloat64 = 8
    rstr     = 9
    rstr32   = 10
    rstr64   = 11


_rtypes_map = {
    None:          rtype.rdrop,
    ...:           rtype.rauto,
    bool:          rtype.rbool,
    int:           rtype.rint,
    float:         rtype.rfloat,
    str:           rtype.rstr,
    "drop":        rtype.rdrop,
    "bool":        rtype.rbool,
    "auto":        rtype.rauto,
    "bool8":       rtype.rbool,
    "logical":     rtype.rbool,
    "int":         rtype.rint,
    "integer":     rtype.rint,
    "int32":       rtype.rint32,
    "int64":       rtype.rint64,
    "float":       rtype.rfloat,
    "float32":     rtype.rfloat32,
    "float64":     rtype.rfloat64,
    "str":         rtype.rstr,
    "str32":       rtype.rstr32,
    "str64":       rtype.rstr64,
    stype.bool8:   rtype.rbool,
    stype.int32:   rtype.rint32,
    stype.int64:   rtype.rint64,
    stype.float32: rtype.rfloat32,
    stype.float64: rtype.rfloat64,
    stype.str32:   rtype.rstr32,
    stype.str64:   rtype.rstr64,
    ltype.bool:    rtype.rbool,
    ltype.int:     rtype.rint,
    ltype.real:    rtype.rfloat,
    ltype.str:     rtype.rstr,
}
