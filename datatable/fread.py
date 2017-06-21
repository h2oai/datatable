#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import os
import tempfile

# noinspection PyUnresolvedReferences
import _datatable as c
from datatable.dt import DataTable
from datatable.utils.typechecks import typed, U
from datatable.utils.terminal import term


_log_color = term.bright_black


def fread(filename="", **params):
    freader = FReader(filename=filename, **params)
    return freader.read()



class FReader(object):

    @typed(filename=str, text=str, sep=str, max_nrows=int, header=bool,
           na_strings=[str], fill=bool, show_progress=bool, encoding=str)
    def __init__(self, filename=None, text=None, sep=None, max_nrows=None,
                 header=None, na_strings=None, verbose=False, fill=False,
                 show_progress=term.is_a_tty, encoding=None, **args):
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

        self._log_newline = True
        self._colnames = None
        self._bar_ends = None
        self._bar_symbols = None

        self.verbose = verbose
        self.text = text
        self.filename = filename
        self.sep = sep
        self.max_nrows = max_nrows
        self.header = header
        self.na_strings = na_strings
        self.fill = fill
        self.show_progress = show_progress

        if "separator" in args:
            self.sep = args.pop("separator")
        if "progress_fn" in args:
            progress = args.pop("progress_fn")
            if not callable(progress):
                raise TypeError("`progress_fn` argument should be a function")
            self._progress = progress
        if args:
            raise TypeError("Unknown argument(s) %r in FReader(...)"
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
        if not text:
            self._text = None
        else:
            if "\x00" in text:
                raise ValueError("Text contains NUL characters")
            self._text = text


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
                raise ValueError("Multi-character separator %r not supported"
                                 % sep)
            if ord(sep) > 127:
                raise ValueError("The separator should be an ASCII character, "
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
    @typed(na_strings=U([str], None))
    def na_strings(self, na_strings):
        if na_strings is None:
            self._nastrings = []
        else:
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



    def read(self):
        _dt = c.fread(self)
        dt = DataTable(_dt, colnames=self._colnames)
        if self._tempfile:
            if self._verbose:
                self._vlog("Removing temporary file %s\n" % self._tempfile)
            os.remove(self._tempfile)
            os.rmdir(self._tempdir)
        return dt


    #---------------------------------------------------------------------------

    def _vlog(self, message):
        if self._log_newline:
            print("  ", end="")
        self._log_newline = message.endswith("\n")
        print(_log_color(message), end="", flush=True)


    def _progress(self, percent):
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
            raise ValueError("Path %s contains NUL characters" % filename)
        if not os.path.exists(filename):
            xpath = os.path.abspath(filename)
            ypath = xpath
            while not os.path.exists(xpath):
                xpath = os.path.abspath(os.path.join(xpath, ".."))
            ypath = ypath[len(xpath):]
            raise ValueError("File %s`%s` does not exist" % (xpath, ypath))
        if not os.path.isfile(filename):
            raise ValueError("Path `%s` is not a file" % filename)

        ext = os.path.splitext(filename)[1]
        if ext == ".zip":
            import zipfile
            zf = zipfile.ZipFile(filename)
            zff = zf.namelist()
            if len(zff) > 1:
                raise ValueError("Zip file %s contains multiple compressed "
                                 "files: %r" % (filename, zff))
            if len(zff) == 0:
                raise ValueError("Zip file %s is empty" % filename)
            self._tempdir = tempfile.mkdtemp()
            if self._verbose:
                self._vlog("Extracting %s to temporary directory %s\n"
                           % (filename, self._tempdir))
            self._tempfile = zf.extract(zff[0], path=self._tempdir)

        elif ext == ".gz":
            import gzip
            zf = gzip.GzipFile(filename)
            if self._verbose:
                self._vlog("Extracting %s into memory\n" % filename)
            self._text = zf.read()
