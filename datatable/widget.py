#!/usr/bin/env python
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
from __future__ import division, print_function, unicode_literals
import time

from datatable.utils.misc import plural_form
from datatable.utils.terminal import term, wait_for_keypresses


# Maximum width for a column (unless all columns fit into screen)
MAX_COL_WIDTH = 50


class DataFrameWidget(object):

    def __init__(self, nrows, ncols, viewdata_callback):
        """
        Initialize a new frame widget.

        :param nrows: number of rows in the frame being displayed.
        :param ncols: number of columns in the frame being displayed.
        :param viewdata_callback: function that can be called to retrieve
            the frame's data. This function should take 4 arguments:
            ``(col0, ncols, row0, nrows)`` and return a dictionary with the
            following fields: "names", "types", "columns". The "names" field
            is the list of column names, "types" are their types, and lastly
            "columns" is the frame's data within the requested view, stored
            as a list of columns.
        """
        # number of rows / columns in the dataframe being displayed
        self._frame_nrows = nrows
        self._frame_ncols = ncols
        # callback function to fetch dataframe's data
        self._fetch_data = viewdata_callback

        # Coordinates of the data window within the frame
        self._view_col0 = 0
        self._view_row0 = 0
        self._view_ncols = min(30, ncols)
        self._view_nrows = min(20, nrows)

        self._displayed_nrows = 0


    def render(self, interactive=True):
        self.draw()
        if not interactive:
            return
        for ch in wait_for_keypresses(0.5):
            if not ch: continue
            if ch == "q" or ch == "Q": return
            if ch.is_sequence:
                if ch.name == "KEY_LEFT":
                    self._view_col0 = max(0, self._view_col0 - 1)
                if ch.name == "KEY_RIGHT":
                    self._view_col0 = min(self._frame_ncols - 1, self._view_col0 + 1)
                if ch.name == "KEY_UP":
                    self._view_row0 = max(0, self._view_row0 - 1)
                if ch.name == "KEY_DOWN":
                    self._view_row0 = min(max(0, self._frame_nrows - 20), self._view_row0 + 1)
                print(term.move_up * (self._displayed_nrows + 1))
                self.draw()


    def draw(self):
        data = self._fetch_data(self._view_col0, self._view_ncols,
                                self._view_row0, self._view_nrows)
        colnames = data["names"]
        coltypes = data["types"]
        coldata = data["columns"]

        indices = range(self._view_row0, self._view_row0 + self._view_nrows)
        indexcol = _Column(name="", ctype="int", data=indices)
        indexcol.color = term.bright_black
        columns = [indexcol]
        for i in range(len(colnames)):
            col = _Column(name=colnames[i], ctype=coltypes[i], data=coldata[i])
            columns.append(col)

        # Adjust widths of columns
        widths = [col.width for col in columns]
        total_width = sum(widths) + 2 * (len(widths) - 1)
        remaining_width = term.width
        if total_width > remaining_width:
            for i, col in enumerate(columns):
                col.width = min(col.width, remaining_width, MAX_COL_WIDTH)
                remaining_width -= col.width + 2
                if remaining_width < 0:
                    remaining_width = 0

        width = term.width
        headers = "  ".join(col.header for col in columns)[:width]
        divider = "  ".join(col.divider for col in columns)[:width]
        print(headers)
        print(term.bright_black(divider))
        for j in range(self._view_nrows):
            row = ""
            for i, col in enumerate(columns):
                if col.width == 0:
                    break
                row += ("  " if i else "") + col.value(j)
            print(row)
        srows = plural_form(self._frame_nrows, "row")
        scols = plural_form(self._frame_ncols, "column")
        print("\n[%s x %s]    " % (srows, scols) +
              term.bright_black("(press q to quit)"))
        self._displayed_nrows = self._view_nrows + 4  # 2 header + 2 footer



#-------------------------------------------------------------------------------
# Single Column within the dataframe widget
#-------------------------------------------------------------------------------

class _Column(object):
    """
    This helper class represents a single column within a dataframe, and
    helps with formatting that column.
    """

    formatters = {
        "int": str,
        "str": lambda x: x,
        "enum": lambda x: x,  # should already be mapped to strings
        "real": lambda x: "%.6f" % x,
        "time": lambda x: time.strftime("%Y-%m-%d %H:%M:%S",
                                        time.gmtime(x / 1000)),
        "bool": lambda x: "1" if x else "0",
        "obj": repr,
    }

    def __init__(self, name, ctype, data, color=None):
        self._name = name
        self._type = ctype
        self._color = color
        self._alignleft = ctype in {"str", "enum", "time"}
        self._strdata = list(self._stringify_data(data))
        if ctype == "real":
            self._align_at_dot()
        self._width = max(len(name),
                          max(len(field) for field in self._strdata))

    @property
    def width(self):
        """Return the desired length of the field"""
        return self._width

    @width.setter
    def width(self, value):
        self._width = value

    @property
    def color(self):
        return self._color

    @color.setter
    def color(self, value):
        self._color = value


    @property
    def header(self):
        return term.bright_white(self._format(self._name))

    @property
    def divider(self):
        return "-" * self._width

    def value(self, row):
        v = self._format(self._strdata[row])
        if self._color:
            return self._color(v)
        else:
            return v


    def _stringify_data(self, values):
        formatter = _Column.formatters[self._type]
        for value in values:
            if value is None:
                yield ""
            else:
                yield formatter(value)


    def _align_at_dot(self):
        tails = [len(x) - x.rfind(".") - 1 for x in self._strdata]
        maxtail = max(tails)
        self._strdata = [v + " " * (maxtail - tails[j])
                         for j, v in enumerate(self._strdata)]


    def _format(self, value):
        diff = self._width - len(value)
        if diff >= 0:
            whitespace = " " * diff
            if self._alignleft:
                return value + whitespace
            else:
                return whitespace + value
        else:
            return value[:self._width - 1] + "…"
