#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
#-------------------------------------------------------------------------------
#
# Test creating a datatable from various sources
#
#-------------------------------------------------------------------------------
import pytest
import datatable as dt


def test_0():
    d0 = dt.DataTable([1, 2, 3])
    assert d0.shape == (3, 1)
    assert d0.names == ("C1", )
    assert d0.types == ("int", )
    d1 = dt.DataTable([[1, 2], [True, False], [.3, -0]], colnames="ABC")
    assert d1.shape == (2, 3)
    assert d1.names == ("A", "B", "C")
    assert d1.types == ("int", "bool", "real")
    d2 = dt.DataTable((3, 5, 6, 0))
    assert d2.shape == (4, 1)
    assert d2.types == ("int", )
    d3 = dt.DataTable({1, 13, 15, -16, -10, 7, 9, 1})
    assert d3.shape == (7, 1)
    assert d3.types == ("int", )
    d4 = dt.DataTable()
    assert d4.shape == (0, 0)
    assert d4.names == tuple()
    assert d4.types == tuple()
    assert d4.stypes == tuple()
    d5 = dt.DataTable([])
    assert d5.shape == (0, 0)
    assert d5.names == tuple()
    assert d5.types == tuple()
    assert d5.stypes == tuple()
    d6 = dt.DataTable([[]])
    assert d6.shape == (0, 1)
    assert d6.names == ("C1", )
    assert d6.types == ("bool", )

    with pytest.raises(TypeError) as e:
        dt.DataTable("scratch")
    assert "Cannot create DataTable from 'scratch'" in str(e.value)


def test_issue_42():
    d = dt.DataTable([-1])
    assert d.shape == (1, 1)
    assert d.types == ("int", )
    d = dt.DataTable([-1, 2, 5, "hooray"])
    assert d.shape == (4, 1)
    assert d.types == ("str", )
