#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import pytest
import datatable as dt


@pytest.mark.run(order=1001)
def test_options_all():
    # Update this test every time a new option is added
    assert dir(dt.options) == []


@pytest.mark.run(order=1002)
def test_option_api():
    dt.options.register_option("fooo", int, 13, "a dozen")
    assert "fooo" in dir(dt.options)
    assert dt.options.fooo == 13
    assert dt.options.get("fooo") == 13
    dt.options.fooo = 23
    assert dt.options.fooo == 23
    dt.options.set("fooo", 25)
    assert dt.options.fooo == 25
    del dt.options.fooo
    assert dt.options.fooo == 13
    dt.options.fooo = 0
    dt.options.reset("fooo")
    assert dt.options.fooo == 13


@pytest.mark.run(order=1003)
def test_option_bad():
    with pytest.raises(AttributeError):
        dt.options.gooo

    with pytest.raises(ValueError) as e:
        dt.options.register_option("gooo", str, 3, "??")
    assert "Default value `3` is not of type str" in str(e.value)

    with pytest.raises(ValueError) as e:
        dt.options.register_option(".hidden", int, 0, "")
    assert "Invalid option name `.hidden`" in str(e.value)

    dt.options.register_option("gooo", int, 3, "???")

    with pytest.raises(ValueError) as e:
        dt.options.register_option("gooo", int, 3, "???")
    assert "Option `gooo` already registered" in str(e.value)

    with pytest.raises(TypeError) as e:
        dt.options.gooo = 2.5
    assert ("Invalid value for option `gooo`: expected type int, got float "
            "instead" in str(e.value))

    with pytest.raises(ValueError) as e:
        dt.options.register_option("gooo.maxima", int, 0, "")
    assert ("Cannot register option `gooo.maxima` because `gooo` is already "
            "registered as an option" in str(e.value))


@pytest.mark.run(order=1004)
def test_options_many():
    dt.options.register_option("tmp1.alpha", int, 1, "A")
    dt.options.register_option("tmp1.beta",  int, 2, "B")
    dt.options.register_option("tmp1.gamma", int, 3, "C")
    dt.options.register_option("tmp1.delta.x", int, 4, "X")
    dt.options.register_option("tmp1.delta.y", int, 5, "Y")
    dt.options.register_option("tmp1.delta.z.zz", int, 6, "Z")
    for _ in [1, 2]:
        assert dt.options.tmp1.alpha == 1
        assert dt.options.tmp1.beta == 2
        assert dt.options.tmp1.gamma == 3
        assert dt.options.tmp1.delta.x == 4
        assert dt.options.tmp1.delta.y == 5
        assert dt.options.tmp1.delta.z.zz == 6
        assert set(dir(dt.options.tmp1)) == {"alpha", "beta", "gamma", "delta"}
        assert set(dt.options.tmp1.get()) == {"tmp1.alpha", "tmp1.beta",
                                              "tmp1.gamma", "tmp1.delta.x",
                                              "tmp1.delta.y", "tmp1.delta.z.zz"}
        dt.options.tmp1.delta.x = 0
        dt.options.tmp1.delta.z.zz = 0
        del dt.options.tmp1


@pytest.mark.run(order=1004)
def test_options_many_bad():
    dt.options.register_option("tmp2.foo.x", int, 4, "")
    dt.options.register_option("tmp2.foo.y", int, 5, "")
    dt.options.tmp2.foo.x = 8
    assert dt.options.tmp2.foo.x == 8
    assert dt.options.get("tmp2.foo.x") == 8
    with pytest.raises(AttributeError) as e:
        dt.options.tmp2.foo = 0
    assert "Cannot modify group of options `tmp2.foo`" in str(e.value)
