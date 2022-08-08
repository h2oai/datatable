#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2022 H2O.ai
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
import datatable as dt
import itertools
import os
import pytest
import subprocess
import sys
import time
from datatable.lib import core
from tests import (cpp_test, skip_on_jenkins, get_core_tests)



#-------------------------------------------------------------------------------
# "parallel" tests
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("testname", get_core_tests("parallel"), indirect=True)
def test_core_parallel(testname):
    # Run all core tests in suite "parallel"
    core.run_test("parallel", testname)


def test_multiprocessing_threadpool():
    # Verify that threads work properly after forking (#1758)
    import multiprocessing as mp
    from datatable.internal import get_thread_ids
    parent_threads = get_thread_ids()
    n = 4
    with mp.Pool(processes=n) as pool:
        child_threads = pool.starmap(get_thread_ids, [()] * n, chunksize=1)
        assert len(child_threads) == n
        for chthreads in child_threads:
            assert len(parent_threads) == len(chthreads)
            # After a fork the child may or may not get the same thread IDs
            # as the parent (this is implementation-dependent)
            # assert chthreads != parent_threads


#-------------------------------------------------------------------------------
# "progress" tests
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("testname", get_core_tests("progress"), indirect=True)
def test_core_progress(testname):
    # Run all core tests in suite "progress"
    core.run_test("progress", testname)


@cpp_test
@skip_on_jenkins
@pytest.mark.usefixtures("nowin") # `SIGINT` is not supported on Windows
@pytest.mark.parametrize('parallel_type, nthreads',
                         itertools.product(
                            [None, "static", "nested", "dynamic", "ordered"],
                            ["1", "half", "all"]
                         )
                        )
def test_progress_interrupt(parallel_type, nthreads):
    import signal
    sleep_time = 0.01
    exception = "KeyboardInterrupt\n"
    message = "[cancelled]\x1b[K\n"
    cmd = "import datatable as dt; from datatable.lib import core;"
    cmd += "dt.options.progress.enabled = True;"
    cmd += "dt.options.progress.min_duration = 0;"
    cmd += "print('%s start', flush = True); " % (parallel_type,);

    if parallel_type:
        cmd += "core.run_test('progress', '%s_%s')" % (parallel_type, nthreads)
    else:
        nth = 1 if nthreads == "1" else \
              dt.options.nthreads//2 if nthreads == "half" else \
              dt.options.nthreads
        cmd += "import time; "
        cmd += "dt.options.nthreads = %d; " % nth
        cmd += "time.sleep(%r);" % (sleep_time * 10)

    proc = subprocess.Popen([sys.executable, "-c", cmd],
                            stdout = subprocess.PIPE,
                            stderr = subprocess.PIPE)

    line = proc.stdout.readline()
    assert line.decode() == str(parallel_type) + " start" + os.linesep
    time.sleep(sleep_time);

    # send interrupt signal and make sure the process throws `KeyboardInterrupt`
    proc.send_signal(signal.Signals.SIGINT)
    (stdout, stderr) = proc.communicate()

    stdout_str = stdout.decode()
    stderr_str = stderr.decode()

    is_exception = stderr_str.endswith(exception)
    is_cancelled = stdout_str.endswith(message) if parallel_type else is_exception

    if not is_exception or not is_cancelled:
        print("\ncmd:\n  %s" % cmd)
        print("\nstdout:\n%s" % stdout_str)
        print("\nstderr:\n%s" % stderr_str)

    assert is_cancelled
    assert is_exception

