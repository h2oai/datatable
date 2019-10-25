#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import pytest
import datatable as dt
from datatable.internal import frame_integrity_check
from tests import noop


def test_options_all():
    # Update this test every time a new option is added
    assert repr(dt.options).startswith("datatable.options.")
    assert set(dir(dt.options)) == {
        "nthreads",
        "core_logger",
        "sort",
        "display",
        "frame",
        "fread",
        "progress",
    }
    assert set(dir(dt.options.sort)) == {
        "insert_method_threshold",
        "max_chunk_length",
        "max_radix_bits",
        "nthreads",
        "over_radix_bits",
        "thread_multiplier",
    }
    assert set(dir(dt.options.display)) == {
        "allow_unicode",
        "head_nrows",
        "interactive",
        "interactive_hint",
        "max_nrows",
        "max_column_width",
        "tail_nrows",
        "use_colors"
    }
    assert set(dir(dt.options.frame)) == {
        "names_auto_index",
        "names_auto_prefix",
    }
    assert set(dir(dt.options.fread)) == {
        "anonymize",
        "log",
    }
    assert set(dir(dt.options.progress)) == {
        "callback",
        "clear_on_success",
        "enabled",
        "min_duration",
        "updates_per_second",
    }


def test_option_api():
    dt.options.register_option(name="fooo", xtype=int, default=13,
                                doc="a dozen")
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


def test_option_bad():
    with pytest.raises(AttributeError):
        noop(dt.options.gooo)

    with pytest.raises(TypeError) as e:
        dt.options.register_option(name="gooo", xtype=str, default=3, doc="??")
    assert "Default value `3` is not of type str" in str(e.value)

    with pytest.raises(ValueError) as e:
        dt.options.register_option(name=".hidden", xtype=int, default=0)
    assert "Invalid option name `.hidden`" in str(e.value)

    dt.options.register_option(name="gooo", xtype=int, default=3)

    with pytest.raises(ValueError) as e:
        dt.options.register_option(name="gooo", xtype=int, default=4, doc="???")
    assert "Option `gooo` already registered" in str(e.value)

    with pytest.raises(TypeError) as e:
        dt.options.gooo = 2.5
    assert ("Invalid value for option `gooo`: expected int, instead got float"
            in str(e.value))


def test_option_suggest():
    with pytest.raises(AttributeError) as e:
        dt.options.fread.log.escapeunicode = False
    assert ("did you mean `fread.log.escape_unicode`?" in str(e.value))


def test_options_many():
    dt.options.register_option("tmp1.alpha", 1, doc="A", xtype=int)
    dt.options.register_option("tmp1.beta",  2, doc="B", xtype=int)
    dt.options.register_option("tmp1.gamma", 3, doc="C", xtype=int)
    dt.options.register_option("tmp1.delta.x", 4, doc="X", xtype=int)
    dt.options.register_option("tmp1.delta.y", 5, doc="Y", xtype=int)
    dt.options.register_option("tmp1.delta.z.zz", 6, doc="Z", xtype=int)
    for _ in [1, 2]:
        assert dt.options.tmp1.alpha == 1
        assert dt.options.tmp1.beta == 2
        assert dt.options.tmp1.gamma == 3
        assert dt.options.tmp1.delta.x == 4
        assert dt.options.tmp1.delta.y == 5
        assert dt.options.tmp1.delta.z.zz == 6
        assert set(dir(dt.options.tmp1)) == {"alpha", "beta", "gamma", "delta"}
        assert set(dir(dt.options.tmp1.delta)) == {"x", "y", "z"}
        dt.options.tmp1.delta.x = 0
        dt.options.tmp1.delta.z.zz = 0
        dt.options.tmp1.reset()


def test_options_many_bad():
    dt.options.register_option("tmp2.foo.x", 4, xtype=int)
    dt.options.register_option("tmp2.foo.y", 5, xtype=int)
    dt.options.tmp2.foo.x = 8
    assert dt.options.tmp2.foo.x == 8
    assert dt.options.get("tmp2.foo.x") == 8
    with pytest.raises(TypeError) as e:
        dt.options.tmp2.foo = 0
    assert ("Cannot assign a value to group of options `tmp2.foo.*`"
            in str(e.value))


def test_options_context1():
    with dt.options.context(**{"fread.log.anonymize": True}):
        assert dt.options.fread.log.anonymize is True
    assert dt.options.fread.log.anonymize is False


def test_options_context2():
    with dt.options.fread.log.context(escape_unicode=True):
        assert dt.options.fread.log.escape_unicode is True
    assert dt.options.fread.log.escape_unicode is False


def test_options_context3():
    # Check that in case of exception the value still gets restored
    dt.options.register_option("zoonk", 1, xtype=int)
    try:
        with dt.options.context(zoonk=100):
            raise RuntimeError
    except RuntimeError:
        pass
    assert dt.options.zoonk == 1


def test_options_context4():
    # Check that if context requires changing multiple parameters, that all
    # their values are properly restored in the end, even if there is an
    # exception during setting one of the parameters
    dt.options.register_option("tmp1.odyn", 1, xtype=int)
    dt.options.register_option("tmp1.dwa", 2, xtype=int)
    try:
        with dt.options.tmp1.context(odyn=10, dwa=None):
            assert False, "Should not be able to enter this context"
    except TypeError:
        pass
    assert dt.options.tmp1.odyn == 1
    assert dt.options.tmp1.dwa == 2




#-------------------------------------------------------------------------------
# Individual options
#-------------------------------------------------------------------------------

def test_nthreads():
    from datatable.internal import get_thread_ids
    nthreads0 = dt.options.nthreads
    curr_threads = get_thread_ids()
    assert len(curr_threads) == nthreads0

    for n in [4, 1, nthreads0 + 10, nthreads0]:
        dt.options.nthreads = n
        new_threads = get_thread_ids()
        assert len(new_threads) == n
        m = min(len(curr_threads), len(new_threads))
        assert curr_threads[:m] == new_threads[:m]
        curr_threads = new_threads

    assert dt.options.nthreads == nthreads0


def test_frame_names_auto_index():
    assert dt.options.frame.names_auto_index == 0
    dt.options.frame.names_auto_index = 1
    f0 = dt.Frame([[1], [2], [3], [4]])
    assert f0.names == ("C1", "C2", "C3", "C4")
    dt.options.frame.names_auto_index = 999
    f1 = dt.Frame([[1], [2], [3], [4]])
    assert f1.names == ("C999", "C1000", "C1001", "C1002")
    del dt.options.frame.names_auto_index
    f2 = dt.Frame([[1], [2], [3], [4]])
    assert f2.names == ("C0", "C1", "C2", "C3")
    with pytest.raises(TypeError):
        dt.options.frame.names_auto_index = "C"


def test_frame_names_auto_prefix():
    assert dt.options.frame.names_auto_prefix == "C"
    dt.options.frame.names_auto_prefix = "foo"
    f0 = dt.Frame([[3], [3], [3]])
    assert f0.names == ("foo0", "foo1", "foo2")
    del dt.options.frame.names_auto_prefix
    f2 = dt.Frame([[1], [2], [3], [4]])
    assert f2.names == ("C0", "C1", "C2", "C3")
    with pytest.raises(TypeError):
        dt.options.frame.names_auto_prefix = 0
