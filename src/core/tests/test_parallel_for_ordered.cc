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
#ifdef DTTEST
#include <atomic>
#include <vector>
#include "parallel/api.h"
#include "parallel/thread_pool.h"
#include "utils/exceptions.h"
#include "utils/tests.h"
namespace dttest {


//------------------------------------------------------------------------------
// Simple ordered loop
//------------------------------------------------------------------------------

static void ordered_simple(size_t niters, size_t nthreads)
{
  std::atomic_flag global = ATOMIC_FLAG_INIT;
  std::vector<int> done(niters, 0);
  std::atomic<size_t> next_ordered {0};

  class OTask : public dt::OrderedTask {
    private:
      std::atomic_flag& executing_global_;
      std::vector<int>& done_;
      std::atomic<size_t>& next_ordered_;
      std::atomic_flag executing_local_ = ATOMIC_FLAG_INIT;
      size_t : 56;

    public:
      OTask(std::atomic_flag& global,
            std::atomic<size_t>& next_ordered, std::vector<int>& done)
        : executing_global_(global),
          done_(done),
          next_ordered_(next_ordered) {}

      void start(size_t j) override {
        ASSERT_FALSE(executing_local_.test_and_set());
        ASSERT_LT(j, done_.size());
        ASSERT_EQ(done_[j], 0);
        done_[j] = 1;
        executing_local_.clear();
      }

      void order(size_t j) override {
        ASSERT_FALSE(executing_global_.test_and_set());
        ASSERT_FALSE(executing_local_.test_and_set());
        ASSERT_EQ(next_ordered_.load(), j);
        next_ordered_++;
        ASSERT_EQ(done_[j], 1);
        done_[j] = 2;
        executing_local_.clear();
        executing_global_.clear();
      }

      void finish(size_t j) override {
        ASSERT_FALSE(executing_local_.test_and_set());
        ASSERT_EQ(done_[j], 2);
        done_[j] = 3;
        executing_local_.clear();
      }
  };

  dt::parallel_for_ordered(niters, dt::NThreads(nthreads),
    [&]{
      return std::make_unique<OTask>(global, next_ordered, done);
    });

  ASSERT_EQ(next_ordered.load(), niters);
  for (size_t i = 0; i < niters; ++i) {
    ASSERT_EQ(done[i], 3);
  }
}


TEST(parallel, for_ordered_all) {
  ordered_simple(1723, 0);
}

TEST(parallel, for_ordered_2) {
  ordered_simple(1723, 2);
}



//------------------------------------------------------------------------------
// Throw exceptions during ordered loop
//------------------------------------------------------------------------------

// Try throwing exceptions from all threads at the same time
TEST(parallel, for_ordered_except_startall) {
  constexpr size_t niters = 1111;

  class OTask : public dt::OrderedTask {
    public:
      void start(size_t i) override {
        std::this_thread::yield();
        throw RuntimeError() << "Exception in iteration " << i;
      }
  };

  ASSERT_THROWS([]{
    dt::parallel_for_ordered(niters, []{ return std::make_unique<OTask>(); });
  }, RuntimeError);
}


// Throw exception at iterations xstart/xorder/xfinish in the
// corresponding code sections
static void except1(size_t niters, size_t xstart, size_t xorder, size_t xfinish) {
  class OTask : public dt::OrderedTask {
    private:
      size_t xstart_, xorder_, xfinish_;
    public:
      OTask(size_t xstart, size_t xorder, size_t xfinish)
        : xstart_(xstart), xorder_(xorder), xfinish_(xfinish) {}

      void start(size_t i) override {
        if (i == xstart_) throw RuntimeError() << "Exception in [start]";
      }

      void order(size_t i) override {
        if (i == xorder_) throw RuntimeError() << "Exception in [order]";
      }

      void finish(size_t i) override {
        if (i == xfinish_) throw RuntimeError() << "Exception in [finish]";
      }
  };

  dt::parallel_for_ordered(niters, [=]{
    return std::make_unique<OTask>(xstart, xorder, xfinish);
  });
}

TEST(parallel, for_ordered_except_start) {
  ASSERT_THROWS([]{
    except1(8645, 123, size_t(-1), size_t(-1));
  }, RuntimeError, "Exception in [start]");
}

TEST(parallel, for_ordered_except_order) {
  ASSERT_THROWS([]{
    except1(8645, size_t(-1), 456, size_t(-1));
  }, RuntimeError, "Exception in [order]");
}

TEST(parallel, for_ordered_except_finish) {
  ASSERT_THROWS([]{
    except1(8645, size_t(-1), size_t(-1), 789);
  }, RuntimeError, "Exception in [finish]");
}

// Throw an exception on the very last iteration
TEST(parallel, for_ordered_except_last_step) {
  ASSERT_THROWS([]{
    except1(8645, size_t(-1), size_t(-1), 8644);
  }, RuntimeError, "Exception in [finish]");
}

TEST(parallel, for_ordered_except_same_step) {
  ASSERT_THROWS([]{
    except1(8645, 17, 17, 17);
  }, RuntimeError, "Exception in [start]");
}




//------------------------------------------------------------------------------
// set_num_iterations
//------------------------------------------------------------------------------

class OTask_SetNumIters : public dt::OrderedTask {
  private:
    std::vector<size_t>& status_;
    size_t stop_at_;

