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
from typing import List, Union, Optional

from datatable.lib import core
from datatable.frame import Frame
from datatable.options import options
from datatable.utils.typechecks import (typed, U, TValueError, TTypeError,
                                        DatatableWarning)
from datatable.utils.terminal import term
from datatable.utils.misc import (normalize_slice, normalize_range,
                                  humanize_bytes)
from datatable.utils.misc import plural_form as plural
from datatable.types import stype, ltype
from datatable.xls import read_xls_workbook


_log_color = term.bright_black
_url_regex = re.compile(r"(?:https?|ftp|file)://")
_glob_regex = re.compile(r"[\*\?\[\]]")
_psutil_load_attempted = False


def fread(
        # Input source
        anysource=None, *,
        file=None,
        text=None,
        cmd=None,
        url=None,

        columns=None,
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
        skip_to_line: int = None,
        skip_blank_lines: bool = False,
        strip_whitespace: bool = True,
        quotechar: Optional[str] = '"',
        save_to: str = None,
        nthreads: int = None,
        logger=None,
        **extra) -> Frame:
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
                 fill=False, show_progress=None, encoding=None, dec=".",
                 skip_to_string=None, skip_to_line=None, save_to=None,
                 nthreads=None, logger=None, skip_blank_lines=True,
                 strip_whitespace=True, quotechar='"', **args):
        self._src = None            # type: str
        self._file = None           # type: str
        self._files = None          # type: List[str]
        self._fileno = None         # type: int
        self._tempfiles = []        # type: List[str]
        self._tempdir = None        # type: str
        self._tempdir_own = False   # type: bool
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
        self._skip_to_line = None
        self._skip_blank_lines = True
        self._skip_to_string = None
        self._strip_whitespace = True
        self._columns = None
        self._save_to = save_to
        self._nthreads = nthreads
        self._logger = None

        self._colnames = None
        self._bar_ends = None
        self._bar_symbols = None
        self._result = None

        if show_progress is None:
            show_progress = term.is_a_tty
        if na_strings is None:
            na_strings = ["NA"]
        if "_tempdir" in args:
            self.tempdir = args.pop("_tempdir")
        self.verbose = verbose
        self.logger = logger
        if verbose:
            self.logger.debug("[1] Prepare for reading")
        self._resolve_source(anysource, file, text, cmd, url)
        self.columns = columns
        self.sep = sep
        self.dec = dec
        self.max_nrows = max_nrows
        self.header = header
        self.na_strings = na_strings
        self.fill = fill
        self.show_progress = show_progress
        self.skip_to_string = skip_to_string
        self.skip_to_line = skip_to_line
        self.skip_blank_lines = skip_blank_lines
        self.strip_whitespace = strip_whitespace
        self.quotechar = quotechar

        if "separator" in args:
            self.sep = args.pop("separator")
        if "progress_fn" in args:
            progress = args.pop("progress_fn")
            if progress is None or callable(progress):
                self._progress = progress
            else:
                raise TTypeError("`progress_fn` argument should be a function")
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
                if self.verbose:
                    self.logger.debug("Input is a string of length %d, "
                                      "treating it as raw text" % len(src))
                self._resolve_source_text(src)
            else:
                fn = ord if is_str else int
                for ch in src:
                    ccode = fn(ch)
                    if ccode < 0x20:
                        if self.verbose:
                            self.logger.debug("Input contains '\\x%02X', "
                                              "treating it as raw text" % ccode)
                        self._resolve_source_text(src)
                        return
                if is_str and re.match(_url_regex, src):
                    if self.verbose:
                        self.logger.debug("Input is a URL.")
                    self._resolve_source_url(src)
                elif is_str and re.search(_glob_regex, src):
                    if self.verbose:
                        self.logger.debug("Input is a glob pattern.")
                    self._resolve_source_list_of_files(glob.glob(src))
                else:
                    if self.verbose:
                        self.logger.debug("Input is assumed to be a "
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
        if cmd is None:
            return
        if not isinstance(cmd, str):
            raise TTypeError("Invalid parameter `cmd` in fread: expected str, "
                             "got %r" % type(cmd))
        result = os.popen(cmd)
        self._text = result.read()
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
                self.logger.warning("Zip file %s contains multiple compressed "
                                    "files: %r. Only the first of them will be "
                                    "used." % (filename, zff))
            if len(zff) == 0:
                raise TValueError("Zip file %s is empty" % filename)
            self._tempdir = tempfile.mkdtemp()
            if self._verbose:
                self.logger.debug("Extracting %s to temporary directory %s"
                                  % (filename, self._tempdir))
            self._tempfiles.append(zf.extract(zff[0], path=self._tempdir))
            self._file = self._tempfiles[-1]

        elif ext == ".gz":
            import gzip
            zf = gzip.GzipFile(filename, mode="rb")
            if self._verbose:
                self.logger.debug("Extracting %s into memory" % filename)
            self._text = zf.read()
            if self._verbose:
                self.logger.debug("Extracted: size = %d" % len(self._text))

        elif ext == ".bz2":
            import bz2
            zf = bz2.open(filename, mode="rb")
            if self._verbose:
                self.logger.debug("Extracting %s into memory" % filename)
            self._text = zf.read()
            if self._verbose:
                self.logger.debug("Extracted: size = %d" % len(self._text))

        elif ext == ".xz":
            import lzma
            zf = lzma.open(filename, mode="rb")
            if self._verbose:
                self.logger.debug("Extracting %s into memory" % filename)
            self._text = zf.read()
            if self._verbose:
                self.logger.debug("Extracted: size = %d" % len(self._text))

        elif ext == ".xlsx" or ext == ".xls":
            self._result = read_xls_workbook(filename, subpath)

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
    def tempdir(self):
        if self._tempdir is None:
            self._tempdir = tempfile.mkdtemp()
            self._tempdir_own = True
        return self._tempdir

    @tempdir.setter
    @typed(tempdir=str)
    def tempdir(self, tempdir):
        self._tempdir = tempdir
        self._tempdir_own = False


    @property
    def columns(self):
        return self._columns

    @columns.setter
    def columns(self, columns):
        self._columns = columns or None


    @property
    def sep(self):
        return self._sep

    @sep.setter
    @typed(sep=U(str, None))
    def sep(self, sep):
        if sep == "":
            self._sep = "\n"
        elif not sep:
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
    def skip_to_line(self):
        return self._skip_to_line

    @skip_to_line.setter
    @typed(n=U(int, None))
    def skip_to_line(self, n):
        self._skip_to_line = n


    @property
    def skip_blank_lines(self) -> bool:
        return self._skip_blank_lines

    @skip_blank_lines.setter
    @typed()
    def skip_blank_lines(self, v: bool):
        self._skip_blank_lines = v


    @property
    def strip_whitespace(self) -> bool:
        return self._strip_whitespace

    @strip_whitespace.setter
    @typed()
    def strip_whitespace(self, v: bool):
        self._strip_whitespace = v


    @property
    def quotechar(self):
        return self._quotechar

    @quotechar.setter
    @typed()
    def quotechar(self, v: Optional[str]):
        if v not in {None, "", "'", '"', "`"}:
            raise ValueError("quotechar should be one of [\"'`] or '' or None")
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


    #---------------------------------------------------------------------------

    def read(self):
        try:
            if self._result:
                return self._result
            if self._files:
                res = {}
                for src, filename, fileno, txt in self._files:
                    self._src = src
                    self._file = filename
                    self._fileno = fileno
                    self._txt = txt
                    self._colnames = None
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

    def _progress(self, progress, status):
        """
        Invoked from the C level to inform that the file reading progress has
        reached the specified level (expressed as a number from 0 to 1).

        Parameters
        ----------
        progress: float
            The overall progress in reading the file. This will be the number
            between 0 and 1.

        status: int
            Status indicator: 0 = the job is running; 1 = the job finished
            successfully; 2 = the job finished with an exception; 3 = the job
            was cancelled by the user (via Ctrl+C or some other mechanism).
        """
        line_width = min(80, term.width)
        if status == 1:
            print("\r" + " " * line_width, end="\r", flush=True)
            return
        bs = self._bar_symbols
        s0 = "Reading file: "
        s1 = " %3d%%" % int(100 * progress)
        bar_width = line_width - len(s0) - len(s1) - 2
        n_chars = int(progress * bar_width + 0.001)
        frac_chars = int((progress * bar_width - n_chars) * len(bs))
        out = bs[-1] * n_chars
        out += bs[frac_chars - 1] if frac_chars > 0 else ""
        outlen = len(out)
        if status == 2:
            out += term.red("(error)")
            outlen += 7
        elif status == 3:
            out += term.dim_yellow("(cancelled)")
            outlen += 11
        out += " " * (bar_width - outlen)
        endf, endl = self._bar_ends
        out = "\r" + s0 + endf + out + endl + s1
        print(_log_color(out), end=("\n" if status else ""), flush=True)


    def _get_destination(self, estimated_size):
        """
        Invoked from the C level, this function will return either the name of
        the folder where the datatable is to be saved; or None, indicating that
        the datatable should be read into RAM. This function may also raise an
        exception if it determines that it cannot find a good strategy to
        handle a dataset of the requested size.
        """
        global _psutil_load_attempted
        if not _psutil_load_attempted:
            _psutil_load_attempted = True
            try:
                import psutil
            except ImportError:
                psutil = None

        if self.verbose and estimated_size > 1:
            self.logger.debug("The Frame is estimated to require %s bytes"
                              % humanize_bytes(estimated_size))
        if estimated_size < 1024 or psutil is None:
            return None
        vm = psutil.virtual_memory()
        if self.verbose:
            self.logger.debug("Memory available = %s (out of %s)"
                              % (humanize_bytes(vm.available),
                                 humanize_bytes(vm.total)))
        if (estimated_size < vm.available and self._save_to is None or
                self._save_to == "memory"):
            if self.verbose:
                self.logger.debug("Frame will be loaded into memory")
            return None
        else:
            if self._save_to:
                tmpdir = self._save_to
                os.makedirs(tmpdir)
            else:
                tmpdir = tempfile.mkdtemp()
            du = psutil.disk_usage(tmpdir)
            if self.verbose:
                self.logger.debug("Free disk space on drive %s = %s"
                                  % (os.path.splitdrive(tmpdir)[0] or "/",
                                     humanize_bytes(du.free)))
            if du.free > estimated_size or self._save_to:
                if self.verbose:
                    self.logger.debug("Frame will be stored in %s"
                                      % tmpdir)
                return tmpdir
        raise RuntimeError("The Frame is estimated to require at lest %s "
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


    def _clear_temporary_files(self):
        for f in self._tempfiles:
            try:
                if self._verbose:
                    self.logger.debug("Removing temporary file %s" % f)
                os.remove(f)
            except OSError as e:
                self.logger.warning("Failed to remove a temporary file: %r" % e)
        if self._tempdir_own:
            shutil.rmtree(self._tempdir, ignore_errors=True)



    #---------------------------------------------------------------------------
    # Process `columns` argument
    #---------------------------------------------------------------------------

    def _set_column_names(self, colnames):
        """
        Invoked by `gread` from C++ to inform the class about the detected
        column names. This method is a simplified version of
        `_override_columns`, and will only be invoked if `self._columns` is
        None.
        """
        self._colnames = colnames


    def _override_columns0(self, coldescs):
        return self._override_columns1(self._columns, coldescs)


    def _override_columns1(self, colspec, coldescs):
        if isinstance(colspec, (slice, range)):
            return self._apply_columns_slice(colspec, coldescs)

        if isinstance(colspec, set):
            return self._apply_columns_set(colspec, coldescs)

        if isinstance(colspec, (list, tuple)):
            return self._apply_columns_list(colspec, coldescs)

        if isinstance(colspec, dict):
            return self._apply_columns_dict(colspec, coldescs)

        if isinstance(colspec, (type, stype, ltype)):
            newcs = {colspec: slice(None)}
            return self._apply_columns_dict(newcs, coldescs)

        if callable(colspec):
            return self._apply_columns_function(colspec, coldescs)

        print(colspec, coldescs)
        raise RuntimeError("Unknown colspec: %r"  # pragma: no cover
                           % colspec)


    def _apply_columns_slice(self, colslice, colsdesc):
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
        self._colnames = colnames
        return coltypes


    def _apply_columns_set(self, colset, colsdesc):
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
            self.logger.warning("Column(s) %r not found in the input file"
                                % list(requested_cols))
        self._colnames = colnames
        return coltypes


    def _apply_columns_list(self, collist, colsdesc):
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
        self._colnames = colnames
        return coltypes


    def _apply_columns_dict(self, colsdict, colsdesc):
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
        for i in range(len(colsdesc)):
            name = colsdesc[i].name
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
        self._colnames = colnames
        return coltypes


    def _apply_columns_function(self, colsfn, colsdesc):
        res = colsfn(colsdesc)
        return self._override_columns1(res, colsdesc)




#-------------------------------------------------------------------------------
# Helper classes
#-------------------------------------------------------------------------------

class FreadWarning(DatatableWarning):
    pass


class _DefaultLogger:
    def debug(self, message):
        if message[0] != "[":
            message = "  " + message
        print(_log_color(message), flush=True)

    def warning(self, message):
        warnings.warn(message, category=FreadWarning)


# os.PathLike interface was added in Python 3.6
_pathlike = (str, bytes, os.PathLike) if hasattr(os, "PathLike") else \
            (str, bytes)

options.register_option(
    "fread.anonymize", xtype=bool, default=False, core=True)

core.register_function(8, fread)



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
