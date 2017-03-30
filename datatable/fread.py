#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import os

# noinspection PyUnresolvedReferences
import _datatable as c
from datatable.utils.typechecks import typed, U



def fread(filename="", **params):
    pass



class FReader(object):

    @typed(filename=str, text=str, separator=str, max_nrows=int, header=bool,
           na_strings=[str])
    def __init__(self, filename=None, text=None, separator=None, max_nrows=None,
                 header=None, na_strings=None, verbose=False, **args):
        self._filename = None   # type: str
        self._text = None       # type: str
        self._separator = None  # type: str
        self._maxnrows = None   # type: int
        self._header = None     # type: bool
        self._nastrings = []    # type: List[str]
        self._verbose = False   # type: bool
        self.filename = filename
        self.text = text
        self.separator = separator
        self.max_nrows = max_nrows
        self.header = header
        self.na_strings = na_strings
        self.verbose = verbose


    @property
    def filename(self):
        return self._filename

    @filename.setter
    @typed(filename=U(str, None))
    def filename(self, filename):
        if not filename:
            self._filename = None
        else:
            if filename.startswith("~"):
                filename = os.path.expanduser(filename)
            if "\x00" in filename:
                raise ValueError(f"Path {filename} contains NULs")
            self._filename = filename
            self._text = None


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
                raise ValueError(f"Text contains NUL characters")
            self._text = text
            self._filename = None


    @property
    def separator(self):
        return self._separator

    @separator.setter
    @typed(separator=U(str, None))
    def separator(self, separator):
        if not separator:
            self._separator = None
        else:
            if len(separator) > 1:
                raise ValueError(f"Multi-character separator {separator!r} "
                                 "not supported")
            if ord(s) > 127:
                raise ValueError(f"The separator should be an ASCII character, "
                                 f"got {separator!r}")
            self._separator = separator


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


    def read(self):
        return c.fread(self)

    def _vlog(self, message):
        print(message)
