#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
from datatable.lib import core
from datatable.utils.typechecks import (is_type, name_type, TTypeError,
                                        TValueError)

__all__ = ("options", )


class DtAttributeError(AttributeError):
    _handle_ = TTypeError._handle_

class _bool(int):
    def __str__(self):
        if self == 1:
            return "True"
        if self == 0:
            return "False"

class _function:
    def __init__(self, v):
        self._obj = v

    def __call__(self):
        self._obj()

    def __repr__(self):
        return repr(self._obj)

    def __str__(self):
        return str(self._obj)

    def __eq__(self, other):
        return self._obj.__eq__(other)

    def __bool__(self):
        return bool(self._obj)



#-------------------------------------------------------------------------------
# DtOption
#-------------------------------------------------------------------------------

class DtOption:
    """
    This is a 'lead node' class in the DtConfig tree of options. However, the
    objects of this class are not directly returned to the user. Instead, the
    DtConfig class invokes the following API:

        dt.options.some_option      ->  return some_option.get()
        dt.options.some_option = v  ->  some_option.set(v)
        del dt.options.some_option  ->  some_option.reset()

    The reasons why we don't return `some_option` directly to the user are:
      - the return value must be derived from the appropriate base class,
        such as `int`, `float`, `str`, so that the user may use the value
        directly as if it was int / float / string;
      - at the same time the option must be mutable (i.e. it should be
        possible to change its value via assignment or a method), and base
        classes listed above are immutable.

    The value returned by `some_option.get()` is not the base value either: it
    is additionally equipped with custom docstring.
    """
    __slots__ = ["_name", "_default", "_klass"]

    def __init__(self, name, xtype, default, doc=None):
        if xtype == bool:
            xtype = _bool
        if xtype == callable:
            xtype = _function
        self._name = name
        self._default = default
        self._klass = type(xtype.__name__, (xtype,), dict(__doc__=doc))
        self.set(default)

    def get(self):
        v = core.get_option(self._name)
        return self._klass(v)

    def set(self, v):
        core.set_option(self._name, v)

    def reset(self):
        self.set(self._default)



class DtConfig:
    __slots__ = ["_keyvals", "_prefix"]

    def __init__(self, prefix=""):
        if prefix:
            prefix += "."
        # Use object.__setattr__ instead of our own __setattr__ below; note:
        # __setattr__ is called for all attributes, regardless of whether they
        # exist in __slots__ or not.
        object.__setattr__(self, "_keyvals", {})
        object.__setattr__(self, "_prefix", prefix)


    def __getattr__(self, key):
        if key:
            opt = self._get_opt(key)
            if isinstance(opt, DtOption):
                return opt.get()
            else:
                return opt
        else:
            res = {}
            for k, v in self._keyvals.items():
                if isinstance(v, DtOption):
                    res[self._prefix + k] = v.get()
                else:
                    res.update(v.__getattr__(""))
            return res


    def __setattr__(self, key, val):
        opt = self._get_opt(key)
        if isinstance(opt, DtOption):
            opt.set(val)
        else:
            raise DtAttributeError("Cannot modify group of options `%s`"
                                   % (self._prefix + key))


    def __delattr__(self, key):
        """Deleting an option resets it to default value."""
        if key:
            opt = self._get_opt(key)
            if isinstance(opt, DtOption):
                opt.reset()
            else:
                opt.__delattr__("")
        else:
            for o in self._keyvals.values():
                if isinstance(o, DtOption):
                    o.reset()
                else:
                    o.__delattr__("")


    def __dir__(self):
        return list(self._keyvals.keys())

    def __repr__(self):
        return ("<datatable.options.DtConfig: %s>"
                % ", ".join("%s=%s" % (k, repr(v.get())
                                       if isinstance(v, DtOption) else "...")
                            for k, v in self._keyvals.items()))

    def _repr_pretty_(self, p, cycle):
        with p.indent(4):
            p.text("dt.options." + self._prefix)
            p.break_()
            for k, v in self._keyvals.items():
                p.text(k)
                p.text(" = ")
                if isinstance(v, DtOption):
                    p.pretty(v.get())
                else:
                    p.text("[...]")
                p.break_()


    def get(self, key=""):
        return self.__getattr__(key)

    def set(self, key, val):
        self.__setattr__(key, val)

    def reset(self, key=""):
        self.__delattr__(key)


    def register_option(self, key, xtype, default, doc=None):
        assert isinstance(key, str)
        idot = key.find(".")
        if idot == 0:
            raise TValueError("Invalid option name `%s`" % key)
        elif idot > 0:
            prekey = key[:idot]
            preval = self._keyvals.get(prekey, None)
            if preval is None:
                preval = DtConfig(self._prefix + prekey)
                self._keyvals[prekey] = preval
            if isinstance(preval, DtConfig):
                subkey = key[idot + 1:]
                preval.register_option(subkey, xtype, default, doc)
            else:
                fullkey = self._prefix + key
                fullprekey = self._prefix + prekey
                raise TValueError("Cannot register option `%s` because `%s` "
                                  "is already registered as an option"
                                  % (fullkey, fullprekey))
        elif key in self._keyvals:
            fullkey = self._prefix + key
            raise TValueError("Option `%s` already registered" % fullkey)
        elif not (xtype is callable or is_type(default, xtype)):
            raise TValueError("Default value `%s` is not of type %s"
                              % (default, name_type(xtype)))
        else:
            opt = DtOption(xtype=xtype, default=default, doc=doc,
                           name=self._prefix + key)
            self._keyvals[key] = opt


    def _get_opt(self, key):
        if key in self._keyvals:
            return self._keyvals[key]

        idot = key.find(".")
        if idot >= 0:
            prefix = key[:idot]
            if prefix in self._keyvals:
                subkey = key[idot + 1:]
                return self._keyvals[prefix].__getattr__(subkey)

        fullkey = self._prefix + key
        raise DtAttributeError("Unknown datatable option `%s`" % fullkey)


# Global options store
options = DtConfig()


options.register_option(
    "nthreads", int, default=0,
    doc="The number of OMP threads used by datatable.\n\n"
        "Many calculations in `datatable` module are parallelized using the\n"
        "OpenMP library. This setting controls how many threads will be used\n"
        "during such calculations.\n\n"
        "Initially, this option is set to the value returned by the call\n"
        "`omp_get_max_threads()`. This is usually equal to the number of\n"
        "available processors, but can be altered via environment variables\n"
        "`OMP_NUM_THREADS` or `OMP_THREAD_LIMIT`.\n\n"
        "You can set `nthreads` to a value greater or smaller than the\n"
        "initial setting. For example, setting `nthreads = 1` will force the\n"
        "library into a single-threaded mode. Setting `nthreads` to 0 will\n"
        "restore the initial `omp_get_max_threads()` value. Setting\n"
        "`nthreads` to a value less than 0 is equivalent to requesting that\n"
        "fewer threads than the maximum.\n\n"
        "Note that requesting **too many** threads may exhaust your system\n"
        "resources and cause the Python process to crash.\n")
