//------------------------------------------------------------------------------
// This code is borrowed from
// https://github.com/llvm-mirror/llvm/blob/master/include/llvm/ADT/STLExtras.h
// with minor modifications. The original code was bearing the following
// copyright/license note:
//
//===- llvm/ADT/STLExtras.h - Useful STL related functions ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef dt_UTILS_FUNCTION_h
#define dt_UTILS_FUNCTION_h
#include <type_traits>  // std::is_same, std::enable_if, std::remove_reference
#include <utility>      // std::forward
namespace dt {


/**
 * An efficient, type-erasing, non-owning reference to a callable. This is
 * intended for use as the type of a function parameter that is not used
 * after the function in question returns.
 *
 * This class does not own the callable, so it is not in general safe to store
 * a dt::function.
 *
 * In particular, beware of the following use pattern:
 *
 *     function<void()> f = [&]{ ... };
 *
 * Here the RHS is a lambda, which is created as a temporary variable. It is
 * then assigned to `f`, at which point the temporary gets destroyed and its
 * storage reclaimed. However, since `f` is a non-owning wrapper, any
 * subsequent call to `f` will be invoking a dangling pointer, which may
 * cause memory corruption and crashes.
 */
template<typename Fn> class function;


template<typename TReturn, typename ...TArgs>
class function<TReturn(TArgs...)>
{
  private:
    using fptr = void*;
    using callback_t = TReturn (*)(fptr, TArgs...);

    callback_t _callback = nullptr;
    fptr _callable = nullptr;

    template<typename F>
    static TReturn callback_fn(fptr callable, TArgs ...params) {
      return (*reinterpret_cast<F*>(callable))(
          std::forward<TArgs>(params)...);
    }

  public:
    function() = default;
    function(std::nullptr_t) {}
    function(const function&) = default;
    function& operator=(const function&) = default;

    template <typename F, typename = typename std::enable_if<
                     !std::is_same<typename std::remove_reference<F>::type,
                                   function>::value>::type>
    function(F&& callable) noexcept
        : _callback(callback_fn<typename std::remove_reference<F>::type>),
          _callable(reinterpret_cast<fptr>(&callable)) {}

    TReturn operator()(TArgs ...params) const {
      return _callback(_callable, std::forward<TArgs>(params)...);
    }

    operator bool() const { return bool(_callback); }
};


}  // namespace dt
#endif
