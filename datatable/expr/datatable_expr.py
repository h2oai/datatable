#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .column_expr import ColSelectorExpr



class DatatableExpr(object):
    """
    Wrapper around a Datatable object that ensures lazy evaluation semantics.

    This is the object that is passed to lambda-expressions `rows`, `select`,
    `update`, etc when performing the ``datatable(rows=..., select=...)`` call.
    The primary use of this object is as a namespace for datable's columns:
    an expression such as

        f.colName

    (where ``f`` is a :class:`DatatableExpr`) will evaluate to a column whose
    name is "colName", provided such column exists in the datatable.
    """

    # Developer notes:
    # This class uses dynamic name resolution to convert arbitrary attribute
    # strings into proper column indices. Thus, we should avoid using any
    # internal attributes or method names that have a chance of clashing with
    # user's column names.

    def __init__(self, datatable):
        self._datatable = datatable
        self._datatable_colmap = datatable.internal.view_colnumbers


    def __getattr__(self, name):
        """Retrieve column `name` from the datatable."""
        i = self._datatable.colindex(name)
        return ColSelectorExpr(self, i)


    def __getitem__(self, item):
        if isinstance(item, str):
            i = self._datatable.colindex(item)
        elif isinstance(item, int):
            ncols = self._datatable.ncols
            if item < -ncols or item >= ncols:
                raise ValueError("Invalid index %d for datatable %r" %
                                 (item, self._datatable))
            i = item % ncols
            assert 0 <= i < ncols
        else:
            raise TypeError("Invalid selector %r" % item)
        return ColSelectorExpr(self, i)


    def __repr__(self):
        return "DatatableExpr(dt#%d)" % self._datatable._id


    def __str__(self):
        return "dt" + str(self._datatable._id)


    def __dir__(self):
        # This will supply column names for tab completion
        # (is there a context where tab completion will be used, I wonder?)
        return self._datatable.names


    # ---- Pass-through properties ---------------------------------------------

    @property
    def ncols(self):
        return self._datatable.ncols

    @property
    def nrows(self):
        return self._datatable.nrows

    @property
    def shape(self):
        return self._datatable.shape

    @property
    def names(self):
        return self._datatable.names

    @property
    def types(self):
        return self._datatable.types

    @property
    def stypes(self):
        return self._datatable.stypes


    def get_column_srcindex(self, colindex):
        if self._datatable_colmap is None:
            return None
        else:
            return self._datatable_colmap[colindex]


    def get_dtname(self):
        return "f"


    def get_datatable(self):
        return self._datatable
