#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
from contextlib import contextmanager
from datatable.lib import core
from datatable.utils.terminal import term
from datatable.utils.typechecks import TTypeError, TValueError, name_type

__all__ = ("options", "Option")


class DtAttributeError(AttributeError):
    _handle_ = TValueError._handle_



#-------------------------------------------------------------------------------
# Config
#-------------------------------------------------------------------------------

class Config:
    """
    Repository of datatable configuration options.
    """
    # All options are stored in the dictionary ``self._options``,
    # where the keys are full option names, and values are the objects
    # of type ``DtOption`` or similar.
    #
    # In addition to each actual option, the ``_options`` dictionary
    # stores nested ``Config`` objects corresponding to every prefix
    # of all other options. For example, if the user declares option
    # ``foo.bar.baz``, then ``Config``s will be created for ``foo.``
    # and ``foo.bar.``. All these nested config objects share the same
    # dictionary ``_options`` as their root.
    #
    __slots__ = ["_options", "_prefix"]

    def __init__(self, options, prefix):
        # Use object.__setattr__ instead of our own __setattr__ below; note:
        # __setattr__ is called for all attributes, regardless of whether they
        # exist in __slots__ or not.
        object.__setattr__(self, "_options", options)
        object.__setattr__(self, "_prefix", prefix)

    def __repr__(self):
        options = sorted(self.__iter__(), key=lambda x: x.name)
        s = "datatable.options.\n"
        s += _render_options_list(options, self._prefix, "    ")
        return s

    def __getattr__(self, key):
        opt = self._get_option(key)
        if isinstance(opt, Config):
            return opt
        return opt.get()

    def __setattr__(self, key, val):
        opt = self._get_option(key)
        if isinstance(opt, Config):
            raise TypeError("Cannot assign a value to group of options `%s.*`"
                            % (self._prefix + key))
        opt.set(val)

    def __delattr__(self, key):
        opt = self._get_option(key)
        opt.set(opt.default)

    def __iter__(self):
        for key, opt in self._options.items():
            if key.startswith(self._prefix):
                yield opt

    def __dir__(self):
        n = len(self._prefix)
        return [key[n:] for key, opt in self._options.items()
                        if key.startswith(self._prefix) and "." not in key[n:]]

    def _get_option(self, key):
        kkey = self._prefix + key
        if kkey in self._options:
            return self._options[kkey]
        msg = "Unknown datatable option `%s`" % kkey
        alternatives = core.fuzzy_match(self._options, kkey)
        if alternatives:
            msg += "; did you mean %s?" % alternatives
        raise DtAttributeError(msg)

    def register(self, opt):
        fullname = opt.name
        if fullname.startswith("."):
            raise TValueError("Invalid option name `%s`" % fullname)
        if fullname in self._options:
            raise TValueError("Option `%s` already registered" % fullname)
        self._options[fullname] = opt
        prefix = fullname.rsplit('.', 1)[0]
        if prefix not in self._options:
            self.register(Config(options=self._options, prefix=prefix + "."))

    def register_option(self, name, default, xtype=None, doc=None,
                        onchange=None):
        opt = Option(name=name, default=default, doc=doc, xtype=xtype,
                     onchange=onchange)
        self.register(opt)

    @property
    def name(self):
        return self._prefix[:-1]

    def get(self, name):
        opt = self._get_option(name)
        return opt.get()

    def set(self, name, value):
        opt = self._get_option(name)
        opt.set(value)

    def reset(self, name=None):
        if name is None:
            # reset all options
            for opt in self.__iter__():
                if not isinstance(opt, Config):
                    opt.set(opt.default)
        else:
            opt = self._get_option(name)
            opt.set(opt.default)

    @contextmanager
    def context(self, **kwargs):
        previous_settings = []
        try:
            for key, value in kwargs.items():
                opt = self._get_option(key)
                original_value = opt.get()
                opt.set(value)
                previous_settings.append((opt, original_value))

            yield

        finally:
            for opt, original_value in previous_settings:
                opt.set(original_value)

    def describe(self, name=None):
        if name is None:  # describe all options
            names = sorted(dir(self))
        else:
            opt = self._get_option(name)
            if isinstance(opt, Config):
                return opt.describe()
            names = [name]
        extras = ""
        for name in names:
            opt = self._get_option(name)
            if isinstance(opt, Config):
                extras += "%s [...]\n\n" % term.color("bold", opt.name)
                continue
            value = opt.get()
            comment = ("(default)" if value == opt.default else
                       "(default: %r)" % opt.default)
            print("%s = %s %s" %
                  (term.color("bold", opt.name),
                   term.color("bright_green", repr(value)),
                   term.color("bright_black", comment)))
            for line in opt.doc.strip().split("\n"):
                print("    " + line)
            print()
        if extras:
            print(extras, end="")






def _render_options_list(options, prefix, indent):
    simple_options = []
    nested_options = []
    skip_prefix = None
    for opt in options:
        if skip_prefix:
            if opt.name.startswith(skip_prefix):
                nested_options[-1][1].append(opt)
                continue
            else:
                skip_prefix = None
        if isinstance(opt, Config):
            nested_options.append((opt, []))
            skip_prefix = opt._prefix
        else:
            simple_options.append(opt)

    n = len(prefix)
    out = ""

    for opt in simple_options:
        out += "%s%s = %r\n" % (indent, opt.name[n:], opt.get())
    for optset, opts in nested_options:
        prefix = optset._prefix
        out += "%s%s\n" % (indent, prefix[n:])
        out += _render_options_list(opts, prefix, indent + "    ")
    return out




#-------------------------------------------------------------------------------
# Option
#-------------------------------------------------------------------------------

class Option:
    def __init__(self, name, default, doc=None, xtype=None, onchange=None):
        self._name = name
        self._default = default
        self._doc = doc
        self._value = default
        self._xtype = xtype
        self._onchange = onchange
        if xtype and not isinstance(default, xtype):
            raise TTypeError("Default value `%r` is not of type %s"
                             % (default, name_type(xtype)))

    @property
    def name(self):
        return self._name

    @property
    def default(self):
        return self._default

    @property
    def doc(self):
        return self._doc

    def get(self):
        return self._value

    def set(self, x):
        if self._xtype is not None:
            if not isinstance(x, self._xtype):
                raise TTypeError("Invalid value for option `%s`: expected %s, "
                                 "instead got %s"
                                 % (self._name, name_type(self._xtype),
                                    name_type(type(x))))
        self._value = x
        if self._onchange is not None:
            self._onchange(x)




#-------------------------------------------------------------------------------
# Global options store
#-------------------------------------------------------------------------------

options = Config(options={}, prefix="")
core.initialize_options(options)

options.register_option("core_logger", default=None, doc="[DEPRECATED]")
