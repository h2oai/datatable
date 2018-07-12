#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import time

import datatable
from datatable.options import options
from datatable.types import ltype
from datatable.utils.misc import plural_form, clamp
from datatable.utils.terminal import term, register_onresize



class DataFrameWidget(object):
    """
    Widget for displaying frame's data interactively.

    This widget will show the data in form of the table, and then (if
    necessary) enter into the "interactive" mode, responding to user's input in
    order to move the viewport.

    Along the Y dimension, we will display at most `VIEW_NROWS_MAX` rows. Less
    if the terminal doesn't fit that many. However if the terminal cannot fit
    at least `VIEW_NROWS_MIN` rows then the interactive mode will be disabled.
    The position of the viewport along Y will be restricted so that the fixed
    amount of rows is always displayed (for example if frame_nrows=100 and
    view_nrows=30, then view_row0 cannot exceed 70).

    Along dimension X, everything is slightly more complicated since all
    columns may have different width.
    """

    VIEW_NROWS_MIN = 10
    VIEW_NROWS_MAX = 30
    RIGHT_MARGIN = 2

    def __init__(self, nrows, ncols, viewdata_callback, interactive=True):
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
        self._view_nrows = DataFrameWidget.VIEW_NROWS_MAX

        # Largest allowed values for ``self._view_(col|row)0``
        self._max_row0 = 0
        self._max_col0 = max(0, ncols - 1)
        self._adjust_viewport()

        # Display state
        if term.jupyter:
            interactive = False
        self._n_displayed_lines = 0
        self._show_types = 0
        self._show_navbar = options.display.interactive_hint and interactive
        self._interactive = interactive
        self._colwidths = {}
        self._term_width = term.width
        self._term_height = term.height
        self._jump_string = None


    def render(self):
        self.draw()
        if not self._interactive:
            return
        old_handler = register_onresize(self._onresize)
        try:
            for ch in datatable.utils.terminal.wait_for_keypresses(0.5):
                if not ch:
                    # Signal handler could have invalidated interactive mode
                    # of the widget -- in which case we need to stop rendering
                    if not self._interactive:
                        break
                    else:
                        continue
                uch = ch.name if ch.is_sequence else ch.upper()
                if self._jump_string is None:
                    if uch == "Q" or uch == "KEY_ESCAPE": break
                    if uch in DataFrameWidget._MOVES:
                        DataFrameWidget._MOVES[uch](self)
                else:
                    if uch in {"Q", "KEY_ESCAPE", "KEY_ENTER"}:
                        self._jump_string = None
                        self.draw()
                    elif uch == "KEY_DELETE" or uch == "KEY_BACKSPACE":
                        self._jump_to(self._jump_string[:-1])
                    elif uch in "0123456789:":
                        self._jump_to(self._jump_string + uch)
        except KeyboardInterrupt:
            pass
        register_onresize(old_handler)
        print(term.move_x(0) + term.clear_eol)


    def draw(self):
        data = self._fetch_data()
        colnames = data["names"]
        coltypes = data["types"]
        stypes = data["stypes"]
        coldata = data["columns"]
        indices = data["indices"]

        # Create column with row indices
        oldwidth = self._colwidths.get("index", 3)
        indexcolumn = _Column(name="", ctype=ltype.str, data=indices)
        indexcolumn.color = term.bright_black
        indexcolumn.margin = "  "
        indexcolumn.width = max(oldwidth, indexcolumn.width)
        self._colwidths["index"] = indexcolumn.width

        # Data columns
        columns = [indexcolumn]
        for i in range(self._view_ncols):
            if self._show_types == 1:
                name = term.green(coltypes[i].name)
            elif self._show_types == 2:
                name = term.cyan(stypes[i].name)
            else:
                name = colnames[i]
            oldwidth = self._colwidths.get(i + self._view_col0, 0)
            col = _Column(name=name, ctype=coltypes[i], data=coldata[i])
            col.width = max(col.width, oldwidth)
            if self._view_ncols < self._frame_ncols:
                col.width = min(col.width, _Column.MAX_WIDTH)
            self._colwidths[i + self._view_col0] = col.width
            columns.append(col)
        columns[-1].margin = ""

        # Adjust widths of columns
        total_width = sum(col.width + len(col.margin) for col in columns)
        extra_space = term.width - total_width - DataFrameWidget.RIGHT_MARGIN
        if extra_space > 0:
            if self._view_col0 + self._view_ncols < self._frame_ncols:
                self._view_ncols += max(1, extra_space // 8)
                return self.draw()
            elif self._view_col0 > 0:
                w = self._fetch_column_width(self._view_col0 - 1)
                if w + 2 <= extra_space:
                    self._view_col0 -= 1
                    self._view_ncols += 1
                    self._max_col0 = self._view_col0
                    return self.draw()
        else:
            if self._max_col0 == self._view_col0:
                self._max_col0 += 1
            available_width = term.width - DataFrameWidget.RIGHT_MARGIN
            for i, col in enumerate(columns):
                col.width = min(col.width, available_width)
                available_width -= col.width + len(col.margin)
                if available_width <= 0:
                    available_width = 0
                    col.margin = ""
                else:
                    self._view_ncols = i + 1

        # Generate the elements of the display
        grey = term.bright_black
        header = ["".join(col.header for col in columns),
                  grey("".join(col.divider for col in columns))]
        rows = ["".join(col.value(j) for col in columns)
                for j in range(self._view_nrows)]
        srows = plural_form(self._frame_nrows, "row")
        scols = plural_form(self._frame_ncols, "column")
        footer = ["", "[%s x %s]" % (srows, scols), ""]

        # Display hint about navigation keys
        if self._show_navbar:
            remaining_width = term.width
            if self._jump_string is None:
                nav_elements = [grey("Press") + " q " + grey("to quit"),
                                "  ↑←↓→ " + grey("to move"),
                                "  wasd " + grey("to page"),
                                "  t " + grey("to toggle types"),
                                "  g " + grey("to jump")]
                for elem in nav_elements:
                    l = term.length(elem)
                    if l > remaining_width:
                        break
                    remaining_width -= l
                    footer[2] += elem
            else:
                footer[2] = grey("Go to (row:col): ") + self._jump_string

        # Render the table
        lines = header + rows + footer
        out = (term.move_x(0) + term.move_up * self._n_displayed_lines +
               (term.clear_eol + "\n").join(lines) + term.clear_eol)
        print(out, end="")
        self._n_displayed_lines = len(lines) - 1


    #---------------------------------------------------------------------------
    # Private
    #---------------------------------------------------------------------------

    _MOVES = {
        "KEY_LEFT": lambda self: self._move_viewport(dx=-1),
        "KEY_RIGHT": lambda self: self._move_viewport(dx=1),
        "KEY_UP": lambda self: self._move_viewport(dy=-1),
        "KEY_DOWN": lambda self: self._move_viewport(dy=1),
        "KEY_SLEFT": lambda self: self._move_viewport(x=0),
        "KEY_SRIGHT": lambda self: self._move_viewport(x=self._max_col0),
        "KEY_SUP": lambda self: self._move_viewport(y=0),
        "KEY_SDOWN": lambda self: self._move_viewport(y=self._max_row0),
        "W": lambda self: self._move_viewport(dy=-self._view_nrows),
        "S": lambda self: self._move_viewport(dy=self._view_nrows),
        "A": lambda self: self._move_viewport(dx=-self._view_ncols),
        "D": lambda self: self._move_viewport(dx=max(1, self._view_ncols - 1)),
        "T": lambda self: self._toggle_types(),
        "G": lambda self: self._toggle_jump_mode(),
    }

    def _fetch_data(self):
        """
        Retrieve frame data within the current view window.

        This method will adjust the view window if it goes out-of-bounds.
        """
        self._view_col0 = clamp(self._view_col0, 0, self._max_col0)
        self._view_row0 = clamp(self._view_row0, 0, self._max_row0)
        self._view_ncols = clamp(self._view_ncols, 0,
                                 self._frame_ncols - self._view_col0)
        self._view_nrows = clamp(self._view_nrows, 0,
                                 self._frame_nrows - self._view_row0)
        return self._data_callback(self._view_row0,
                                   self._view_row0 + self._view_nrows,
                                   self._view_col0,
                                   self._view_col0 + self._view_ncols)

    def _fetch_column_width(self, icol):
        if icol in self._colwidths:
            return self._colwidths[icol]
        else:
            data = self._data_callback(self._view_row0,
                                       self._view_row0 + self._view_nrows,
                                       icol,
                                       icol + 1)
            col = _Column(name=data["names"][0], ctype=data["types"][0],
                          data=data["columns"][0])
            w = min(col.width, _Column.MAX_WIDTH)
            self._colwidths[icol] = w
            return w


    def _move_viewport(self, dx=0, dy=0, x=None, y=None, force_draw=False):
        if x is None:
            x = self._view_col0 + dx
        if y is None:
            y = self._view_row0 + dy
        ncol0 = clamp(x, 0, self._max_col0)
        nrow0 = clamp(y, 0, self._max_row0)
        if ncol0 != self._view_col0 or nrow0 != self._view_row0 or force_draw:
            self._view_col0 = ncol0
            self._view_row0 = nrow0
            self.draw()


    def _toggle_types(self):
        self._show_types = (self._show_types + 1) % 3
        self.draw()


    def _toggle_jump_mode(self):
        assert self._jump_string is None
        self._jump_string = ""
        self.draw()


    def _jump_to(self, newloc):
        parts = newloc.split(":")
        if parts[0]:
            newy = int(parts[0]) - self._view_nrows // 2
        else:
            newy = None
        if len(parts) >= 2 and parts[1]:
            newx = int(parts[1])
        else:
            newx = None
        self._jump_string = ":".join(parts[:2])
        self._move_viewport(x=newx, y=newy, force_draw=True)


    def _adjust_viewport(self):
        # Adjust Y position
        new_nrows = min(DataFrameWidget.VIEW_NROWS_MAX,
                        max(term.height - 5, DataFrameWidget.VIEW_NROWS_MIN),
                        self._frame_nrows)
        self._view_nrows = new_nrows
        self._max_row0 = self._frame_nrows - new_nrows
        self._view_row0 = min(self._view_row0, self._max_row0)


    def _onresize(self, signum, stkfrm):
        if term.width < self._term_width:
            self._interactive = False
        else:
            self._adjust_viewport()
            self._term_width = term.width
            self._term_height = term.height
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
        ltype.int: str,
        ltype.str: lambda x: x,
        ltype.real: lambda x: "%.6g" % x,
        ltype.time: lambda x: time.strftime("%Y-%m-%d %H:%M:%S",
                                            time.gmtime(x / 1000)),
        ltype.bool: lambda x: "1" if x else "0",
        ltype.obj: repr,
    }

    def __init__(self, name, ctype, data, color=None):
        self._name = name
        self._type = ctype
        self._color = color
        self._alignleft = ctype in (ltype.str, ltype.time)
        # May be empty if the column has 0 rows.
        self._strdata = list(self._stringify_data(data))
        self._rmargin = "  "
        if ctype == ltype.real:
            self._align_at_dot()
        if self._strdata:
            self._width = max(term.length(name), 2,
                              max(len(field) for field in self._strdata))
        else:
            self._width = max(term.length(name), 2)


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
        if not self._strdata:
            return
        tails = [_float_tail(x) for x in self._strdata]
        maxtail = max(tails)
        self._strdata = [v if v == "nan" else v + " " * (maxtail - tails[j])
                         for j, v in enumerate(self._strdata)]


    def _format(self, value):
        diff = self._width - term.length(value)
        if diff >= 0:
            whitespace = " " * diff
            if self._alignleft:
                return value + whitespace
            else:
                return whitespace + value
        else:
            return value[:self._width - 1] + "…"



def _float_tail(x):
    idot = x.rfind(".")
    if idot >= 0:
        return len(x) - idot - 1
    else:
        return -1


options.register_option(
    "display.interactive_hint", xtype=bool, default=True,
    doc="Display navigation hint at the bottom of a Frame when viewing "
        "its contents in the console.")
