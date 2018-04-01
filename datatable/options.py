#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
from datatable.lib import core as _datatable
from datatable.utils.typechecks import (is_type, name_type, TTypeError,
                                        TValueError)

__all__ = ("options", )


class DtAttributeError(AttributeError):
    _handle_ = TTypeError._handle_



class DtOption:
    __slots__ = ["_name", "_value", "xtype", "default", "doc", "_core"]

    def __init__(self, name, xtype, default, doc=None, core=None):
        self._name = name
        self._core = core
        self._value = None
        self.xtype = xtype
        self.default = default
        self.doc = doc
        self.value = default  # Set value, possibly reporting to core too

    @property
    def value(self):
        return self._value

    @value.setter
    def value(self, v):
        self._value = v
        if self._core:
            _datatable.set_option(self._name, v)



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
                return opt.value
            else:
                return opt
        else:
            res = {}
            for k, v in self._keyvals.items():
                if isinstance(v, DtOption):
                    res[self._prefix + k] = v.value
                else:
                    res.update(v.__getattr__(""))
            return res


    def __setattr__(self, key, val):
        opt = self._get_opt(key)
        if isinstance(opt, DtOption):
            if is_type(val, opt.xtype):
                opt.value = val
            else:
                fullkey = self._prefix + key
                exptype = name_type(opt.xtype)
                acttype = name_type(type(val))
                raise TTypeError("Invalid value for option `%s`: expected "
                                 "type %s, got %s instead"
                                 % (fullkey, exptype, acttype))
        else:
            raise DtAttributeError("Cannot modify group of options `%s`"
                                   % (self._prefix + key))


    def __delattr__(self, key):
        """Deleting an option resets it to default value."""
        if key:
            opt = self._get_opt(key)
            if isinstance(opt, DtOption):
                opt.value = opt.default
            else:
                opt.__delattr__("")
        else:
            for o in self._keyvals.values():
                if isinstance(o, DtOption):
                    o.value = o.default
                else:
                    o.__delattr__("")


    def __dir__(self):
        return list(self._keyvals.keys())

    def __repr__(self):
        return ("<datatble.options.DtConfig: %s>"
                % ", ".join("%s=%r" % (k, v.value)
                            for k, v in self._keyvals.items()))


    def get(self, key=""):
        return self.__getattr__(key)

    def set(self, key, val):
        self.__setattr__(key, val)

    def reset(self, key=""):
        self.__delattr__(key)


    def register_option(self, key, xtype, default, doc=None, core=False):
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
                preval.register_option(subkey, xtype, default, doc, core)
            else:
                fullkey = self._prefix + key
                fullprekey = self._prefix + prekey
                raise TValueError("Cannot register option `%s` because `%s` "
                                  "is already registered as an option"
                                  % (fullkey, fullprekey))
        elif key in self._keyvals:
            fullkey = self._prefix + key
            raise TValueError("Option `%s` already registered" % fullkey)
        elif not is_type(default, xtype):
            raise TValueError("Default value `%s` is not of type %s"
                              % (default, name_type(xtype)))
        else:
            opt = DtOption(xtype=xtype, default=default, doc=doc, core=core,
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
