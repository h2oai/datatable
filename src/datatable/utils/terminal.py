#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
"""
Various functions to help display something in a terminal.
"""
import sys
from datatable.lib import core

__all__ = ("term", "register_onresize")


_default_palette = {
    "reset": "\x1B[m",
    "bold": "\x1B[1m",
    "dim": "\x1B[2m",
    "red": "\x1B[31m",
    "green": "\x1B[32m",
    "yellow": "\x1B[33m",
    "blue": "\x1B[34m",
    "magenta": "\x1B[35m",
    "cyan": "\x1B[36m",
    "white": "\x1B[37m",
    "bright_black": "\x1B[90m",
    "bright_red": "\x1B[91m",
    "bright_green": "\x1B[92m",
    "bright_yellow": "\x1B[93m",
    "bright_cyan": "\x1B[96m",
    "bright_white": "\x1B[97m",
}


# Initialize the terminal
class Terminal:

    def __init__(self):
        self.jupyter = False
        self.ipython = False
        if sys.__stdin__ and sys.__stdout__:
            import blessed
            import _locale

            # Save current locale settings
            try:
                _lls = []
                for i in range(100):
                    ll = _locale.setlocale(i)
                    _lls.append(ll)
            except _locale.Error:
                pass

            self._blessed_term = blessed.Terminal()

            # Restore previous locale settings
            for i, ll in enumerate(_lls):
                _locale.setlocale(i, ll)

            self._allow_unicode = False
            self._enable_keyboard = True
            self._enable_colors = True
            self._enable_terminal_codes = True
            self._encoding = self._blessed_term._encoding
            enc = self._encoding.upper()
            if enc == "UTF8" or enc == "UTF-8":
                self._allow_unicode = True
            self.is_a_tty = sys.__stdin__.isatty() and sys.__stdout__.isatty()
            self._width = 0
            self._height = 0
            self._check_ipython()
        else:
            self._enable_keyboard = False
            self._enable_colors = False
            self._enable_terminal_codes = False
            self._encoding = "UTF8"
            self._allow_unicode = True
            self.is_a_tty = False
            self._width = 80
            self._height = 25

    @property
    def width(self):
        return self._width or self._blessed_term.width

    @property
    def height(self):
        return self._height or self._blessed_term.height

    def length(self, x):
        return self._blessed_term.length(x)

    def clear_line(self, end=""):
        if self._enable_terminal_codes:
            print("\x1B[1G\x1B[K", end=end)

    def rewrite_lines(self, lines, nold, end=""):
        if self._enable_terminal_codes and nold:
            print("\x1B[1G\x1B[%dA" % nold +
                  "\x1B[K\n".join(lines) + "\x1B[K", end=end)
        else:
            print("\n".join(lines), end=end)

    def _check_ipython(self):
        # When running inside a Jupyter notebook, IPython and ipykernel will
        # already be preloaded (in sys.modules). We don't want to try to
        # import them, because it adds unnecessary startup delays.
        if "IPython" in sys.modules:
            ipy = sys.modules["IPython"].get_ipython()
            ipy_type = str(type(ipy))
            if "ZMQInteractiveShell" in ipy_type:
                self._encoding = "UTF8"
                self.jupyter = True
            elif "TerminalInteractiveShell" in ipy_type:
                self.ipython = True


    def using_colors(self):
        return self._enable_colors

    def use_colors(self, f):
        self._enable_colors = f

    def use_keyboard(self, f):
        self._enable_keyboard = f

    def use_terminal_codes(self, f):
        self._enable_terminal_codes = f

    def set_allow_unicode(self, v):
        self._allow_unicode = bool(v)

    def color(self, color, text):
        if self._enable_colors:
            return _default_palette[color] + text + "\x1B[m"
        else:
            return text

    def wait_for_keypresses(self, refresh_rate=1):
        """
        Listen to user's keystrokes and return them to caller one at a time.

        The produced values are instances of blessed.keyboard.Keystroke class.
        If the user did not press anything with the last `refresh_rate` seconds
        the generator will yield `None`, allowing the caller to perform any
        updates necessary.

        This generator is infinite, and thus needs to be stopped explicitly.
        """
        if not self._enable_keyboard:
            return
        with self._blessed_term.cbreak():
            while True:
                yield self._blessed_term.inkey(timeout=refresh_rate)



# Instantiate a terminal
term = Terminal()



# Override sys.displayhook to allow datatable Frame to show a rich
# display when viewed.
#
def _new_displayhook(value):
    if isinstance(value, core.Frame):
        value.view(None)
    else:
        _original_displayhook(value)


_original_displayhook = sys.displayhook
if not term.jupyter:
    sys.displayhook = _new_displayhook



def register_onresize(callback):
    import signal
    return signal.signal(signal.SIGWINCH, callback)
