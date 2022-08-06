//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#ifdef DT_TEST
#include <vector>
#include "parallel/api.h"
#include "progress/work.h"      // dt::progress::work
#include "utils/exceptions.h"
#include "utils/tests.h"
namespace dt {
namespace tests {


static void test_progress_static(size_t n, size_t nth) {
  const size_t iteration_size = 1000;
  dt::progress::work job(n);
  job.set_message("Starting test_progress_static...");

  dt::NThreads nthreads = dt::NThreads(nth);
  size_t niterations = n * nthreads.get();
  std::vector<size_t> data(niterations, 0);

  job.set_message("Running test_progress_static...");
  dt::parallel_for_static(niterations, dt::ChunkSize(1), nthreads,
    [&](size_t i) {
      for (size_t j = 0; j < iteration_size; ++j) {
        data[i] += i % (j + 1);
      }
      if (dt::this_thread_index() == 0) {
        job.add_done_amount(1);
      }
    });

  job.set_message("Finishing test_progress_static...");
  job.done();
}

TEST(progress, static_1) {
  test_progress_static(10000, 1);
}

TEST(progress, static_half) {
  test_progress_static(10000, dt::num_threads_in_pool()/2);
}

TEST(progress, static_all) {
  test_progress_static(10000, dt::num_threads_in_pool());
}



static void test_progress_nested(size_t n, size_t nth) {
  const size_t iteration_size = 1000;
  const size_t nloops = 2;
  dt::progress::work job(nloops * n);
  job.set_message("Starting test_progress_nested...");

  dt::NThreads nthreads = dt::NThreads(nth);
  size_t niterations= n * nthreads.get();
  std::vector<size_t> data(niterations, 0);

  dt::parallel_region(nthreads,
    [&]() {
      if (dt::this_thread_index() == 0) {
        job.set_message("Running test_progress_nested...");
      }
      for (size_t l = 0; l < nloops; ++l) {
        dt::nested_for_static(niterations, dt::ChunkSize(1),
          [&](size_t i) {
            for (size_t j = 0; j < iteration_size; ++j) {
              data[i] += i % (j + 1);
            }
            if (dt::this_thread_index() == 0) {
              job.add_done_amount(1);
            }
          });
      }
      dt::barrier();
    });

  job.set_message("Finishing test_progress_nested...");
  job.done();
}

TEST(progress, nested_1) {
  test_progress_nested(10000, 1);
}

TEST(progress, nested_half) {
  test_progress_nested(10000, dt::num_threads_in_pool()/2);
}

TEST(progress, nested_all) {
  test_progress_nested(10000, dt::num_threads_in_pool());
}



static void test_progress_dynamic(size_t n, size_t nth) {
  const size_t iteration_size = 1000;
  dt::progress::work job(n);
  job.set_message("Starting test_progress_dynamic...");

  dt::NThreads nthreads = dt::NThreads(nth);
  size_t niterations= n * nthreads.get();
  std::vector<size_t> data(niterations, 0);

  job.set_message("Running test_progress_dynamic...");
  dt::parallel_for_dynamic(niterations, nthreads,
    [&](size_t i) {
      for (size_t j = 0; j < iteration_size; ++j) {
        data[i] += i % (j + 1);
      }

      if (dt::this_thread_index() == 0) {
        job.set_done_amount(i / nthreads.get());
      }
    });

  job.set_done_amount(n);
  job.set_message("Finishing test_progress_dynamic...");
  job.done();
}

TEST(progress, dynamic_1) {
  test_progress_dynamic(10000, 1);
}

TEST(progress, dynamic_half) {
  test_progress_dynamic(10000, dt::num_threads_in_pool()/2);
}

TEST(progress, dynamic_all) {
  test_progress_dynamic(10000, dt::num_threads_in_pool());
}



static void test_progress_ordered(size_t n, size_t nth) {
  dt::NThreads nthreads = dt::NThreads(nth);
  size_t niterations= n * nthreads.get();
  std::vector<size_t> data(niterations, 0);

  class OTask : public dt::OrderedTask {
    private:
      sztvec& data_;
      size_t iteration_size_;

    public:
      OTask(sztvec& data)
        : data_(data),
          iteration_size_(1000) {}

      void start(size_t i) override {
        for (size_t j = 0; j < iteration_size_; ++j) {
          data_[i] += i % (j + 1) + 1;
        }
      }

      void order(size_t i) override {
        for (size_t j = 0; j < iteration_size_; ++j) {
          data_[i] += i % (j + 1) + 2;
        }
      }

      void finish(size_t i) override {
        for (size_t j = 0; j < iteration_size_; ++j) {
          data_[i] += i % (j + 1) + 3;
        }
      }
  };

  dt::parallel_for_ordered(
    niterations, nthreads,
    [&] { return std::make_unique<OTask>(data); });
}

TEST(progress, ordered_1) {
  test_progress_ordered(1000, 1);
}

TEST(progress, ordered_half) {
  test_progress_ordered(1000, dt::num_threads_in_pool()/2);
}

TEST(progress, ordered_all) {
  test_progress_ordered(1000, dt::num_threads_in_pool());
}



}} // namespace dt::tests
#endif
