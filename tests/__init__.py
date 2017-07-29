#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import sys


def same_iterables(a, b):
    """
    Convenience function for testing datatables created from dictionaries.

    On Python3.6+ it simply checks whether `a == b`. However on Python3.5, it
    checks whether `a` and `b` have same but potentially in a different order.

    The reason for this helper function is the difference between semantics of
    a dictionary in Python3.6 versus older versions: in Python3.6 dictionaries
    preserved the order of their keys, whereas in previous Python versions they
    didn't. Thus, if one creates say a datatable

        dt = datatable.DataTable({"A": 1, "B": "foo", "C": 3.5})

    then in Python3.6 it will be guaranteed to have columns ("A", "B", "C") --
    in this order, whereas in Python3.5 or older the order may be arbitrary.
    Thus, this function is designed to help with testing of such a datatable:

        assert same_iterables(dt.names, ("A", "B", "C"))
        assert same_iterables(dt.types, ("int", "string", "float"))

    Then in Python3.6 lhs and rhs will be tested with strict equality, whereas
    in older Python versions the test will be weaker, taking into account the
    non-deterministic nature of the dictionary that created the datatable.
    """
    if sys.version_info >= (3, 6):
        return a == b
    else:
        if type(a) != type(b) or len(a) != len(b):
            return False
        js = set(range(len(a)))
        for i in range(len(a)):
            found = False
            for j in js:
                if a[i] == b[j]:
                    found = True
                    js.remove(j)
                    break
            if not found:
                return False
        return True


def assert_equals(datatable1, datatable2):
    """
    Helper function to assert that 2 datatables are equal to each other.
    """
    nrows, ncols = datatable1.shape
    assert datatable1.internal.check()
    assert datatable2.internal.check()
    assert datatable1.shape == datatable2.shape
    assert same_iterables(datatable1.names, datatable2.names)
    assert same_iterables(datatable1.types, datatable2.types)
    assert same_iterables(datatable1.topython(), datatable2.topython())
