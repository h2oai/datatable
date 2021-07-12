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
#include "column/cast.h"
#include "csv/toa.h"
#include "read/parsers/info.h"
#include "stype.h"
namespace dt {



//------------------------------------------------------------------------------
// CastTime64ToString_ColumnImpl
//------------------------------------------------------------------------------

CastTime64ToString_ColumnImpl::CastTime64ToString_ColumnImpl(SType st, Column&& arg)
  : Cast_ColumnImpl(st, std::move(arg))
{
  xassert(arg_.can_be_read_as<int64_t>());
}


ColumnImpl* CastTime64ToString_ColumnImpl::clone() const {
  return new CastTime64ToString_ColumnImpl(stype(), Column(arg_));
}


bool CastTime64ToString_ColumnImpl::get_element(size_t i, CString* out) const {
  int64_t value;
  bool isvalid = arg_.get_element(i, &value);
  if (isvalid) {
    char* ch = out->prepare_buffer(29);
    char* ch0 = ch;
    time64_toa(&ch, value);
    out->set_size(static_cast<size_t>(ch - ch0));
  }
  return isvalid;
}




}  // namespace dt
