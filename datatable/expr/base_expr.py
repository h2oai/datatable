#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import datatable
from .consts import ctypes_map, nas_map




class BaseExpr:
    """
    This class represents a function applied to a single datatable row.

    In the MapReduce terminology this class corresponds to the "Map" step.
    Multiple `BaseExpr`s may be composed with each other, in which case they all
    operate simultaneously on the same row.

    This class is the basic building block for evaluation expression(s) that are
    passed as parameters ``rows``, ``select``, etc in the main
    ``datatable(...)`` call. For example, expression

        f.colX > f.colY + 5

    becomes a tree of :class:`BaseExpr`s, where the root of the tree is the
    :class:`RelationalOpExpr` node with `op = "gt"` and 2 child nodes
    representing sub-trees `f.colX` and `f.colY + 5`. The former is a
    :class:`ColSelectorExpr` object corresponding to column "colX", and the
    latter is a :class:`BinaryOpExpr` node with `op = "add"` and 2 more
    children: `f.colY` and `5`.

    Once built, the tree of ``BaseExpr``s is then used to generate C code for
    evaluation of the expression. The C code is produced within the context of
    an :class:`IteratorNode`, which encapsulates a C function that performs
    iteration over the rows of a datatable.

    Each ``BaseExpr`` maps to a single (scalar) value, having a particular
    ``stype``. Multi-column expressions are not supported (yet).

    This class is abstract and should not be instantiated explicitly.
    """
    __slots__ = ["_stype"]


    def __init__(self):
        self._stype = None

    def resolve(self):
        """
        Part of initialization process: this method will be called once it is
        known which Frame this expression should bind to. Derived classes
        are expected to override this method and perform `_stype` resolution /
        error checking.
        """
        raise NotImplementedError


    @property
    def stype(self):
        """
        "Storage type" of the column produced by this expression.

        The stype is an enum declared in `types.py`.

        Each class deriving from ``BaseExpr`` is expected to set the
        ``self._stype`` property in its initializer.
        """
        assert self._stype is not None
        return self._stype


    @property
    def ctype(self):
        """
        C type of an individual element produced by this expression.

        This is a helper property useful for code generation. For the string
        columns it will return the type of the element within the "offsets"
        part of the data.
        """
        return ctypes_map[self._stype]


    @property
    def itype(self):
        """
        SType of the element, expressed as an integer (same as ST_* constants).
        """
        return self._stype.value


    def _core(self):
        """
        Converts this object into a ``_datatable.base_expr``.
        """
        raise NotImplementedError


    #----- Binary operators ----------------------------------------------------

    def __add__(self, other):
        return datatable.expr.BinaryOpExpr(self, "+", other)

    def __sub__(self, other):
        return datatable.expr.BinaryOpExpr(self, "-", other)

    def __mul__(self, other):
        return datatable.expr.BinaryOpExpr(self, "*", other)

    def __truediv__(self, other):
        return datatable.expr.BinaryOpExpr(self, "/", other)

    def __floordiv__(self, other):
        return datatable.expr.BinaryOpExpr(self, "//", other)

    def __mod__(self, other):
        return datatable.expr.BinaryOpExpr(self, "%", other)

    def __pow__(self, other):
        return datatable.expr.BinaryOpExpr(self, "**", other)

    def __and__(self, other):
        return datatable.expr.BinaryOpExpr(self, "&", other)

    def __xor__(self, other):
        return datatable.expr.BinaryOpExpr(self, "^", other)

    def __or__(self, other):
        return datatable.expr.BinaryOpExpr(self, "|", other)

    def __lshift__(self, other):
        return datatable.expr.BinaryOpExpr(self, "<<", other)

    def __rshift__(self, other):
        return datatable.expr.BinaryOpExpr(self, ">>", other)


    def __radd__(self, other):
        return datatable.expr.BinaryOpExpr(other, "+", self)

    def __rsub__(self, other):
        return datatable.expr.BinaryOpExpr(other, "-", self)

    def __rmul__(self, other):
        return datatable.expr.BinaryOpExpr(other, "*", self)

    def __rtruediv__(self, other):
        return datatable.expr.BinaryOpExpr(other, "/", self)

    def __rfloordiv__(self, other):
        return datatable.expr.BinaryOpExpr(other, "//", self)

    def __rmod__(self, other):
        return datatable.expr.BinaryOpExpr(other, "%", self)

    def __rpow__(self, other):
        return datatable.expr.BinaryOpExpr(other, "**", self)

    def __rand__(self, other):
        return datatable.expr.BinaryOpExpr(other, "&", self)

    def __rxor__(self, other):
        return datatable.expr.BinaryOpExpr(other, "^", self)

    def __ror__(self, other):
        return datatable.expr.BinaryOpExpr(other, "|", self)

    def __rlshift__(self, other):
        return datatable.expr.BinaryOpExpr(other, "<<", self)

    def __rrshift__(self, other):
        return datatable.expr.BinaryOpExpr(other, ">>", self)


    #----- Relational operators ------------------------------------------------

    def __eq__(self, other):
        return datatable.expr.RelationalOpExpr(self, "==", other)

    def __ne__(self, other):
        return datatable.expr.RelationalOpExpr(self, "!=", other)

    def __lt__(self, other):
        return datatable.expr.RelationalOpExpr(self, "<", other)

    def __gt__(self, other):
        return datatable.expr.RelationalOpExpr(self, ">", other)

    def __le__(self, other):
        return datatable.expr.RelationalOpExpr(self, "<=", other)

    def __ge__(self, other):
        return datatable.expr.RelationalOpExpr(self, ">=", other)


    #----- Unary operators -----------------------------------------------------

    def __bool__(self):
        """Coercion to boolean: forbidden."""
        raise TypeError(
            "Expression %s cannot be cast to bool.\n\n"
            "You may be seeing this error because either:\n"
            "  * you tried to use chained inequality such as\n"
            "        0 < f.A < 100\n"
            "    If so please rewrite it as\n"
            "        (0 < f.A) & (f.A < 100)\n\n"
            "  * you used keywords and/or, for example\n"
            "        f.A < 0 or f.B >= 1\n"
            "    If so then replace keywords with operators `&` or `|`:\n"
            "        (f.A < 0) | (f.B >= 1)\n"
            "    Be mindful that `&` / `|` have higher precedence than `and`\n"
            "    or `or`, so make sure to use parentheses appropriately.\n\n"
            "  * you used expression in the `if` statement, for example:\n"
            "        f.A if f.A > 0 else -f.A\n"
            "    You may write this as a ternary operator instead:\n"
            "        (f.A > 0) & f.A | -f.A\n\n"
            "  * you explicitly cast the expression into `bool`:\n"
            "        bool(f.B)\n"
            "    this can be replaced with an explicit comparison operator:\n"
            "        f.B != 0\n"
            % self
        )

    def __invert__(self):
        """Unary inversion: ~expr."""
        return datatable.expr.UnaryOpExpr("~", self)

    def __neg__(self):
        """Unary minus: -expr."""
        return datatable.expr.UnaryOpExpr("-", self)

    def __pos__(self):
        """Unary plus (no-op)."""
        return datatable.expr.UnaryOpExpr("+", self)



    #----- Code generation -----------------------------------------------------

    def __str__(self):
        """
        String representation of the expression.

        This method is not just for debug purposes. Stringified expression is
        used as a key in the dictionary of all objects that were evaluated, to
        ensure that no subexpression is evaluated more than once. Thus, __str__
        should return representation that is in 1-to-1 correspondence with the
        expression being evaluated (within the single `IteratorNode`).

        Expression returned by __str__ should be properly parenthesized to
        ensure that it can be used as-is to form combinations with other
        expressions.
        """
        raise NotImplementedError("Class %s should implement method __str__"
                                  % self.__class__.__name__)

    def __repr__(self):
        return "%s(%s)" % (self.__class__.__name__, self)


    def safe_name(self):
        return str(self)
