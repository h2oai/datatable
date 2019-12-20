#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import enum
import glob
import os
import pathlib
import re
import shutil
import tempfile
import warnings

from datatable.lib import core
from datatable.utils.typechecks import (TValueError, TTypeError,
                                        DatatableWarning)
from datatable.utils.terminal import term
from datatable.utils.misc import (normalize_slice, normalize_range,
                                  humanize_bytes)
from datatable.utils.misc import plural_form as plural
from datatable.types import stype, ltype
from datatable.xls import read_xls_workbook


_url_regex = re.compile(r"(?:https?|ftp|file)://")
_glob_regex = re.compile(r"[\*\?\[\]]")
# _psutil_load_attempted = False


def fread(
        # Input source
        anysource=None, *,
        file=None,
        text=None,
        cmd=None,
        url=None,

        columns=None,
        sep=None,
        dec=".",
        max_nrows=None,
        header=None,
        na_strings=None,
        verbose=False,
        fill=False,
        encoding=None,
        skip_to_string=None,
        skip_to_line=None,
        skip_blank_lines=False,
        strip_whitespace=True,
        quotechar='"',
        save_to=None,
        nthreads=None,
        logger=None,
        **extra):
    params = {**locals(), **extra}
    del params["extra"]
    freader = GenericReader(**params)
    return freader.read()



