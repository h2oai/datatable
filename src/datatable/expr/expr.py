#!/usr/bin/env python3
#-------------------------------------------------------------------------------
# Copyright 2018-2020 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
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
    SETPLUS = 3
    SETMINUS = 4
    SHIFTFN = 5

    # Unary
    UPLUS = 101
    UMINUS = 102
    UINVERT = 103

    # Binary
    AND = 208
    XOR = 209
    OR = 210
    LSHIFT = 211
    RSHIFT = 212

    # String
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
        return core.FExpr(self).extend(other)

    def remove(self, other):
        return core.FExpr(self).remove(other)


    #----- Binary operators ----------------------------------------------------

    def __add__(self, other):
        return core.FExpr(self) + core.FExpr(other)

    def __sub__(self, other):
        return core.FExpr(self) - core.FExpr(other)

    def __mul__(self, other):
        return core.FExpr(self) * core.FExpr(other)

    def __truediv__(self, other):
        return core.FExpr(self) / core.FExpr(other)

    def __floordiv__(self, other):
        return core.FExpr(self) // core.FExpr(other)

    def __mod__(self, other):
        return core.FExpr(self) % core.FExpr(other)

    def __pow__(self, other):
        return core.FExpr(self) ** core.FExpr(other)

    def __and__(self, other):
        return core.FExpr(self) & core.FExpr(other)

    def __xor__(self, other):
        return core.FExpr(self) ^ core.FExpr(other)

    def __or__(self, other):
        return core.FExpr(self) | core.FExpr(other)

    def __lshift__(self, other):
        return core.FExpr(self) << core.FExpr(other)

    def __rshift__(self, other):
        return core.FExpr(self) >> core.FExpr(other)


    def __radd__(self, other):
        return core.FExpr(other) + core.FExpr(self)

    def __rsub__(self, other):
        return core.FExpr(other) - core.FExpr(self)

    def __rmul__(self, other):
        return core.FExpr(other) * core.FExpr(self)

    def __rtruediv__(self, other):
        return core.FExpr(other) / core.FExpr(self)

    def __rfloordiv__(self, other):
        return core.FExpr(other) // core.FExpr(self)

    def __rmod__(self, other):
        return core.FExpr(other) % core.FExpr(self)

    def __rpow__(self, other):
        return core.FExpr(other) ** core.FExpr(self)

    def __rand__(self, other):
        return core.FExpr(other) & core.FExpr(self)

    def __rxor__(self, other):
        return core.FExpr(other) ^ core.FExpr(self)

    def __ror__(self, other):
        return core.FExpr(other) | core.FExpr(self)

    def __rlshift__(self, other):
        return core.FExpr(other) << core.FExpr(self)

    def __rrshift__(self, other):
        return core.FExpr(other) >> core.FExpr(self)


    #----- Relational operators ------------------------------------------------

    def __eq__(self, other):
        return core.FExpr(self) == core.FExpr(other)

    def __ne__(self, other):
        return core.FExpr(self) != core.FExpr(other)

    def __lt__(self, other):
        return core.FExpr(self) < core.FExpr(other)

    def __gt__(self, other):
        return core.FExpr(self) > core.FExpr(other)

    def __le__(self, other):
        return core.FExpr(self) <= core.FExpr(other)

    def __ge__(self, other):
        return core.FExpr(self) >= core.FExpr(other)


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
        return ~core.FExpr(self)

    def __neg__(self):
        """Unary minus: -expr."""
        return -core.FExpr(self)

    def __pos__(self):
        """Unary plus (no-op)."""
        return +core.FExpr(self)


    #----- String functions ----------------------------------------------------

    def len(self):
        return core.FExpr(self).len()

    def re_match(self, pattern, flags=None):
        return core.FExpr(self).re_match(pattern)  # will warn



f = core.Namespace()
g = core.Namespace()
core._register_function(9, Expr);
