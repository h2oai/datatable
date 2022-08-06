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
#include <atomic>       // std::atomic
#include <cstdlib>      // std::rand, RAND_MAX
#include <ctime>        // std::time
#include <future>       // std::future, std::packaged_task
#include <vector>       // std::vector
#include <thread>
#include "parallel/api.h"
#include "parallel/shared_mutex.h"
#include "utils/exceptions.h"
#include "utils/tests.h"
namespace dt {
namespace tests {


// Helper class to ensure the thread pool is joined in the end, even if an
// exception occurs.
class joining_threads {
  private:
    std::vector<std::thread>& threads;
  public:
    explicit joining_threads(std::vector<std::thread>& th_) : threads(th_) {}
    ~joining_threads() {
      for (auto& thread : threads) {
        thread.join();
      }
    }
};


// Task to be executed in each thread
template <typename MUTEX>
static int _thread_task(MUTEX* shmutex, std::atomic<int>* barrier,
                        size_t n_iters, int8_t* exclusives, int* data)
{
  barrier->fetch_sub(1);
  while (barrier->load());  // wait for all threads to spawn
  for (size_t i = 0; i < n_iters; ++i) {
    if (exclusives[i]) {
      dt::shared_lock<MUTEX> lock(*shmutex, true);
      data[0]++;
      data[1]++;
      data[2]++;
    }
    else {
      dt::shared_lock<MUTEX> lock(*shmutex, false);
      int x = data[0];
      int y = data[1];
      int z = data[2];
      ASSERT_EQ(y, x + 2);
      ASSERT_EQ(z, x + 4);
    }
  }
  return 0;
}


// Spawn a thread pool, execute `_thread_task` in each thread, and collect
// the results (in our case, the "result" is presence/absence of an exception).
template <typename MUTEX>
static void _execute(size_t n_iters, size_t n_threads, int8_t* exclusives,
                     int* data)
{
  using task_t = int(MUTEX*, std::atomic<int>*, size_t, int8_t*, int*);
  MUTEX shmutex;
  std::atomic<int> barrier;

  std::vector<std::future<int>> futures;
  std::vector<std::thread> threads;
  joining_threads pool(threads);

  futures.reserve(n_threads);
  threads.reserve(n_threads);
  barrier = static_cast<int>(n_threads);
  for (size_t j = 0; j < n_threads; ++j) {
    std::packaged_task<task_t> task(_thread_task<MUTEX>);
    std::future<int> future = task.get_future();
    std::thread thread(std::move(task),
                       &shmutex, &barrier, n_iters, exclusives + j*n_iters, data);
    threads.push_back(std::move(thread));
    futures.push_back(std::move(future));
  }
  for (auto& f : futures) {
    f.get();  // if any threads throws an error, it will be re-thrown here
  }
}


static void test_shmutex(size_t n_iters, size_t n_threads, int impl) {
  std::srand(static_cast<unsigned int>(std::time(nullptr)));
  const size_t ntotal = n_iters * n_threads;
  const int threshold = RAND_MAX / 10;

  int n = 0;
  std::vector<int8_t> excl;
  excl.reserve(ntotal);
  for (size_t i = 0; i < ntotal; ++i) {
    bool go_exclusive = std::rand() < threshold;
    n += go_exclusive;
    excl.push_back(go_exclusive);
  }
  int data[3] = {0, 2, 4};

  if (impl == 0) {
    _execute<dt::shared_bmutex>(n_iters, n_threads, excl.data(), data);
  } else {
    _execute<dt::shared_mutex>(n_iters, n_threads, excl.data(), data);
  }

  int x = data[0];
  int y = data[1];
  int z = data[2];
  ASSERT_EQ(x, n);
  ASSERT_EQ(y, n + 2);
  ASSERT_EQ(z, n + 4);
}


TEST(parallel, shared_mutex) {
  test_shmutex(500, dt::num_threads_in_pool() * 2, 1);
}

TEST(parallel, shared_bmutex) {
  test_shmutex(1000, dt::num_threads_in_pool() * 2, 0);
}



}} // namespace dt::tests
#endif