class GenericReader(object):
    """
    Parser object for reading CSV files.
    """

    def __init__(self, anysource=None, *, file=None, text=None, url=None,
                 cmd=None, columns=None, sep=None,
                 max_nrows=None, header=None, na_strings=None, verbose=False,
                 fill=False, encoding=None, dec=".",
                 skip_to_string=None, skip_to_line=None, save_to=None,
                 nthreads=None, logger=None, skip_blank_lines=True,
                 strip_whitespace=True, quotechar='"', **args):
        self._src = (anysource, file, text, cmd, url)
        self._file = None
        self._files = None
        self._fileno = None
        self._tempfiles = []
        self._tempdir = None
        self._tempdir_own = False
        self._text = None
        self._result = None

        self._sep = args.pop("separator", sep)
        self._dec = dec
        self._maxnrows = max_nrows
        self._header = header
        self._nastrings = na_strings
        self._verbose = verbose
        self._fill = fill
        self._encoding = encoding
        self._quotechar = quotechar
        self._skip_to_line = skip_to_line
        self._skip_blank_lines = skip_blank_lines
        self._skip_to_string = skip_to_string
        self._strip_whitespace = strip_whitespace
        self._columns = columns
        # self._save_to = save_to
        self._nthreads = nthreads
        self._tempdir = args.pop("_tempdir", None)
        self._logger = logger
        if verbose and not logger:
            self._logger = _DefaultLogger()
        if args:
            raise TTypeError("Unknown argument(s) %r in FReader(...)"
                             % list(args.keys()))


    #---------------------------------------------------------------------------
    # Resolve from various sources
    #---------------------------------------------------------------------------

    def _resolve_source(self, anysource, file, text, cmd, url):
        args = (["any"] * (anysource is not None) +
                ["file"] * (file is not None) +
                ["text"] * (text is not None) +
                ["cmd"] * (cmd is not None) +
                ["url"] * (url is not None))
        if len(args) == 0:
            raise TValueError(
                "No input source for `fread` was given. Please specify one of "
                "the parameters `file`, `text`, `url`, or `cmd`")
        if len(args) > 1:
            if anysource is None:
                raise TValueError(
                    "Both parameters `%s` and `%s` cannot be passed to fread "
                    "simultaneously." % (args[0], args[1]))
            else:
                args.remove("any")
                raise TValueError(
                    "When an unnamed argument is passed, it is invalid to also "
                    "provide the `%s` parameter." % (args[0], ))
        self._resolve_source_any(anysource)
        self._resolve_source_text(text)
        self._resolve_source_file(file)
        self._resolve_source_cmd(cmd)
        self._resolve_source_url(url)


    def _resolve_source_any(self, src):
        if src is None:
            return
        is_str = isinstance(src, str)
        if is_str or isinstance(src, bytes):
            # If there are any control characters (such as \n or \r) in the
            # text of `src`, then its type is "text".
            if len(src) >= 4096:
                if self._verbose:
                    self._logger.debug("Input is a string of length %d, "
                                      "treating it as raw text" % len(src))
                self._resolve_source_text(src)
            else:
                fn = ord if is_str else int
                for ch in src:
                    ccode = fn(ch)
                    if ccode < 0x20:
                        if self._verbose:
                            self._logger.debug("Input contains '\\x%02X', "
                                              "treating it as raw text" % ccode)
                        self._resolve_source_text(src)
                        return
                if is_str and re.match(_url_regex, src):
                    if self._verbose:
                        self._logger.debug("Input is a URL.")
                    self._resolve_source_url(src)
                elif is_str and re.search(_glob_regex, src):
                    if self._verbose:
                        self._logger.debug("Input is a glob pattern.")
                    self._resolve_source_list_of_files(glob.glob(src))
                else:
                    if self._verbose:
                        self._logger.debug("Input is assumed to be a "
                                          "file name.")
                    self._resolve_source_file(src)
        elif isinstance(src, _pathlike) or hasattr(src, "read"):
            self._resolve_source_file(src)
        elif isinstance(src, (list, tuple)):
            self._resolve_source_list_of_files(src)
        else:
            raise TTypeError("Unknown type for the first argument in fread: %r"
                             % type(src))


    def _resolve_source_text(self, text):
        if text is None:
            return
        if not isinstance(text, (str, bytes)):
            raise TTypeError("Invalid parameter `text` in fread: expected "
                             "str or bytes, got %r" % type(text))
        self._text = text
        self._src = "<text>"


    def _resolve_source_file(self, file):
        if file is None:
            return
        if isinstance(file, _pathlike):
            # `_pathlike` contains (str, bytes), and on Python 3.6 also
            # os.PathLike interface
            file = os.path.expanduser(file)
            file = os.fsdecode(file)
        elif isinstance(file, pathlib.Path):
            # This is only for Python 3.5; in Python 3.6 pathlib.Path implements
            # os.PathLike interface and is included in `_pathlike`.
            file = file.expanduser()
            file = str(file)
        elif hasattr(file, "read") and callable(file.read):
            # A builtin `file` object, or something similar. We check for the
            # presence of `fileno` attribute, which will allow us to provide a
            # more direct access to the underlying file.
            # noinspection PyBroadException
            try:
                # .fileno can be either a method, or a property
                # The implementation of .fileno may raise an exception too
                # (indicating that no file descriptor is available)
                fd = file.fileno
                if callable(fd):
                    fd = fd()
                if not isinstance(fd, int) or fd <= 0:
                    raise Exception
                self._fileno = fd
            except Exception:
                # Catching if: file.fileno is not defined, or is not an integer,
                # or raises an error, or returns a closed file descriptor
                rawtxt = file.read()
                self._text = rawtxt
            file = getattr(file, "name", None)
            if not isinstance(file, (str, bytes)):
                self._src = "<file>"
            elif isinstance(file, bytes):
                self._src = os.fsdecode(file)
            else:
                self._src = file
            return
        else:
            raise TTypeError("Invalid parameter `file` in fread: expected a "
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
                self._resolve_archive(xpath, ypath)
                return
            else:
                raise TValueError("File %s`%s` does not exist" % (xpath, ypath))
        if not os.path.isfile(file):
            raise TValueError("Path `%s` is not a file" % file)
        self._src = file
        self._resolve_archive(file)


    def _resolve_source_list_of_files(self, files_list):
        self._files = []
        for s in files_list:
            self._resolve_source_file(s)
            entry = (self._src, self._file, self._fileno, self._text)
            self._files.append(entry)


    def _resolve_source_cmd(self, cmd):
        import subprocess
        if cmd is None:
            return
        if not isinstance(cmd, str):
            raise TTypeError("Invalid parameter `cmd` in fread: expected str, "
                             "got %r" % type(cmd))
        proc = subprocess.Popen(cmd, shell=True,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
        ret = proc.wait()
        msgout, msgerr = proc.communicate()
        if ret:
            msgerr = msgerr.decode("utf-8", errors="replace").strip()
            raise TValueError("Shell command returned error code %r: `%s`"
                              % (ret, msgerr))
        else:
            self._text = msgout
            self._src = cmd


    def _resolve_source_url(self, url):
        if url is not None:
            import urllib.request
            targetfile = tempfile.mktemp(dir=self.tempdir)
            urllib.request.urlretrieve(url, filename=targetfile)
            self._tempfiles.append(targetfile)
            self._file = targetfile
            self._src = url


    def _resolve_archive(self, filename, subpath=None):
        ext = os.path.splitext(filename)[1]
        if subpath and subpath[0] == "/":
            subpath = subpath[1:]

        if ext == ".zip":
            import zipfile
            zf = zipfile.ZipFile(filename)
            # MacOS is found guilty of adding extra files into the Zip archives
            # it creates. The files are hidden, and in the directory __MACOSX/.
            # We remove those files from the list, since they are not real user
            # files, and have an unknown binary format.
            zff = [name for name in zf.namelist()
                   if not(name.startswith("__MACOSX/") or name.endswith("/"))]
            if subpath:
                if subpath in zff:
                    zff = [subpath]
                else:
                    raise TValueError("File `%s` does not exist in archive "
                                      "`%s`" % (subpath, filename))
            if len(zff) > 1:
                warnings.warn("Zip file %s contains multiple compressed "
                              "files: %r. Only the first of them will be used."
                              % (filename, zff), category=FreadWarning)
            if len(zff) == 0:
                raise TValueError("Zip file %s is empty" % filename)
            if self._verbose:
                self._logger.debug("Extracting %s to temporary directory %s"
                                  % (filename, self.tempdir))
            self._tempfiles.append(zf.extract(zff[0], path=self.tempdir))
            self._file = self._tempfiles[-1]

        elif ext == ".gz":
            import gzip
            zf = gzip.GzipFile(filename, mode="rb")
            if self._verbose:
                self._logger.debug("Extracting %s into memory" % filename)
            self._text = zf.read()
            if self._verbose:
                self._logger.debug("Extracted: size = %d" % len(self._text))

        elif ext == ".bz2":
            import bz2
            with bz2.open(filename, mode="rb") as zf:
                if self._verbose:
                    self._logger.debug("Extracting %s into memory" % filename)
                self._text = zf.read()
                if self._verbose:
                    self._logger.debug("Extracted: size = %d" % len(self._text))

        elif ext == ".xz":
            import lzma
            with lzma.open(filename, mode="rb") as zf:
                if self._verbose:
                    self._logger.debug("Extracting %s into memory" % filename)
                self._text = zf.read()
                if self._verbose:
                    self._logger.debug("Extracted: size = %d" % len(self._text))

        elif ext == ".xlsx" or ext == ".xls":
            self._result = read_xls_workbook(filename, subpath)

        elif ext == ".jay":
            self._result = core.open_jay(filename)

        else:
            self._file = filename


    @property
    def tempdir(self):
        if self._tempdir is None:
            self._tempdir = tempfile.mkdtemp()
            self._tempdir_own = True
        return self._tempdir


    #---------------------------------------------------------------------------

    def read(self):
        try:
            if self._verbose:
                self._logger.debug("[1] Prepare for reading")
            self._resolve_source(*self._src)
            if self._result:
                return self._result
            if self._files:
                res = {}
                for src, filename, fileno, txt in self._files:
                    self._src = src
                    self._file = filename
                    self._fileno = fileno
                    self._txt = txt
                    try:
                        res[src] = core.gread(self)
                    except Exception as e:
                        res[src] = e
                return res
            else:
                return core.gread(self)
        finally:
            self._clear_temporary_files()


    #---------------------------------------------------------------------------

    # def _get_destination(self, estimated_size):
    #     """
    #     Invoked from the C level, this function will return either the name of
    #     the folder where the datatable is to be saved; or None, indicating that
    #     the datatable should be read into RAM. This function may also raise an
    #     exception if it determines that it cannot find a good strategy to
    #     handle a dataset of the requested size.
    #     """
    #     global _psutil_load_attempted
    #     if not _psutil_load_attempted:
    #         _psutil_load_attempted = True
    #         try:
    #             import psutil
    #         except ImportError:
    #             psutil = None
    #
    #     if self._verbose and estimated_size > 1:
    #         self._logger.debug("The Frame is estimated to require %s bytes"
    #                           % humanize_bytes(estimated_size))
    #     if estimated_size < 1024 or psutil is None:
    #         return None
    #     vm = psutil.virtual_memory()
    #     if self._verbose:
    #         self._logger.debug("Memory available = %s (out of %s)"
    #                           % (humanize_bytes(vm.available),
    #                              humanize_bytes(vm.total)))
    #     if (estimated_size < vm.available and self._save_to is None or
    #             self._save_to == "memory"):
    #         if self._verbose:
    #             self._logger.debug("Frame will be loaded into memory")
    #         return None
    #     else:
    #         if self._save_to:
    #             tmpdir = self._save_to
    #             os.makedirs(tmpdir)
    #         else:
    #             tmpdir = tempfile.mkdtemp()
    #         du = psutil.disk_usage(tmpdir)
    #         if self._verbose:
    #             self._logger.debug("Free disk space on drive %s = %s"
    #                               % (os.path.splitdrive(tmpdir)[0] or "/",
    #                                  humanize_bytes(du.free)))
    #         if du.free > estimated_size or self._save_to:
    #             if self._verbose:
    #                 self._logger.debug("Frame will be stored in %s"
    #                                   % tmpdir)
    #             return tmpdir
    #     raise RuntimeError("The Frame is estimated to require at lest %s "
    #                        "of memory, and you don't have that much available "
    #                        "either in RAM or on a hard drive."
    #                        % humanize_bytes(estimated_size))


    def _clear_temporary_files(self):
        for f in self._tempfiles:
            try:
                if self._verbose:
                    self._logger.debug("Removing temporary file %s" % f)
                os.remove(f)
            except OSError as e:
                self._logger.warning("Failed to remove a temporary file: %r" % e)
        if self._tempdir_own:
            shutil.rmtree(self._tempdir, ignore_errors=True)
        self._tempfiles = []
        self._tempdir_own = False




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
            raise TValueError("Invalid range iterator for a file with "
                              "%d columns: %r" % (n, colslice))
        start, count, step = t
    if step <= 0:
        raise TValueError("Cannot use slice/range with negative step "
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
                      % list(requested_cols), category=FreadWarning)
    return (colnames, coltypes)


def _apply_columns_list(collist, colsdesc):
    n = len(colsdesc)
    nn = len(collist)
    if n != nn:
        raise TValueError("Input contains %s, whereas `columns` "
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
                raise TValueError("Unknown type %r used as an override "
                                  "for column %r" % (newtype, newname))
            colnames.append(newname)
            coltypes[i] = _rtypes_map[newtype].value
        else:
            raise TTypeError("Entry `columns[%d]` has invalid type %r"
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
                        raise TTypeError(
                            "Type %s in the `columns` parameter should map"
                            " to a string or list of strings (column names)"
                            "; however it contains an entry %r"
                            % (key, entry))
                    if entry in colsdict:
                        continue
                    new_entries[entry] = key
            else:
                raise TTypeError(
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
                raise TValueError("Unknown type %r used as an override "
                                  "for column %r" % (newtype, newname))
        else:
            raise TTypeError("Unknown value %r for column '%s' in "
                             "columns descriptor" % (entry, name))
    return (colnames, coltypes)


def _apply_columns_function(colsfn, colsdesc):
    res = colsfn(colsdesc)
    return _override_columns(res, colsdesc)




#-------------------------------------------------------------------------------
# Helper classes
#-------------------------------------------------------------------------------

class FreadWarning(DatatableWarning):
    pass


class _DefaultLogger:
    def debug(self, message):
        if message[0] != "[":
            message = "  " + message
        print(term.color("bright_black", message), flush=True)

    def warning(self, message):
        warnings.warn(message, category=FreadWarning)


# os.PathLike interface was added in Python 3.6
_pathlike = (str, bytes, os.PathLike) if hasattr(os, "PathLike") else \
            (str, bytes)




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
