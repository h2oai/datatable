//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include <algorithm>         // std::min
#include <cstring>           // std::strncmp, std::strlen
#include <string>            // std::string
#include "buffer.h"
#include "cstring.h"
#include "utils/assert.h"    // xassert
namespace dt {



//------------------------------------------------------------------------------
// CString constructors
//------------------------------------------------------------------------------

CString::CString()
  : ptr_(nullptr), size_(0) {}


CString::CString(const CString& other)
  : ptr_(other.ptr_),
    size_(other.size_),
    buffer_(other.buffer_) {}

CString::CString(CString&& other) noexcept
  : ptr_(other.ptr_),
    size_(other.size_),
    buffer_(std::move(other.buffer_)) {}

CString::CString(const char* ptr, size_t sz)
  : ptr_(ptr), size_(sz) {}

CString::CString(const std::string& str)
  : ptr_(str.data()), size_(str.size()) {}

CString CString::from_null_terminated_string(const char* cstr) {
  return CString(cstr, std::strlen(cstr));
}


CString& CString::operator=(CString&& other) {
  ptr_ = other.ptr_;
  size_ = other.size_;
  buffer_ = std::move(other.buffer_);
  return *this;
}

CString& CString::operator=(const std::string& str) {
  ptr_ = str.data();
  size_ = str.size();
  return *this;
}



void CString::set(const char* ptr, size_t size) {
  ptr_ = ptr;
  size_ = size;
}

void CString::set_na() {
  ptr_ = nullptr;
  size_ = 0;
}

void CString::set_data(const char* ptr) {
  ptr_ = ptr;
}

void CString::set_size(size_t sz) {
  size_ = sz;
}



//------------------------------------------------------------------------------
// CString operators
//------------------------------------------------------------------------------

bool CString::operator==(const CString& other) const noexcept {
  if (ptr_ == other.ptr_) {
    return (size_ == other.size_) || (ptr_ == nullptr);
  } else {
    return (size_ == other.size_) && ptr_ && other.ptr_ &&
           (std::strncmp(ptr_, other.ptr_, size_) == 0);
  }
}


bool CString::operator<(const CString& other) const noexcept {
  if (ptr_ == other.ptr_ && size_ == other.size_) {  // strings are equal
    return false;
  }
  size_t cmpsize = std::min(size_, other.size_);
  int comparison = std::strncmp(ptr_, other.ptr_, cmpsize);
  return comparison? (comparison < 0)
                   : (size_ < other.size_);
}


bool CString::operator>(const CString& other) const noexcept {
  if (ptr_ == other.ptr_ && size_ == other.size_) {  // strings are equal
    return false;
  }
  size_t cmpsize = std::min(size_, other.size_);
  int comparison = std::strncmp(ptr_, other.ptr_, cmpsize);
  return comparison? (comparison > 0)
                   : (size_ > other.size_);
}


bool CString::operator<=(const CString& other) const noexcept {
  return !(*this > other);
}


bool CString::operator>=(const CString& other) const noexcept {
  return !(*this < other);
}


char CString::operator[](size_t i) const {
  xassert(ptr_ && i < size_);
  return ptr_[i];
}




//------------------------------------------------------------------------------
// Miscellaneous
//------------------------------------------------------------------------------

bool CString::isna() const noexcept {
  return (ptr_ == nullptr);
}

size_t CString::size() const noexcept {
  return size_;
}

const char* CString::data() const noexcept {
  return ptr_;
}

const char* CString::end() const noexcept {
  return ptr_ + size_;
}


std::string CString::to_string() const {
  return ptr_? std::string(ptr_, size_) : std::string();
}



//------------------------------------------------------------------------------
// Internal buffer functions
//------------------------------------------------------------------------------

// Prepare buffer for writing `new_size` bytes. The data pointer
// `ptr_` will be set to the start of the buffer, and string size set
// to `new_size`. The pointer `ptr_` is returned to the user, and it
// is valid for writing at most `new_size` bytes of data.
//
char* CString::prepare_buffer(size_t new_size) {
  size_t old_size = buffer_.size();
  if (new_size) {
    if (old_size < new_size) {
      buffer_.resize(new_size, false);
    }
    ptr_ = static_cast<const char*>(buffer_.xptr());
  } else {
    // make sure ptr_ is a non-null pointer
    ptr_ = reinterpret_cast<const char*>(this);
  }
  size_ = new_size;
  return const_cast<char*>(ptr_);
}




}  // namespace dt
