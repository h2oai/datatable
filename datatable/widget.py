#!/usr/bin/env python
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
from __future__ import division, print_function, unicode_literals
import time

from datatable.utils.misc import plural_form, clamp
from datatable.utils.terminal import term, wait_for_keypresses

grey = term.bright_black



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
        self._data_callback = viewdata_callback

        # Coordinates of the data window within the frame
        self._view_col0 = 0
        self._view_row0 = 0
        self._view_ncols = 30
        self._view_nrows = 20

        #
        self._max_row0 = max(0, nrows - self._view_nrows)

        self._n_displayed_lines = 0


    def render(self, interactive=True):
        self.draw(show_nav=interactive)
        if not interactive:
            return
        try:
            for ch in wait_for_keypresses(0.5):
                if not ch: continue
                if ch == "q" or ch == "Q": return
                uch = ch.name if ch.is_sequence else ch.upper()
                if uch in DataFrameWidget._MOVES:
                    DataFrameWidget._MOVES[uch](self)
                # print(uch)
        except KeyboardInterrupt:
            pass


    def draw(self, show_nav=True):
        data = self._fetch_data()
        colnames = data["names"]
        coltypes = data["types"]
        coldata = data["columns"]

        indices = range(self._view_row0, self._view_row0 + self._view_nrows)
        indexcolumn = _Column(name="", ctype="int", data=indices)
        indexcolumn.color = grey
        indexcolumn.margin = "  "

        columns = [indexcolumn]
        for i in range(len(colnames)):
            col = _Column(name=colnames[i], ctype=coltypes[i], data=coldata[i])
            columns.append(col)
        columns[-1].margin = ""

        # Adjust widths of columns
        total_width = sum(col.width + len(col.margin) for col in columns)
        remaining_width = term.width
        if total_width > remaining_width:
            for i, col in enumerate(columns):
                col.width = min(col.width, remaining_width, _Column.MAX_WIDTH)
                remaining_width -= col.width + 2
                if remaining_width < 0:
                    remaining_width = 0
                if remaining_width < 2:
                    col.margin = ""

        # Generate the elements of the display
        header = ["".join(col.header for col in columns),
                  grey("".join(col.divider for col in columns))]
        rows = ["".join(col.value(j) for col in columns)
                for j in range(self._view_nrows)]
        srows = plural_form(self._frame_nrows, "row")
        scols = plural_form(self._frame_ncols, "column")
        footer = ["", "[%s x %s]" % (srows, scols), ""]

        # Display hint about navigation keys
        if show_nav:
            remaining_width = term.width
            nav_elements = [grey("Press") + " q " + grey("to quit"),
                            "  ↑←↓→ " + grey("to move"),
                            "  wasd " + grey("to page")]
            for elem in nav_elements:
                l = term.length(elem)
                if l > remaining_width:
                    break
                remaining_width -= l
                footer[2] += elem

        # Render the table
        lines = header + rows + footer
        out = (term.move_up * self._n_displayed_lines +
               (term.clear_eol + "\n").join(lines))
        print(out)
        self._n_displayed_lines = len(lines)


    #---------------------------------------------------------------------------
    # Private
    #---------------------------------------------------------------------------

    _MOVES = {
        "KEY_LEFT": lambda self: self._move_viewport(dx=-1),
        "KEY_RIGHT": lambda self: self._move_viewport(dx=1),
        "KEY_UP": lambda self: self._move_viewport(dy=-1),
        "KEY_DOWN": lambda self: self._move_viewport(dy=1),
        "KEY_SLEFT": lambda self: self._move_viewport(dx=-self._view_col0),
        "W": lambda self: self._move_viewport(dy=-self._view_nrows),
        "S": lambda self: self._move_viewport(dy=self._view_nrows),
        "A": lambda self: self._move_viewport(dx=-self._view_ncols),
        "D": lambda self: self._move_viewport(dx=self._view_ncols),
    }

    def _fetch_data(self):
        """
        Retrieve frame data within the current view window.

        This method will adjust the view window if it goes out-of-bounds.
        """
        self._view_col0 = clamp(self._view_col0, 0, self._frame_ncols - 1)
        self._view_row0 = clamp(self._view_row0, 0, self._max_row0)
        self._view_ncols = clamp(self._view_ncols, 0,
                                 self._frame_ncols - self._view_col0)
        self._view_nrows = clamp(self._view_nrows, 0,
                                 self._frame_nrows - self._view_row0)
        return self._data_callback(self._view_col0, self._view_ncols,
                                   self._view_row0, self._view_nrows)


    def _move_viewport(self, dx=0, dy=0):
        newcol0 = clamp(self._view_col0 + dx, 0, self._frame_ncols - 1)
        newrow0 = clamp(self._view_row0 + dy, 0, self._max_row0)
        if newcol0 != self._view_col0 or newrow0 != self._view_row0:
            self._view_col0 = newcol0
            self._view_row0 = newrow0
            self.draw()




#===============================================================================
# Single Column within the dataframe widget
#===============================================================================

class _Column(object):
    """
    This helper class represents a single column within a dataframe, and
    helps with formatting that column.
    """

    # Maximum width for a column (unless all columns fit into screen)
    MAX_WIDTH = 50

    formatters = {
        "int": str,
        "str": lambda x: x,
        "enum": lambda x: x,  # should already be mapped to strings
        "real": lambda x: "%.6g" % x,
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
        self._rmargin = "  "
        if ctype == "real":
            self._align_at_dot()
        self._width = max(len(name),
                          max(len(field) for field in self._strdata))


    #---------------------------------------------------------------------------
    # Basic properties
    #---------------------------------------------------------------------------

    @property
    def width(self):
        """
        The display width of the column (not including the margin).

        Reducing the width of the column will truncate its content.
        """
        return self._width

    @width.setter
    def width(self, value):
        self._width = value
        if value == 0:
            self._rmargin = ""

    @property
    def color(self):
        """ColorFormatter applied to the field."""
        return self._color

    @color.setter
    def color(self, value):
        self._color = value

    @property
    def margin(self):
        """Column's margin on the right-hand side."""
        return self._rmargin

    @margin.setter
    def margin(self, value):
        self._rmargin = value


    @property
    def header(self):
        if self._width == 0:
            return ""
        else:
            return term.bright_white(self._format(self._name)) + self._rmargin

    @property
    def divider(self):
        if self._width == 0:
            return ""
        else:
            return "-" * self._width + self._rmargin

    def value(self, row):
        if self._width == 0:
            return ""
        v = self._format(self._strdata[row])
        if self._color:
            return self._color(v) + self._rmargin
        else:
            return v + self._rmargin


    #---------------------------------------------------------------------------
    # Private
    #---------------------------------------------------------------------------

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
