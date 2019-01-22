#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------

from .column_expr import ColSelectorExpr

__all__ = ("f", "g")



class DatatableProxy(object):
    """
    Helper object that enables lazy evaluation semantics.

    The "standard" instance of this class is called ``f`` and is exported in the
    main :module:`datatable` namespace. This variable is only used in the
    ``datatable(rows=..., select=...)`` call, and serves as a namespace for the
    source datatable's columns. For example,

        f.colName

    within the ``datatable(...)`` call will evaluate to the ``datatable``'s
    column whose name is "colName".

    Additionally, there is also instance ``ff`` which is used to refer to the
    second Frame in expressions involving two DataTables (joins).

    The purpose of ``f`` is:
      1. to serve as a shorthand for the Frame being operated on, which
         usually improves formulas readability;
      2. to allow accessing columns via "." syntax, rather than using more
         verbose square brackets;
      3. to provide lazy evaluation semantics: for example if ``d`` is a
         Frame, then ``d["A"] + d["B"] + d["C"]`` evaluates immediately in
         2 steps and returns a single-column Frame containing sum of
         columns A, B and C. At the same time ``f.A + f.B + f.C`` does not
         evaluate at all until after it is applied to ``d``, and then it is
         computed in a single step, optimized depending on the presence of
         any other components of a single Frame call.

    Examples
    --------
    >>> import datatable as dt
    >>> from datatable import f  # Standard name for a DatatableProxy object
    >>> d0 = dt.Frame([[1, 2, 3], [4, 5, -6], [0, 2, 7]], names=list("ABC"))
    >>>
    >>> # Select all rows where column A is positive
    >>> d1 = d0[f.A > 0, :]
    >>> # Construct column D which is a sum of A, B, and C
    >>> d2 = d0[:, {"D": f.A + f.B + f.C}]
    >>>
    >>> # If needed, formulas based on `f` can be stored and reused later
    >>> expr = f.A + f.B + f.C
    >>> d3 = d0[:, expr]
    >>> d4 = d0[expr > 0, expr]
    """
    __slots__ = ["_datatable", "_rowindex", "_name", "_id"]

    # Developer notes:
    # This class uses dynamic name resolution to convert arbitrary attribute
    # strings into proper column indices. Thus, we should avoid using any
    # internal attributes or method names that have a chance of clashing with
    # user's column names.

    def __init__(self, name, id_):
        self._datatable = None
        self._rowindex = None
        self._name = name
        self._id = id_


    def get_datatable(self):
        return self._datatable

    def get_rowindex(self):
        return self._rowindex

    def set_rowindex(self, ri):
        self._rowindex = ri


    def bind_datatable(self, dt):
        """Used internally for context manager."""
        self._datatable = dt
        return self

    def __enter__(self):
        pass

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._datatable = None
        self._rowindex = None
        # Don't return anything, so that the exception is propagated


    def __getattr__(self, name):
        """Retrieve column `name` from the datatable."""
        if name == "_display_in_terminal_":
            raise AttributeError
        return ColSelectorExpr(self, name)


    def __getitem__(self, item):
        return ColSelectorExpr(self, item)


    def __repr__(self):
        return "FrameProxy('%s', %s)" % (self._name, repr(self._datatable))


    def __str__(self):
        return self._name


    def __dir__(self):
        # This will supply column names for tab completion
        # (is there a context where tab completion will be used, I wonder?)
        if self._datatable:
            return self._datatable.names
        else:
            return []


    # ---- Pass-through properties ---------------------------------------------

    @property
    def ncols(self):
        if self._datatable:
            return self._datatable.ncols

    @property
    def nrows(self):
        if self._datatable:
            return self._datatable.nrows

    @property
    def shape(self):
        if self._datatable:
            return self._datatable.shape

    @property
    def names(self):
        if self._datatable:
            return self._datatable.names

    @property
    def types(self):
        if self._datatable:
            return self._datatable.types

    @property
    def stypes(self):
        if self._datatable:
            return self._datatable.stypes


f = DatatableProxy("f", 0)
g = DatatableProxy("g", 1)
