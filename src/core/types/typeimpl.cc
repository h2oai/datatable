//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#include "column.h"
#include "python/obj.h"
#include "types/typeimpl.h"
#include "stype.h"
namespace dt {


TypeImpl::TypeImpl(SType stype)
  : stype_(stype),
    refcount_(1) {}

TypeImpl::~TypeImpl() {}



void TypeImpl::acquire() noexcept {
  refcount_++;
}

void TypeImpl::release() noexcept {
  refcount_--;
  if (refcount_ == 0) delete this;
}

size_t TypeImpl::hash() const noexcept {
  return static_cast<size_t>(stype_);
}

bool TypeImpl::is_array()    const { return false; }
bool TypeImpl::is_boolean()  const { return false; }
bool TypeImpl::is_compound() const { return false; }
bool TypeImpl::is_float()    const { return false; }
bool TypeImpl::is_integer()  const { return false; }
bool TypeImpl::is_invalid()  const { return false; }
bool TypeImpl::is_numeric()  const { return false; }
bool TypeImpl::is_object()   const { return false; }
bool TypeImpl::is_string()   const { return false; }
bool TypeImpl::is_temporal() const { return false; }
bool TypeImpl::is_void()     const { return false; }


bool TypeImpl::can_be_read_as_int8()     const { return false; }
bool TypeImpl::can_be_read_as_int16()    const { return false; }
bool TypeImpl::can_be_read_as_int32()    const { return false; }
bool TypeImpl::can_be_read_as_int64()    const { return false; }
bool TypeImpl::can_be_read_as_float32()  const { return false; }
bool TypeImpl::can_be_read_as_float64()  const { return false; }
bool TypeImpl::can_be_read_as_date()     const { return false; }
bool TypeImpl::can_be_read_as_cstring()  const { return false; }
bool TypeImpl::can_be_read_as_pyobject() const { return false; }
bool TypeImpl::can_be_read_as_column()   const { return false; }

bool TypeImpl::equals(const TypeImpl* other) const {
  return stype_ == other->stype_;
}


py::oobj TypeImpl::min() const { return py::None(); }
py::oobj TypeImpl::max() const { return py::None(); }
const char* TypeImpl::struct_format() const { return ""; }


Column TypeImpl::cast_column(Column&&) const {
  throw NotImplError()
      << "Type casts for type `" << to_string()
      << "` are not implemented";
}




}  // namespace dt
