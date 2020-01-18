#ifndef dt_UTILS_CPLUSES_h
#define dt_UTILS_CPLUSES_h
#include <memory>     // std::unique_ptr
#include <utility>    // std::forward
namespace dt {


// Defined in C++14, but not in C++11
// See https://en.cppreference.com/w/cpp/memory/unique_ptr/make_unique
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}


}
#endif
