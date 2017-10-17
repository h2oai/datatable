#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import os
import tempfile
import warnings
import psutil
from typing import List, Union, Callable, Tuple, Dict, Set

# noinspection PyUnresolvedReferences
import datatable.lib._datatable as c
from datatable.dt import DataTable
from datatable.utils.typechecks import typed, U, TValueError, TTypeError
from datatable.utils.terminal import term
from datatable.utils.misc import (normalize_slice, normalize_range,
                                  humanize_bytes)
from datatable.utils.misc import plural_form as plural


_log_color = term.bright_black

TColName = Union[str, type(...)]
TColType = Union[str, type]
TColumnsSpec = Union[
    None,
    slice,
    range,
    Set[str],
    List[Union[None, str, Tuple[str, TColType]]],
    Dict[TColName, Union[None, TColName, Tuple[TColName, TColType]]],
    Callable[[str], Union[None, bool, str]],
    Callable[[int, str], Union[None, bool, str]],
    Callable[[int, str, str], Union[None, bool, str, Tuple[str, TColType]]]
]


@typed()
def fread(filename: str = None,
          text: str = None,
          columns: TColumnsSpec = None,
          sep: str = None,
          max_nrows: int = None,
          header: bool = None,
          na_strings: List[str] = None,
          verbose: bool = False,
          fill: bool = False,
          show_progress: bool = None,
          encoding: str = None,
          skip_to_string: str = None,
          skip_lines: int = None,
          save_to: str = None,
          logger=None,
          **extra) -> DataTable:
    freader = FReader(filename=filename,
                      text=text,
                      columns=columns,
                      sep=sep,
                      max_nrows=max_nrows,
                      header=header,
                      na_strings=na_strings,
                      fill=fill,
                      show_progress=show_progress,
                      encoding=encoding,
                      skip_to_string=skip_to_string,
                      skip_lines=skip_lines,
                      verbose=verbose,
                      save_to=save_to,
                      logger=logger,
                      **extra)
    return freader.read()



