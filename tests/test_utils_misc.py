#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from tests import datatable


def test_plural_form():
    plural = datatable.utils.misc.plural_form
    assert plural(0, "egg") == "0 eggs"
    assert plural(1, "egg") == "1 egg"
    assert plural(2, "egg") == "2 eggs"
    assert plural(-1, "egg") == "-1 egg"
    assert plural(-2, "ham") == "-2 hams"
    assert plural(10, "stock") == "10 stocks"
    assert plural(29, "fine") == "29 fines"
    assert plural(29, "kiwi") == "29 kiwis"
    assert plural(102, "box") == "102 boxes"
    assert plural(7, "try") == "7 tries"
    assert plural(8, "elf") == "8 elves"
    assert plural(3, "buff") == "3 buffs"
    assert plural(1021, "loss") == "1021 losses"
    assert plural(4, "mesh") == "4 meshes"
    assert plural(5, "branch") == "5 branches"
    assert plural(1, "foot", "feet") == "1 foot"
    assert plural(7, "foot", "feet") == "7 feet"



def test_clamp():
    clamp = datatable.utils.misc.clamp
    assert clamp(0, 1, 10) == 1
    assert clamp(1, 1, 10) == 1
    assert clamp(2, 1, 10) == 2
    assert clamp(10, 1, 10) == 10
    assert clamp(15, 1, 10) == 10
    assert clamp(-1, -5, -3) == -3
    assert clamp(-2, -5, -3) == -3
    assert clamp(-3, -5, -3) == -3
    assert clamp(-4, -5, -3) == -4
    assert clamp(-5, -5, -3) == -5
    assert clamp(-6, -5, -3) == -5
    assert clamp(0.3, 0, 1) == 0.3