  public:
    OTask_SetNumIters(std::vector<size_t>& status, size_t stop_at)
      : status_(status), stop_at_(stop_at) {}

    void start(size_t i) override {
      status_[i] = 1;
    }
    void order(size_t i) override {
      status_[i] = 2;
      if (i == stop_at_ - 1) {
        set_num_iterations(stop_at_);
      }
    }
    void finish(size_t i) override {
      status_[i] = 3;
    }
};


TEST(parallel, for_ordered_setnumiters_all) {
  constexpr size_t N_ITERS = 1000;
  constexpr size_t STOPAT = 123;
  std::vector<size_t> status(N_ITERS);

  dt::parallel_for_ordered(N_ITERS, [&]{
    return std::make_unique<OTask_SetNumIters>(status, STOPAT);
  });

  size_t i = 0;
  for (; i < STOPAT; ++i) {
    ASSERT_EQ(status[i], size_t(3));
  }
  for (; i < STOPAT + dt::num_threads_in_pool()*3/2; ++i) {
    ASSERT_LE(status[i], size_t(1));
  }
  for (; i < N_ITERS; ++i) {
    ASSERT_EQ(status[i], size_t(0));
  }
}


TEST(parallel, for_ordered_setnumiters_1) {
  constexpr size_t N_ITERS = 1000;
  constexpr size_t STOPAT = 123;
  std::vector<size_t> status(N_ITERS);

  dt::parallel_for_ordered(N_ITERS, dt::NThreads(1), [&]{
    return std::make_unique<OTask_SetNumIters>(status, STOPAT);
  });

  size_t i = 0;
  for (; i < N_ITERS; ++i) {
    ASSERT_EQ(status[i], i < STOPAT? size_t(3) : size_t(0));
  }
}




//------------------------------------------------------------------------------
// wait_until_all_finalized
//------------------------------------------------------------------------------

TEST(parallel, for_ordered_wait_until_all_finalized) {
  constexpr size_t N_ITERS = 1000;
  constexpr size_t TEST_1  = 200;
  constexpr size_t TEST_0  = 160;
  std::vector<size_t> status(N_ITERS);

  class OTask : public dt::OrderedTask {
    private:
      std::vector<size_t>& status_;
    public:
      OTask(std::vector<size_t>& status)
        : status_(status) {}

      void start(size_t i) override {
        status_[i] = 1;
      }
      void order(size_t i) override {
        status_[i] = 2;
        if (i == TEST_1) {
          wait_until_all_finalized();
          // set status[] to 5 for iterations [TEST_0; TEST_1]
          for (size_t j = i; j >= TEST_0; --j) {
            status_[j] = 5;
          }
        }
      }
      void finish(size_t i) override {
        status_[i] = 3;
      }
  };

  dt::parallel_for_ordered(N_ITERS, [&]{
    return std::make_unique<OTask>(status);
  });

  for (size_t i = 0; i < N_ITERS; ++i) {
    // Note: although iteration TEST_1's status was set to 5, that
    // iteration was later finished, setting its status back to 3.
    size_t exp = (i < TEST_1 && i >= TEST_0)? 5 : 3;
    ASSERT_EQ(status[i], exp);
  }
}




}  // namespace dttest
#endif