class FReader(object):
    """
    Parser object for reading CSV files.
    """

    def __init__(self, filename=None, text=None, columns=None, sep=None,
                 max_nrows=None, header=None, na_strings=None, verbose=False,
                 fill=False, show_progress=None, encoding=None,
                 skip_to_string=None, skip_lines=None, save_to=None,
                 logger=None, **args):
        self._filename = None   # type: str
        self._tempfile = None   # type: str
        self._tempdir = None    # type: str
        self._text = None       # type: str
        self._sep = None        # type: str
        self._maxnrows = None   # type: int
        self._header = None     # type: bool
        self._nastrings = []    # type: List[str]
        self._verbose = False   # type: bool
        self._fill = False      # type: bool
        self._show_progress = True  # type: bool
        self._encoding = encoding
        self._skip_lines = None
        self._skip_to_string = None
        self._columns = None
        self._save_to = save_to
        self._logger = None

        self._colnames = None
        self._bar_ends = None
        self._bar_symbols = None

        if show_progress is None:
            show_progress = term.is_a_tty
        if na_strings is None:
            na_strings = ["NA"]
        self.verbose = verbose
        self.text = text
        self.filename = filename
        self.columns = columns
        self.sep = sep
        self.max_nrows = max_nrows
        self.header = header
        self.na_strings = na_strings
        self.fill = fill
        self.show_progress = show_progress
        self.skip_to_string = skip_to_string
        self.skip_lines = skip_lines
        self.logger = logger

        if "separator" in args:
            self.sep = args.pop("separator")
        if "progress_fn" in args:
            progress = args.pop("progress_fn")
            if not callable(progress):
                raise TTypeError("`progress_fn` argument should be a function")
            self._progress = progress
        if args:
            raise TTypeError("Unknown argument(s) %r in FReader(...)"
                             % list(args.keys()))


    @property
    def filename(self):
        if self._text:
            return None
        return self._tempfile or self._filename

    @filename.setter
    @typed(filename=U(str, None))
    def filename(self, filename):
        if not filename:
            self._filename = None
        else:
            if filename.startswith("~"):
                filename = os.path.expanduser(filename)
            self._check_file(filename)
            self._filename = filename


    @property
    def text(self):
        return self._text

    @text.setter
    @typed(text=U(str, None))
    def text(self, text):
        self._text = text or None


    @property
    def columns(self):
        return self._columns

    @columns.setter
    @typed(columns=TColumnsSpec)
    def columns(self, columns):
        self._columns = columns or None


    @property
    def sep(self):
        return self._sep

    @sep.setter
    @typed(sep=U(str, None))
    def sep(self, sep):
        if not sep:
            self._sep = None
        else:
            if len(sep) > 1:
                raise TValueError("Multi-character separator %r not supported"
                                  % sep)
            if ord(sep) > 127:
                raise TValueError("The separator should be an ASCII character, "
                                  "got %r" % sep)
            self._sep = sep


    @property
    def max_nrows(self):
        return self._maxnrows

    @max_nrows.setter
    @typed(max_nrows=U(int, None))
    def max_nrows(self, max_nrows):
        if max_nrows is None or max_nrows < 0:
            max_nrows = -1
        self._maxnrows = max_nrows


    @property
    def header(self):
        return self._header

    @header.setter
    @typed(header=U(bool, None))
    def header(self, header):
        self._header = header


    @property
    def na_strings(self):
        return self._nastrings

    @na_strings.setter
    @typed()
    def na_strings(self, na_strings: List[str]):
        self._nastrings = na_strings


    @property
    def verbose(self):
        return self._verbose

    @verbose.setter
    @typed(verbose=bool)
    def verbose(self, verbose):
        self._verbose = verbose


    @property
    def fill(self):
        return self._fill

    @fill.setter
    @typed(fill=bool)
    def fill(self, fill):
        self._fill = fill


    @property
    def show_progress(self):
        return self._show_progress

    @show_progress.setter
    @typed(show_progress=bool)
    def show_progress(self, show_progress):
        self._show_progress = show_progress
        if show_progress:
            self._prepare_progress_bar()


    @property
    def skip_to_string(self):
        return self._skip_to_string

    @skip_to_string.setter
    @typed(s=U(str, None))
    def skip_to_string(self, s):
        self._skip_to_string = s or None


    @property
    def skip_lines(self):
        return self._skip_lines

    @skip_lines.setter
    @typed(n=U(int, None))
    def skip_lines(self, n):
        self._skip_lines = n


    @property
    def logger(self):
        return self._logger

    @logger.setter
    def logger(self, l):
        if l is None:
            # reset to the default logger
            l = _DefaultLogger()
        else:
            # If custom logger is provided, turn on the verbose mode
            self.verbose = True
        if not(hasattr(l, "debug") and callable(l.debug) and
               (hasattr(l.debug, "__func__") and
                l.debug.__func__.__code__.co_argcount >= 2 or
                type(l) is type and hasattr(l.debug, "__code__") and
                l.debug.__code__.co_argcount >= 1)):
            # Allow either an instance of a class with .debug(self, msg) method,
            # or the class itself, with static `.debug(msg)` method.
            raise TTypeError("`logger` parameter must be a class with method "
                             ".debug() taking at least one argument")
        self._logger = l


    def read(self):
        _dt = c.fread(self)
        dt = DataTable(_dt, colnames=self._colnames)
        if self._tempfile:
            if self._verbose:
                self.logger.debug("Removing temporary file %s\n"
                                  % self._tempfile)
            os.remove(self._tempfile)
            os.rmdir(self._tempdir)
        return dt


    #---------------------------------------------------------------------------

    def _progress(self, percent):
        """
        Invoked from the C level to inform that the file reading progress has
        reached the specified level (expressed as a number from 0 to 100.0).
        """
        bs = self._bar_symbols
        s0 = "Reading file: "
        s1 = " %3d%%" % int(percent)
        line_width = min(100, term.width)
        bar_width = line_width - len(s0) - len(s1) - 2
        progress = percent / 100
        n_chars = int(progress * bar_width + 0.001)
        frac_chars = int((progress * bar_width - n_chars) * len(bs))
        out = bs[-1] * n_chars
        out += bs[frac_chars - 1] if frac_chars > 0 else ""
        out += " " * (bar_width - len(out))
        endf, endl = self._bar_ends
        out = "\r" + s0 + endf + out + endl + s1
        print(_log_color(out), end="", flush=True)


    def _get_destination(self, estimated_size):
        """
        Invoked from the C level, this function will return either the name of
        the folder where the datatable is to be saved; or None, indicating that
        the datatable should be read into RAM. This function may also raise an
        exception if it determines that it cannot find a good strategy to
        handle a dataset of the requested size.
        """
        if self.verbose:
            self.logger.debug("  The DataTable is estimated to require %s\n"
                              % humanize_bytes(estimated_size))
        vm = psutil.virtual_memory()
        if self.verbose:
            self.logger.debug("  Memory available = %s (out of %s)\n"
                              % (humanize_bytes(vm.available),
                                 humanize_bytes(vm.total)))
        if (estimated_size < vm.available and self._save_to is None or
                self._save_to == "memory"):
            if self.verbose:
                self.logger.debug("  DataTable will be loaded into memory\n")
            return None
        else:
            if self._save_to:
                tmpdir = self._save_to
                os.makedirs(tmpdir)
            else:
                tmpdir = tempfile.mkdtemp()
            du = psutil.disk_usage(tmpdir)
            if self.verbose:
                self.logger.debug("  Free disk space on drive %s = %s\n"
                                  % (os.path.splitdrive(tmpdir)[0] or "/",
                                     humanize_bytes(du.free)))
            if du.free > estimated_size or self._save_to:
                if self.verbose:
                    self.logger.debug("  DataTable will be stored in %s\n"
                                      % tmpdir)
                return tmpdir
        raise RuntimeError("The DataTable is estimated to require at lest %s "
                           "of memory, and you don't have that much available "
                           "either in RAM or on a hard drive."
                           % humanize_bytes(estimated_size))


    def _prepare_progress_bar(self):
        tty_encoding = term._encoding
        self._bar_ends = "[]"
        self._bar_symbols = "#"
        if not tty_encoding:
            return
        s1 = "\u258F\u258E\u258D\u258C\u258B\u258A\u2589\u2588"
        s2 = "\u258C\u2588"
        s3 = "\u2588"
        for s in (s1, s2, s3):
            try:
                s.encode(tty_encoding)
                self._bar_ends = "||"
                self._bar_symbols = s
                return
            except UnicodeEncodeError:
                pass
            except LookupError:
                print("Warning: unknown encoding %s" % tty_encoding)


    def _check_file(self, filename):
        if "\x00" in filename:
            raise TValueError("Path %s contains NUL characters" % filename)
        if not os.path.exists(filename):
            xpath = os.path.abspath(filename)
            ypath = xpath
            while not os.path.exists(xpath):
                xpath = os.path.abspath(os.path.join(xpath, ".."))
            ypath = ypath[len(xpath):]
            raise TValueError("File %s`%s` does not exist" % (xpath, ypath))
        if not os.path.isfile(filename):
            raise TValueError("Path `%s` is not a file" % filename)

        ext = os.path.splitext(filename)[1]
        if ext == ".zip":
            import zipfile
            zf = zipfile.ZipFile(filename)
            zff = zf.namelist()
            if len(zff) > 1:
                raise TValueError("Zip file %s contains multiple compressed "
                                  "files: %r" % (filename, zff))
            if len(zff) == 0:
                raise TValueError("Zip file %s is empty" % filename)
            self._tempdir = tempfile.mkdtemp()
            if self._verbose:
                self.logger.debug("Extracting %s to temporary directory %s\n"
                                  % (filename, self._tempdir))
            self._tempfile = zf.extract(zff[0], path=self._tempdir)

        elif ext == ".gz":
            import gzip
            zf = gzip.GzipFile(filename)
            if self._verbose:
                self.logger.debug("Extracting %s into memory\n" % filename)
            self._text = zf.read()


    def _override_columns(self, colnames, coltypes):
        assert len(colnames) == len(coltypes)
        n = len(colnames)
        colspec = self._columns
        self._colnames = []

        if colspec is None:
            self._colnames = colnames
            return

        if isinstance(colspec, (slice, range)):
            if isinstance(colspec, slice):
                start, count, step = normalize_slice(colspec, n)
            else:
                t = normalize_range(colspec, n)
                if t is None:
                    raise TValueError("Invalid range iterator for a file with "
                                      "%d columns: %r" % (n, colspec))
                start, count, step = t
            if step <= 0:
                raise TValueError("Cannot use slice/range with negative step "
                                  "for column filter: %r" % colspec)
            for i in range(n):
                if (i - start) % step == 0 and i < start + count * step:
                    self._colnames.append(colnames[i])
                else:
                    coltypes[i] = 0
            return

        if isinstance(colspec, set):
            cols = set(colspec)  # Make a copy, so that we can modify it freely
            for i in range(n):
                if colnames[i] in cols:
                    cols.remove(colnames[i])
                    self._colnames.append(colnames[i])
                else:
                    coltypes[i] = 0
            if cols:
                warnings.warn("Column(s) %r not found in the input file" %
                              list(cols))
            return

        if isinstance(colspec, list):
            nn = len(colspec)
            if n != nn:
                raise TValueError("Input file contains %s, whereas `columns` "
                                  "parameter specifies only %s"
                                  % (plural(n, "column"), plural(nn, "column")))
            for i in range(n):
                entry = colspec[i]
                if entry is None:
                    coltypes[i] = 0
                elif isinstance(entry, str):
                    self._colnames.append(entry)
                else:
                    assert isinstance(entry, tuple)
                    newname, newtype = entry
                    self._colnames.append(newname)
                    coltypes[i] = _coltypes.get(newtype)
                    if not coltypes[i]:
                        raise TValueError("Unknown type %r used as an override "
                                          "for column %r" % (newtype, newname))
            return

        if isinstance(colspec, dict):
            for i in range(n):
                name = colnames[i]
                if name in colspec:
                    entry = colspec[name]
                else:
                    entry = colspec.get(..., ...)
                if entry is None:
                    coltypes[i] = 0
                elif entry is Ellipsis:
                    self._colnames.append(name)
                elif isinstance(entry, str):
                    self._colnames.append(entry)
                else:
                    assert isinstance(entry, tuple)
                    newname, newtype = entry
                    if newname is Ellipsis:
                        newname = name
                    self._colnames.append(newname)
                    coltypes[i] = _coltypes.get(newtype)
                    if not coltypes[i]:
                        raise TValueError("Unknown type %r used as an override "
                                          "for column %r" % (newtype, newname))

        if callable(colspec):
            nargs = colspec.__code__.co_argcount

            if nargs == 1:
                for i in range(n):
                    ret = colspec(colnames[i])
                    if ret is None or ret is False:
                        coltypes[i] = 0
                    elif ret is True:
                        self._colnames.append(colnames[i])
                    elif isinstance(ret, str):
                        self._colnames.append(ret)
                    else:
                        raise TValueError("Function passed as the `columns` "
                                          "argument was expected to return a "
                                          "`Union[None, bool, str]` but "
                                          "instead returned value %r" % ret)
                return

            if nargs == 2:
                for i in range(n):
                    ret = colspec(i, colnames[i])
                    if ret is None or ret is False:
                        coltypes[i] = 0
                    elif ret is True:
                        self._colnames.append(colnames[i])
                    elif isinstance(ret, str):
                        self._colnames.append(ret)
                    else:
                        raise TValueError("Function passed as the `columns` "
                                          "argument was expected to return a "
                                          "`Union[None, bool, str]` but "
                                          "instead returned value %r" % ret)
                return

            if nargs == 3:
                for i in range(n):
                    typ = _coltypes_strs[coltypes[i]]
                    ret = colspec(i, colnames[i], typ)
                    if ret is None or ret is False:
                        coltypes[i] = 0
                    elif ret is True:
                        self._colnames.append(colnames[i])
                    elif isinstance(ret, str):
                        self._colnames.append(ret)
                    elif isinstance(ret, tuple) and len(ret) == 2:
                        newname, newtype = ret
                        self._colnames.append(newname)
                        coltypes[i] = _coltypes.get(newtype)
                    else:
                        raise TValueError("Function passed as the `columns` "
                                          "argument was expected to return a "
                                          "`Union[None, bool, str, Tuple[str, "
                                          "Union[str, type]]]` but "
                                          "instead returned value %r" % ret)
                return

            raise RuntimeError("Unknown colspec: %r"  # pragma: no cover
                               % colspec)



