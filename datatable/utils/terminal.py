#!/usr/bin/env python
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
"""
Various functions to help display something in a terminal.
"""
from __future__ import division, print_function, unicode_literals
import sys
import blessed

__all__ = ("term", "wait_for_keypresses", "register_onresize")


# Initialize the terminal
term = blessed.Terminal()


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
