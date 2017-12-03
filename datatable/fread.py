#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import os
import pathlib
import psutil
import re
import tempfile
import urllib.request
import warnings
from typing import List, Union, Callable, Optional, Tuple, Dict, Set

# noinspection PyUnresolvedReferences,PyProtectedMember
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


def fread(
        # Input source
        anysource=None, *,
        file=None,
        text=None,
        cmd=None,
        url=None,

        columns: TColumnsSpec = None,
        sep: str = None,
        dec: str = ".",
        max_nrows: int = None,
        header: bool = None,
        na_strings: List[str] = None,
        verbose: bool = False,
        fill: bool = False,
        show_progress: bool = None,
        encoding: str = None,
        skip_to_string: str = None,
        skip_lines: int = None,
        skip_blank_lines: bool = True,
        strip_white: bool = True,
        quotechar: Optional[str] = '"',
        save_to: str = None,
        nthreads: int = None,
        logger=None,
        **extra) -> DataTable:
    params = {**locals(), **extra}
    del params["extra"]
    freader = TextReader(**params)
    return freader.read()



class TextReader(object):
    """
    Parser object for reading CSV files.
    """

    def __init__(self, anysource=None, *, file=None, text=None, url=None,
                 cmd=None, columns=None, sep=None,
                 max_nrows=None, header=None, na_strings=None, verbose=False,
                 fill=False, show_progress=None, encoding=None, dec=".",
                 skip_to_string=None, skip_lines=None, save_to=None,
                 nthreads=None, logger=None, skip_blank_lines=True,
                 strip_white=True, quotechar='"', **args):
        self._src = None            # type: str
        self._file = None           # type: str
        self._files = None          # type: List[str]
        self._fileno = None         # type: int
        self._tempfile = None       # type: str
        self._tempdir = None        # type: str
        self._text = None           # type: Union[str, bytes]
        self._sep = None            # type: str
        self._dec = None            # type: str
        self._maxnrows = None       # type: int
        self._header = None         # type: bool
        self._nastrings = []        # type: List[str]
        self._verbose = False       # type: bool
        self._fill = False          # type: bool
        self._show_progress = True  # type: bool
        self._encoding = encoding   # type: str
        self._quotechar = None      # type: str
        self._skip_lines = None
        self._skip_blank_lines = True
        self._skip_to_string = None
        self._strip_white = True
        self._columns = None
        self._save_to = save_to
        self._nthreads = nthreads
        self._logger = None

        self._colnames = None
        self._bar_ends = None
        self._bar_symbols = None

        if show_progress is None:
            show_progress = term.is_a_tty
        if na_strings is None:
            na_strings = ["NA"]
        if "_tempdir" in args:
            self._tempdir = args.pop("_tempdir")
        self.verbose = verbose
        self.logger = logger
        self._resolve_source(anysource, file, text, cmd, url)
        # self.text = text
        # self.file = file
        self.columns = columns
        self.sep = sep
        self.dec = dec
        self.max_nrows = max_nrows
        self.header = header
        self.na_strings = na_strings
        self.fill = fill
        self.show_progress = show_progress
        self.skip_to_string = skip_to_string
        self.skip_lines = skip_lines
        self.skip_blank_lines = skip_blank_lines
        self.strip_white = strip_white
        self.quotechar = quotechar

        if "separator" in args:
            self.sep = args.pop("separator")
        if "progress_fn" in args:
            progress = args.pop("progress_fn")
            if not callable(progress):
                raise TTypeError("`progress_fn` argument should be a function")
            self._progress = progress
        if "_tempfile" in args:
            self._tempfile = args.pop("_tempfile")
            self._file = self._tempfile
        if "_fileno" in args:
            self._fileno = args.pop("_fileno")
        if args:
            raise TTypeError("Unknown argument(s) %r in FReader(...)"
                             % list(args.keys()))



    #---------------------------------------------------------------------------
    # Parameter helpers
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
                    "provide the `%s` parameter." % args[0])
        self._resolve_source_any(anysource)
        self._resolve_source_text(text)
        self._resolve_source_file(file)
        self._resolve_source_cmd(cmd)
        self._resolve_source_url(url)


    def _resolve_source_any(self, src):
        if src is None:
            return
        if isinstance(src, (str, bytes)):
            # If there are any control characters (such as \n or \r) in the
            # text of `src`, then its type is "text".
            if len(src) >= 4096:
                if self.verbose:
                    self.logger.debug("  Source has length %d characters, and "
                                      "will be treated as raw text" % len(src))
                self._resolve_source_text(src)
            else:
                fn = ord if isinstance(src, str) else int
                for i, ch in enumerate(src):
                    ccode = fn(ch)
                    if ccode < 0x20:
                        if self.verbose:
                            self.logger.debug("  Character %d in the input is "
                                              "%r, treating input as raw text"
                                              % (i, chr(ccode)))
                        self._resolve_source_text(src)
                        return
                if (isinstance(src, str) and
                        re.match(r"(?:https?|ftp|file)://", src)):
                    if self.verbose:
                        self.logger.debug("  Input is a URL.")
                    self._resolve_source_url(src)
                else:
                    if self.verbose:
                        self.logger.debug("  Input is assumed to be a "
                                          "file name.")
                    self._resolve_source_file(src)
        elif isinstance(src, _pathlike) or hasattr(src, "read"):
            self._resolve_source_file(src)
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


    def _resolve_source_cmd(self, cmd):
        if cmd is None:
            return
        if not isinstance(cmd, str):
            raise TTypeError("Invalid parameter `cmd` in fread: expected str, "
                             "got %r" % type(cmd))
        result = os.popen(cmd)
        self._text = result.read()
        self._src = cmd


    def _resolve_source_url(self, url):
        if url is None:
            return
        tempdir = self._tempdir
        if tempdir is None:
            tempdir = tempfile.mkdtemp()
            self._tempdir = tempdir
        targetfile = tempfile.mktemp(dir=tempdir)
        urllib.request.urlretrieve(url, filename=targetfile)
        self._tempfile = targetfile
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
                              % (filename, zff))
            if len(zff) == 0:
                raise TValueError("Zip file %s is empty" % filename)
            self._tempdir = tempfile.mkdtemp()
            if self._verbose:
                self.logger.debug("  Extracting %s to temporary directory %s"
                                  % (filename, self._tempdir))
            self._tempfile = zf.extract(zff[0], path=self._tempdir)
            self._file = self._tempfile

        elif ext == ".gz":
            import gzip
            zf = gzip.GzipFile(filename)
            if self._verbose:
                self.logger.debug("  Extracting %s into memory" % filename)
            self._text = zf.read()

        elif ext == ".xz":
            import lzma
            zf = lzma.open(filename)
            if self._verbose:
                self.logger.debug("  Extracting %s into memory" % filename)
            self._text = zf.read()

        else:
            self._file = filename


    #---------------------------------------------------------------------------
    # Properties
    #---------------------------------------------------------------------------

    @property
    def src(self) -> str:
        """
        Name of the source of the data.

        This is a "portmanteau" value, intended mostly for displaying in error
        messages or verbose output. This value contains one of:
          - the name of the file requested by the user (possibly with minor
            modifications such as user/glob expansion). This never gives the
            name of a temporary file created by FRead internally.
          - URL text, if the user provided a url to fread.
          - special token "<file>" if an open file object was provided, but
            its file name is not known.
          - "<text>" if the input was a raw text.

        In order to determine the actual data source, the caller should query
        properties `.file`, `.text` and `.fileno`. One and only one of them
        will be non-None.
        """
        return self._src


    @property
    def file(self) -> Optional[str]:
        """
        Name of the file to be read.

        This always refers to the actual file, on a file system, that the
        underlying C code is expected to open and read. In particular, if the
        "original" source (as provided by the user) required processing the
        content and saving it into a temporary file, then this property will
        return the name of that temporary file. On the other hand, if the
        source is not a file, this property will return None. The returned value
        is always a string, even if the user passed a `bytes` object as `file=`
        argument to the constructor.
        """
        return self._file


    @property
    def text(self) -> Union[str, bytes, None]:
        """
        String/bytes object with the content to read.

        The returned value is None if the content should be read from file or
        some other source.
        """
        return self._text


    @property
    def fileno(self) -> Optional[int]:
        """
        File descriptor of an open file that should be read.

        This property is an equivalent way of specifying a file source. However
        instead of providing a file name, this property gives a file descriptor
        of a file that was already opened. The caller should not attempt to
        close this file.
        """
        return self._fileno


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
    def dec(self):
        return self._dec

    @dec.setter
    def dec(self, v):
        if v == "." or v == ",":
            self._dec = v
        else:
            raise ValueError("Only dec='.' or ',' are allowed")


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
    def skip_blank_lines(self) -> bool:
        return self._skip_blank_lines

    @skip_blank_lines.setter
    @typed()
    def skip_blank_lines(self, v: bool):
        self._skip_blank_lines = v


    @property
    def strip_white(self) -> bool:
        return self._strip_white

    @strip_white.setter
    @typed()
    def strip_white(self, v: bool):
        self._strip_white = v


    @property
    def quotechar(self):
        return self._quotechar

    @quotechar.setter
    @typed()
    def quotechar(self, v: Optional[str]):
        if v not in {None, "'", '"', "`"}:
            raise ValueError("quotechar should be one of [\"'`] or None")
        self._quotechar = v

    @property
    def nthreads(self):
        """Number of threads to use when reading the file."""
        return self._nthreads

    @nthreads.setter
    @typed(nth=U(int, None))
    def nthreads(self, nth):
        self._nthreads = nth


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
        dt = DataTable(_dt, names=self._colnames)
        if self._tempfile:
            if self._verbose:
                self.logger.debug("Removing temporary file %s"
                                  % self._tempfile)
            try:
                os.remove(self._tempfile)
                os.rmdir(self._tempdir)
            except OSError as e:
                self.logger.warn("Failed to remove temporary files: %r" % e)
        return dt


    #---------------------------------------------------------------------------

    def _progress(self, percent):
        """
        Invoked from the C level to inform that the file reading progress has
        reached the specified level (expressed as a number from 0 to 100.0).
        """
        bs = self._bar_symbols
        s0 = "  Reading file: "
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
        if self.verbose and estimated_size > 1:
            self.logger.debug("  The DataTable is estimated to require %s bytes"
                              % humanize_bytes(estimated_size))
        if estimated_size < 1024:
            return None
        vm = psutil.virtual_memory()
        if self.verbose:
            self.logger.debug("  Memory available = %s (out of %s)"
                              % (humanize_bytes(vm.available),
                                 humanize_bytes(vm.total)))
        if (estimated_size < vm.available and self._save_to is None or
                self._save_to == "memory"):
            if self.verbose:
                self.logger.debug("  DataTable will be loaded into memory")
            return None
        else:
            if self._save_to:
                tmpdir = self._save_to
                os.makedirs(tmpdir)
            else:
                tmpdir = tempfile.mkdtemp()
            du = psutil.disk_usage(tmpdir)
            if self.verbose:
                self.logger.debug("  Free disk space on drive %s = %s"
                                  % (os.path.splitdrive(tmpdir)[0] or "/",
                                     humanize_bytes(du.free)))
            if du.free > estimated_size or self._save_to:
                if self.verbose:
                    self.logger.debug("  DataTable will be stored in %s"
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
            # Make a copy of the `colspec`, in order to check whether all the
            # columns requested by the user were found, and issue a warning
            # otherwise.
            colsfound = set(colspec)
            for i in range(n):
                if colnames[i] in colspec:
                    if colnames[i] in colsfound:
                        colsfound.remove(colnames[i])
                    self._colnames.append(colnames[i])
                else:
                    coltypes[i] = 0
            if colsfound:
                warnings.warn("Column(s) %r not found in the input file" %
                              list(colsfound))
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
    def debug(self, message):
        print(_log_color(message), flush=True)

    def warn(self, message):
        warnings.warn(message)


# os.PathLike interface was added in Python 3.6
_pathlike = (str, bytes, os.PathLike) if hasattr(os, "PathLike") else \
            (str, bytes)




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
