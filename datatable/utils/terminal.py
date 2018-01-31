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
import blessed
import _locale

__all__ = ("term", "wait_for_keypresses", "register_onresize")


# Initialize the terminal
class MyTerminal(blessed.Terminal):
    def __init__(self):
        super().__init__()
        self.override_width = 0

    @property
    def width(self):
        return self.override_width or blessed.Terminal.width.fget(self)

    def disable_styling(self):
        # The `does_styling` attr is read-only, so in order to actually change
        # it we do a small hack: create a new Terminal object which has styling
        # disabled, and then replace the "guts" of the current object with those
        # of the newly created object.
        tt = blessed.Terminal(force_styling=None)
        tt.override_width = self.override_width
        self.__dict__, tt.__dict__ = tt.__dict__, self.__dict__


# Save current locale settings
try:
    _lls = []
    for i in range(100):
        ll = _locale.setlocale(i)
        _lls.append(ll)
except _locale.Error:
    pass

# Instantiate a terminal
term = MyTerminal()

# Restore previous locale settings
for i, ll in enumerate(_lls):
    _locale.setlocale(i, ll)



# Override sys.displayhook to allow better control over what is being printed
# in the console. In particular, if an object to be displayed has method
# ``_display_in_terminal_``, then that method will be used for custom object
# rendering. Otherwise the default handler is used, which simply prints
# ``repr(obj)`` if obj is not None.
#
def _new_displayhook(value):
    if value is None:
        return
    if hasattr(value, "_display_in_terminal_") and type(value) is not type:
        value._display_in_terminal_()
    else:
        _original_displayhook(value)


_original_displayhook = sys.displayhook
sys.displayhook = _new_displayhook



def wait_for_keypresses(refresh_rate=1):
    """
    Listen to user's keystrokes and return them to caller one at a time.

    The produced values are instances of blessed.keyboard.Keystroke class.
    If the user did not press anything with the last `refresh_rate` seconds
    the generator will yield `None`, allowing the caller to perform any
    updates necessary.

    This generator is infinite, and thus needs to be stopped explicitly.
    """
    with term.cbreak():
        while True:
            yield term.inkey(timeout=refresh_rate)



def register_onresize(callback):
    import signal
    return signal.signal(signal.SIGWINCH, callback)
