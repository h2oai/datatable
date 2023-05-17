#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2021 H2O.ai
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
import pytest
import re
import datatable as dt
from datatable.internal import frame_integrity_check
from tests import noop


def test_options_all():
    # Update this test every time a new option is added
    assert repr(dt.options).startswith("datatable.options.")
    assert set(dir(dt.options)) == {
        "nthreads",
        "use_semaphore",
        "debug",
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
        "new",
        "nthreads",
        "over_radix_bits",
        "thread_multiplier",
    }
    assert set(dir(dt.options.display)) == {
        "allow_unicode",
        "head_nrows",
        "interactive",
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
        "parse_dates",
        "parse_times",
    }
    assert set(dir(dt.options.progress)) == {
        "callback",
        "clear_on_success",
        "enabled",
        "min_duration",
        "updates_per_second",
        "allow_interruption",
    }
    assert set(dir(dt.options.debug)) == {
        "enabled",
        "logger",
        "report_args",
        "arg_max_size"
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
    assert "Default value 3 is not of type <class 'str'>" in str(e.value)

    with pytest.raises(ValueError) as e:
        dt.options.register_option(name=".hidden", xtype=int, default=0)
    assert "Invalid option name .hidden" in str(e.value)

    dt.options.register_option(name="gooo", xtype=int, default=3)

    with pytest.raises(ValueError) as e:
        dt.options.register_option(name="gooo", xtype=int, default=4, doc="???")
    assert "Option gooo already registered" in str(e.value)

    with pytest.raises(TypeError) as e:
        dt.options.gooo = 2.5
    assert ("Invalid value for option gooo: expected <class 'int'>, instead "
            "got <class 'float'>"
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
    assert ("Cannot assign a value to group of options tmp2.foo.*"
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




#-------------------------------------------------------------------------------
# .debug options
#-------------------------------------------------------------------------------
class SimpleLogger:
    def __init__(self):
        self.msg = ""

    def debug(self, msg):
        self.msg += msg
        self.msg += "\n"


def test_debug_logger_default_without_report_args(capsys):
    assert dt.options.debug.enabled is False
    assert dt.options.debug.logger is None
    assert dt.options.debug.report_args is False
    with dt.options.debug.context(enabled=True):
        assert dt.options.debug.logger is None
        assert dt.options.debug.enabled is True
        DT = dt.Frame(range(100000))
        out, err = capsys.readouterr()
        assert not err
        assert re.search(r"<Frame#[\da-fA-F]+>.__init__\(\) ", out)
        assert re.search(r"# \d+(?:\.\d+)?(?:[eE][+-]?\d+)? s", out)

        with pytest.raises(TypeError):
            dt.cbind(3)
        out, err = capsys.readouterr()
        assert not err
        assert "datatable.cbind() {" in out
        assert re.search(r"} # \d+(?:\.\d+)?(?:[eE][+-]?\d+)? s \(failed\)", out)


def test_debug_logger_default_with_report_args(capsys):
    assert dt.options.debug.logger is None
    with dt.options.debug.context(enabled=True, report_args=True):
        assert dt.options.debug.logger is None
        assert dt.options.debug.enabled is True
        DT = dt.Frame(range(100000))
        out, err = capsys.readouterr()
        print(out)
        assert not err
        assert re.search(r"<Frame#[\da-fA-F]+>.__init__\(range\(0, 100000\)\)", out)
        assert re.search(r"# \d+(?:\.\d+)?(?:[eE][+-]?\d+)? s", out)

        with pytest.raises(TypeError):
            dt.cbind(3)
        out, err = capsys.readouterr()
        assert not err
        assert "datatable.cbind(3) {" in out
        assert re.search(r"} # \d+(?:\.\d+)?(?:[eE][+-]?\d+)? s \(failed\)", out)


def test_debug_logger_object():
    assert dt.options.debug.logger is None
    logger = SimpleLogger()
    with dt.options.debug.context(logger=logger, enabled=True, report_args=True):
        assert dt.options.debug.logger is logger
        assert dt.options.debug.enabled is True

        DT = dt.rbind([])
        assert "datatable.rbind([]) {" in logger.msg
        assert re.search(r"} # \d+(?:\.\d+)?(?:[eE][+-]?\d+)? s", logger.msg)
        logger.msg = ""

        with pytest.raises(TypeError):
            dt.rbind(4)
        assert "datatable.rbind(4) {" in logger.msg
        assert re.search(r"} # \d+(?:\.\d+)?(?:[eE][+-]?\d+)? s \(failed\)", logger.msg)


def test_debug_logger_invalid_object():
    msg = r"Logger should be an object having a method \.debug\(self, msg\)"
    with pytest.raises(TypeError, match=msg):
        dt.options.debug.logger = "default"

    with pytest.raises(TypeError, match=msg):
        dt.options.debug.logger = False

    class A: pass
    with pytest.raises(TypeError, match=msg):
        dt.options.debug.logger = A()

    class B:
        debug = True

    with pytest.raises(TypeError, match=msg):
        dt.options.debug.logger = B()


def test_debug_arg_max_size():
    logger = SimpleLogger()
    with dt.options.debug.context(logger=logger, enabled=True, report_args=True):
        assert dt.options.debug.arg_max_size == 100
        with dt.options.debug.context(arg_max_size=0):
            assert dt.options.debug.arg_max_size == 10
        with pytest.raises(ValueError):
            dt.options.debug.arg_max_size = -1
        with pytest.raises(TypeError):
            dt.options.debug.arg_max_size = None

        with dt.options.debug.context(arg_max_size=20):
            logger.msg = ""
            DT = dt.Frame(A=["abcdefghij"*100])
            assert ".__init__(A=['abcdefghij...hij']) #" in logger.msg


def test_debug_logger_invalid_option():
    # This test checks that invalid options do not cause a crash
    # when logging is enabled
    with dt.options.debug.context(enabled=True, report_args=True):
        try:
            dt.options.gooo0
            assert False, "Did not raise AttributeError"
        except AttributeError:
            pass


def test_debug_logger_bad_repr():
    # This test checks that logging does not crash if a repr()
    # function throws an error
    class A:
        def __repr__(self):
            raise RuntimeError("Malformed repr")

    with dt.options.debug.context(enabled=True, report_args=True):
        DT = dt.Frame()
        try:
            DT[A()]
        except TypeError:
            pass


def test_debug_logger_no_deadlock():
    # This custom logger invokes datatable functionality, which has
    # the potential of causing deadlocks or deep recursive messages.
    class MyLogger:
        def __init__(self):
            self.frame = dt.Frame(msg=[], stype=str)

        def debug(self, msg):
            self.frame.nrows += 1
            self.frame[-1, 0] = msg

    logger = MyLogger()
    with dt.options.debug.context(enabled=True, logger=logger, report_args=True):
        logger.frame.nrows = 0
        DT = dt.Frame(range(10))
        DT.rbind(DT)
        del DT[::2, :]
        assert logger.frame.nrows == 4
