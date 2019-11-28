#!/usr/bin/env python3
#-------------------------------------------------------------------------------
# Copyright 2018-2019 H2O.ai
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#-------------------------------------------------------------------------------
import enum
from datatable.lib import core


__all__ = ("Expr", "OpCodes", "f", "g")


@enum.unique
class OpCodes(enum.Enum):
    # The values in this enum must be kept in sync with C++ enum `Op` in
    # file "c/expr/op.h"

    # Misc
    NOOP = 0
    COL = 1
    CAST = 2
    SETPLUS = 3
    SETMINUS = 4
    SHIFTFN = 5

    # Unary
    UPLUS = 101
    UMINUS = 102
    UINVERT = 103

    # Binary
    PLUS = 201
    MINUS = 202
    MULTIPLY = 203
    DIVIDE = 204
    INTDIV = 205
    MODULO = 206
    POWEROP = 207
    AND = 208
    XOR = 209
    OR = 210
    LSHIFT = 211
    RSHIFT = 212
    EQ = 213
    NE = 214
    LT = 215
    GT = 216
    LE = 217
    GE = 218

    # String
    RE_MATCH = 301
    LEN = 302

    # Reducers
    MEAN = 401
    MIN = 402
    MAX = 403
    STDEV = 404
    FIRST = 405
    LAST = 406
    SUM = 407
    COUNT = 408
    COUNT0 = 409
    MEDIAN = 410
    COV = 411
    CORR = 412

    # Math: trigonometric
    SIN = 501
    COS = 502
    TAN = 503
    ARCSIN = 504
    ARCCOS = 505
    ARCTAN = 506
    ARCTAN2 = 507
    HYPOT = 508
    DEG2RAD = 509
    RAD2DEG = 510

    # Math: hyperbolic
    SINH = 511
    COSH = 512
    TANH = 513
    ARSINH = 514
    ARCOSH = 515
    ARTANH = 516

    # Math: exponential/power
    CBRT = 517
    EXP = 518
    EXP2 = 519
    EXPM1 = 520
    LOG = 521
    LOG10 = 522
    LOG1P = 523
    LOG2 = 524
    LOGADDEXP = 525
    LOGADDEXP2 = 526
    POWERFN = 527
    SQRT = 528
    SQUARE = 529

    # Math: special
    ERF = 530
    ERFC = 531
    GAMMA = 532
    LGAMMA = 533

    # Math: floating-point
    ABS = 534
    CEIL = 535
    COPYSIGN = 536
    FABS = 537
    FLOOR = 538
    FREXP = 539
    ISCLOSE = 540
    ISFINITE = 541
    ISINF = 542
    ISNA = 543
    LDEXP = 544
    MODF = 545
    RINT = 546
    SIGN = 547
    SIGNBIT = 548
    TRUNC = 549

    # Math: misc
    CLIP = 550
    DIVMOD = 551
    FMOD = 552
    MAXIMUM = 553
    MINIMUM = 554



#-------------------------------------------------------------------------------
# Expr
#-------------------------------------------------------------------------------

