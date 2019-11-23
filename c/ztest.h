//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#ifndef dt_ZTEST_h
#define dt_ZTEST_h
#ifdef DTTEST
#include <functional>
#include <string>

namespace dttest {

void test_assert(const std::function<void(void)>&, const std::string&);

void cover_init_FrameInitializationManager_em();
void cover_names_FrameNameProviders();
void cover_names_integrity_checks();

// Defined in parallel/ztest_atomic.cc
void test_atomic();

// Defined in parallel/ztest_barrier.cc
void test_barrier(size_t);

// Defined in parallel/ztest_parallel_for.cc
void test_parallel_for_static(size_t);
void test_parallel_for_dynamic(size_t);
void test_parallel_for_dynamic_nested(size_t);
void test_parallel_for_ordered(size_t);

// Defined in parallel/ztest_shared_mutex.cc
void test_shmutex(size_t n_iters, size_t n_threads, int impl);

// Defined in progress/ztest_progress.cc
void test_progress_static(size_t, size_t);
void test_progress_nested(size_t, size_t);
void test_progress_dynamic(size_t, size_t);
void test_progress_ordered(size_t, size_t);

}  // namespace dttest

#endif
#endif
