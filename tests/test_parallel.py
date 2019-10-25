#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
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
import subprocess
import time
import itertools
import pytest
from datatable.lib import core


#-------------------------------------------------------------------------------
# Check if we need to run C++ tests
#-------------------------------------------------------------------------------

cpp_test = pytest.mark.skipif(not hasattr(core, "test_coverage"),
                              reason="C++ tests were not compiled")

#-------------------------------------------------------------------------------
# Test parallel infrastructure
#-------------------------------------------------------------------------------

def test_multiprocessing_threadpool():
    # Verify that threads work properly after forking (#1758)
    import multiprocessing as mp
    from datatable.internal import get_thread_ids
    parent_threads = get_thread_ids()
    n = 4
    pool = mp.Pool(processes=n)
    child_threads = pool.starmap(get_thread_ids, [()] * n, chunksize=1)
    assert len(child_threads) == n
    for chthreads in child_threads:
        assert len(parent_threads) == len(chthreads)
        assert chthreads != parent_threads


@cpp_test
def test_internal_shared_mutex():
    core.test_shmutex(500, dt.options.nthreads * 2, 1)


@cpp_test
def test_internal_shared_bmutex():
    core.test_shmutex(1000, dt.options.nthreads * 2, 0)


@cpp_test
def test_internal_atomic():
    core.test_atomic()


@cpp_test
def test_internal_barrier():
    core.test_barrier(100)


@cpp_test
def test_internal_parallel_for_static():
    core.test_parallel_for_static(1000)


@cpp_test
def test_internal_parallel_for_dynamic():
    core.test_parallel_for_dynamic(1000)


@cpp_test
def test_internal_parallel_for_ordered1():
    core.test_parallel_for_ordered(1723)


@cpp_test
def test_internal_parallel_for_ordered2():
    n0 = dt.options.nthreads
    try:
        dt.options.nthreads = 2
        core.test_parallel_for_ordered(1723)
    finally:
        dt.options.nthreads = n0


# Make sure C++ tests run cleanly when not interrupted
@cpp_test
@pytest.mark.parametrize('parallel_type, nthreads',
                         itertools.product(
                            ["static", "nested", "dynamic", "ordered"],
                            [1, dt.options.nthreads//2, dt.options.nthreads]
                         )
                        )
def test_progress(parallel_type, nthreads):
    niterations = 1000
    ntimes = 2
    cmd_run = "core.test_progress_%s(%s, %s);" % (
              parallel_type, niterations, nthreads)
    for _ in range(ntimes) :
        exec(cmd_run)


# Send interrupt signal and make sure process throws `KeyboardInterrupt`.
@cpp_test
@pytest.mark.parametrize('parallel_type, nthreads',
                         itertools.product(
                            [None, "static", "nested", "dynamic", "ordered"],
                            [1, dt.options.nthreads//2, dt.options.nthreads]
                         )
                        )
def test_progress_interrupt(parallel_type, nthreads):
    import signal
    # In some cases `core.test_progress_*` may not start immediately
    # after the `print()` statement, that we catch with
    # `proc.stdout.readline()`, in such a case we retry with twice
    # larger `sleep_time`.
    max_tries = 5
    niterations = 10000
    sleep_time = 0.01
    delay_coeff = 1.5
    exception = "KeyboardInterrupt\n"
    cmd = "import datatable as dt; from datatable.lib import core;"
    cmd += "dt.options.progress.enabled = True;"
    cmd += "dt.options.progress.min_duration = 0;"
    cmd += "print('%s start', flush = True); " % parallel_type;

    if parallel_type is None:
        cmd += "import time; "
        cmd += "dt.options.nthreads = %s; " % nthreads
        cmd += "time.sleep(%s);" % (sleep_time * 10 * (delay_coeff ** max_tries))
    else:
        if parallel_type is "ordered":
            niterations //= 10
        cmd += "core.test_progress_%s(%s, %s)" % (
                  parallel_type, niterations, nthreads)

    i = 0
    while i < max_tries:
        proc = subprocess.Popen(["python", "-c", cmd],
                                stdout = subprocess.PIPE,
                                stderr = subprocess.PIPE)

        line = proc.stdout.readline()
        assert line.decode() == str(parallel_type) + " start\n"
        time.sleep(sleep_time);

        proc.send_signal(signal.Signals.SIGINT)
        (stdout, stderr) = proc.communicate()

        is_cancelled = stdout.decode().endswith("[cancelled]\x1b[m\x1b[K\n")
        is_exception = stderr.decode().endswith(exception)
        if is_exception:
            break

        sleep_time *= delay_coeff
        print(sleep_time)
        i += 1

    assert is_cancelled if parallel_type else not is_cancelled
    assert is_exception


