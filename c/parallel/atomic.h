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
#ifndef dt_PARALLEL_ATOMIC_h
#define dt_PARALLEL_ATOMIC_h
#include <atomic>       // std::atomic, std::memory_order
#include <cstring>      // std::memcpy
#include <type_traits>  // std::is_floating_point, std::enable_if
namespace dt {

template <typename T> struct int_from_real {};
template <> struct int_from_real<float> { using t = int32_t; };
template <> struct int_from_real<double> { using t = int64_t; };



// Note: `std::atomic<T>` has specializations for float and double only since
// C++20, so we need to implement our own class for those.
//
template <typename T, typename = typename std::enable_if<
                      std::is_floating_point<T>::value>::type>
class atomic {
  // T is float or double
  // V is int32_t or int64_t
  using V = typename int_from_real<T>::t;
  static_assert(sizeof(T) == sizeof(V), "Invalid size of int/float");
  static constexpr std::memory_order DEFAULT_MO = std::memory_order_seq_cst;

  private:
    std::atomic<V> v;

  public:
    atomic() noexcept = default;
    atomic(T x) noexcept : v(real_to_int(x)) {}
    atomic& operator=(const atomic&) = delete;
    T operator=(T x) noexcept { v = real_to_int(x); return x; }

    void store(T x, std::memory_order order = DEFAULT_MO) noexcept {
      v.store(real_to_int(x), order);
    }
    T load(std::memory_order order = DEFAULT_MO) const noexcept {
      return int_to_real(v.load(order));
    }
    operator T() const noexcept { return load(); }

    T exchange(T x, std::memory_order order = DEFAULT_MO) noexcept {
      V old = v.exchange(real_to_int(x), order);
      return int_to_real(old);
    }
    bool compare_exchange_weak(T& expected, T desired,
                               std::memory_order success,
                               std::memory_order failure) noexcept {
      V expv = real_to_int(expected);
      V desv = real_to_int(desired);
      bool ret = compare_exchange_weak(expv, desv, success, failure);
      expected = int_to_real(expv);
      return ret;
    }
    bool compare_exchange_weak(T& expected, T desired,
                               std::memory_order order = DEFAULT_MO) noexcept {
      V expv = real_to_int(expected);
      V desv = real_to_int(desired);
      bool ret = compare_exchange_weak(expv, desv, order);
      expected = int_to_real(expv);
      return ret;
    }

    T fetch_add(T arg, std::memory_order order = DEFAULT_MO) noexcept {
      V vcurr = v.load(order);
      while (!v.compare_exchange_weak(vcurr,
                                      real_to_int(int_to_real(vcurr) + arg),
                                      order));
      return int_to_real(vcurr);
    }

    T fetch_sub(T arg, std::memory_order order = DEFAULT_MO) noexcept {
      V vcurr = v.load(order);
      while (!v.compare_exchange_weak(vcurr,
                                      real_to_int(int_to_real(vcurr) - arg),
                                      order));
      return int_to_real(vcurr);
    }

    T fetch_mul(T arg, std::memory_order order = DEFAULT_MO) noexcept {
      V vcurr = v.load(order);
      while (!v.compare_exchange_weak(vcurr,
                                      real_to_int(int_to_real(vcurr) * arg),
                                      order));
      return int_to_real(vcurr);
    }

    T fetch_div(T arg, std::memory_order order = DEFAULT_MO) noexcept {
      V vcurr = v.load(order);
      while (!v.compare_exchange_weak(vcurr,
                                      real_to_int(int_to_real(vcurr) / arg),
                                      order));
      return int_to_real(vcurr);
    }

    T operator+=(T arg) noexcept { return fetch_add(arg) + arg; }
    T operator-=(T arg) noexcept { return fetch_sub(arg) - arg; }
    T operator*=(T arg) noexcept { return fetch_mul(arg) * arg; }
    T operator/=(T arg) noexcept { return fetch_div(arg) / arg; }

  private:
    // The only safe way to convert float<->int (bitwise) is via memcpy, see
    // https://en.cppreference.com/w/cpp/language/reinterpret_cast
    // https://chromium.googlesource.com/chromium/src/+/1587f7d/base/macros.h#76
    static inline V real_to_int(T x) noexcept {
      V res;
      std::memcpy(&res, &x, sizeof(V));
      return res;
    }
    static inline T int_to_real(V y) noexcept {
      T res;
      std::memcpy(&res, &y, sizeof(T));
      return res;
    }
};



//------------------------------------------------------------------------------
// atomic_fetch_min / atomic_fetch_max
//------------------------------------------------------------------------------

template <typename T>
T atomic_fetch_min(std::atomic<T>* obj, T arg) noexcept {
  T curr = obj->load();
  while (curr > arg &&
         !obj->compare_exchange_weak(curr, arg));
  return curr;
}

template <typename T>
T atomic_fetch_max(std::atomic<T>* obj, T arg) noexcept {
  T curr = obj->load();
  while (curr < arg &&
         !obj->compare_exchange_weak(curr, arg));
  return curr;
}



}  // namespace dt
#endif