class Expr:
    """
    This class represents a function applied to a single datatable row.

    In the MapReduce terminology this class corresponds to the "Map" step.
    Multiple `BaseExpr`s may be composed with each other, in which case they all
    operate simultaneously on the same row.

    This class is the basic building block for evaluation expression(s) that are
    passed as parameters ``i``, ``j``, etc in the main ``DT[i, j, ...]`` call.
    For example, expression

        f.colX > f.colY + 5

    becomes a tree of ``Expr``s:

        Expr(">", Expr("col", "colX"),
                  Expr("+", Expr("col", "colY"), 5))

    The main property of ``Expr`` is the ``._args`` field, which is used from
    C++ engine to evaluate this expression.
    """
    __slots__ = ["_op", "_args", "_params"]


    def __init__(self, op, args, params=()):
        self._op = op if isinstance(op, int) else op.value
        self._args = args
        self._params = params

    def __repr__(self):
        return "Expr:%s(%s; %s)" % (OpCodes(self._op).name.lower(),
                                    ", ".join(repr(x) for x in self._args),
                                    ", ".join(repr(x) for x in self._params))

    def extend(self, other):
        return Expr(OpCodes.SETPLUS, (self, other))

    def remove(self, other):
        return Expr(OpCodes.SETMINUS, (self, other))


    #----- Binary operators ----------------------------------------------------

    def __add__(self, other):
        return Expr(OpCodes.PLUS, (self, other))

    def __sub__(self, other):
        return Expr(OpCodes.MINUS, (self, other))

    def __mul__(self, other):
        return Expr(OpCodes.MULTIPLY, (self, other))

    def __truediv__(self, other):
        return Expr(OpCodes.DIVIDE, (self, other))

    def __floordiv__(self, other):
        return Expr(OpCodes.INTDIV, (self, other))

    def __mod__(self, other):
        return Expr(OpCodes.MODULO, (self, other))

    def __pow__(self, other):
        return Expr(OpCodes.POWEROP, (self, other))

    def __and__(self, other):
        return Expr(OpCodes.AND, (self, other))

    def __xor__(self, other):
        return Expr(OpCodes.XOR, (self, other))

    def __or__(self, other):
        return Expr(OpCodes.OR, (self, other))

    def __lshift__(self, other):
        return Expr(OpCodes.LSHIFT, (self, other))

    def __rshift__(self, other):
        return Expr(OpCodes.RSHIFT, (self, other))


    def __radd__(self, other):
        return Expr(OpCodes.PLUS, (other, self))

    def __rsub__(self, other):
        return Expr(OpCodes.MINUS, (other, self))

    def __rmul__(self, other):
        return Expr(OpCodes.MULTIPLY, (other, self))

    def __rtruediv__(self, other):
        return Expr(OpCodes.DIVIDE, (other, self))

    def __rfloordiv__(self, other):
        return Expr(OpCodes.INTDIV, (other, self))

    def __rmod__(self, other):
        return Expr(OpCodes.MODULO, (other, self))

    def __rpow__(self, other):
        return Expr(OpCodes.POWEROP, (other, self))

    def __rand__(self, other):
        return Expr(OpCodes.AND, (other, self))

    def __rxor__(self, other):
        return Expr(OpCodes.XOR, (other, self))

    def __ror__(self, other):
        return Expr(OpCodes.OR, (other, self))

    def __rlshift__(self, other):
        return Expr(OpCodes.LSHIFT, (other, self))

    def __rrshift__(self, other):
        return Expr(OpCodes.RSHIFT, (other, self))


    #----- Relational operators ------------------------------------------------

    def __eq__(self, other):
        return Expr(OpCodes.EQ, (self, other))

    def __ne__(self, other):
        return Expr(OpCodes.NE, (self, other))

    def __lt__(self, other):
        return Expr(OpCodes.LT, (self, other))

    def __gt__(self, other):
        return Expr(OpCodes.GT, (self, other))

    def __le__(self, other):
        return Expr(OpCodes.LE, (self, other))

    def __ge__(self, other):
        return Expr(OpCodes.GE, (self, other))


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
        return Expr(OpCodes.UINVERT, (self,))

    def __neg__(self):
        """Unary minus: -expr."""
        return Expr(OpCodes.UMINUS, (self,))

    def __pos__(self):
        """Unary plus (no-op)."""
        return Expr(OpCodes.UPLUS, (self,))


    #----- String functions ----------------------------------------------------

    def len(self):
        return Expr(OpCodes.LEN, (self,))

    def re_match(self, pattern, flags=None):
        return Expr(OpCodes.RE_MATCH, (self,), (pattern, flags))



#-------------------------------------------------------------------------------
# FrameProxy
#-------------------------------------------------------------------------------

class FrameProxy:
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
    __slots__ = ["_id"]

    # Developer notes:
    # This class uses dynamic name resolution to convert arbitrary attribute
    # strings into proper column indices. Thus, we should avoid using any
    # internal attributes or method names that have a chance of clashing with
    # user's column names.

    def __init__(self, id_):
        self._id = id_


    def __getattr__(self, name):
        """Retrieve column `name` from the datatable."""
        return self[name]


    def __getitem__(self, item):
        if not isinstance(item, (int, str, slice)):
            from datatable import TypeError, stype, ltype
            if not(item in [bool, int, float, str, object, None] or
                   isinstance(item, (stype, ltype))):
                raise TypeError("Column selector should be an integer, string, "
                                "or slice, not %r" % type(item))
        return Expr(OpCodes.COL, (item,), (self._id,))



f = FrameProxy(0)
g = FrameProxy(1)
core._register_function(9, Expr);