class _DefaultLogger:
    def __init__(self):
        self._log_newline = False

    def debug(self, message):
        if self._log_newline:
            print("  ", end="")
        self._log_newline = message.endswith("\n")
        print(_log_color(message), end="", flush=True)


#-------------------------------------------------------------------------------
# Directly corresponds to `colType` enum in "fread.h"
_coltypes_strs = [
    "drop",      # 0
    "bool8",     # 1
    "int32",     # 2
    "int32x",    # 3
    "int64",     # 4
    "float32x",  # 5
    "float64",   # 6
    "float64e",  # 7
    "float64x",  # 8
    "str",       # 9
]

# FIXME !
_coltypes = {k: _coltypes_strs.index(v) for (k, v) in [
    (bool,       "bool8"),
    (int,        "int32"),
    (float,      "float64"),
    (str,        "str"),
    ("bool",     "bool8"),
    ("bool8",    "bool8"),
    ("int",      "int32"),
    ("int32",    "int32"),
    ("int32a",   "int32"),
    ("int32b",   "int32x"),
    ("int64",    "int64"),
    ("float32x", "float32x"),
    ("float",    "float64"),
    ("float64",  "float64"),
    ("float64e", "float64e"),
    ("float64x", "float64x"),
    ("str",      "str"),
    ("drop",     "drop"),
]}
