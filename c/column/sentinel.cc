//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#include "column/sentinel.h"
#include "column/sentinel_fw.h"
#include "column/sentinel_str.h"
namespace dt {



Column Sentinel_ColumnImpl::make_column(size_t nrows, SType stype) {
  switch (stype) {
    case SType::BOOL:    return Column(new SentinelBool_ColumnImpl(nrows));
    case SType::INT8:    return Column(new SentinelFw_ColumnImpl<int8_t>(nrows));
    case SType::INT16:   return Column(new SentinelFw_ColumnImpl<int16_t>(nrows));
    case SType::INT32:   return Column(new SentinelFw_ColumnImpl<int32_t>(nrows));
    case SType::INT64:   return Column(new SentinelFw_ColumnImpl<int64_t>(nrows));
    case SType::FLOAT32: return Column(new SentinelFw_ColumnImpl<float>(nrows));
    case SType::FLOAT64: return Column(new SentinelFw_ColumnImpl<double>(nrows));
    case SType::STR32:   return Column(new SentinelStr_ColumnImpl<uint32_t>(nrows));
    case SType::STR64:   return Column(new SentinelStr_ColumnImpl<uint64_t>(nrows));
    case SType::OBJ:     return Column(new SentinelObj_ColumnImpl(nrows));
    default:
      throw ValueError()
          << "Unable to create a column of stype `" << stype << "`";
  }
}


Column Sentinel_ColumnImpl::make_fw_column(
    size_t nrows, SType stype, Buffer&& buf)
{
  xassert(buf.size() >= nrows * info(stype).elemsize());
  switch (stype) {
    case SType::BOOL:    return Column(new SentinelBool_ColumnImpl(nrows, std::move(buf)));
    case SType::INT8:    return Column(new SentinelFw_ColumnImpl<int8_t>(nrows, std::move(buf)));
    case SType::INT16:   return Column(new SentinelFw_ColumnImpl<int16_t>(nrows, std::move(buf)));
    case SType::INT32:   return Column(new SentinelFw_ColumnImpl<int32_t>(nrows, std::move(buf)));
    case SType::INT64:   return Column(new SentinelFw_ColumnImpl<int64_t>(nrows, std::move(buf)));
    case SType::FLOAT32: return Column(new SentinelFw_ColumnImpl<float>(nrows, std::move(buf)));
    case SType::FLOAT64: return Column(new SentinelFw_ColumnImpl<double>(nrows, std::move(buf)));
    case SType::OBJ:     return Column(new SentinelObj_ColumnImpl(nrows, std::move(buf)));
    default:
      throw ValueError()
          << "Unable to create a column of stype `" << stype << "`";
  }
}



static Buffer _recode_offsets_to_u64(const Buffer& offsets) {
  // TODO: make this parallel
  Buffer off64 = Buffer::mem(offsets.size() * 2);
  auto data64 = static_cast<uint64_t*>(off64.xptr());
  auto data32 = static_cast<const uint32_t*>(offsets.rptr());
  data64[0] = 0;
  uint64_t curr_offset = 0;
  size_t n = offsets.size() / sizeof(uint32_t) - 1;
  for (size_t i = 1; i <= n; ++i) {
    uint32_t len = data32[i] - data32[i - 1];
    if (len == GETNA<uint32_t>()) {
      data64[i] = curr_offset ^ GETNA<uint64_t>();
    } else {
      curr_offset += len & ~GETNA<uint32_t>();
      data64[i] = curr_offset;
    }
  }
  return off64;
}


Column Sentinel_ColumnImpl::make_str_column(
    size_t nrows, Buffer&& offsets, Buffer&& strdata)
{
  size_t data_size = offsets.size();
  size_t strb_size = strdata.size();

  if (data_size == sizeof(uint32_t) * (nrows + 1)) {
    if (strb_size <= Column::MAX_ARR32_SIZE &&
        nrows <= Column::MAX_ARR32_SIZE) {
      return Column(new SentinelStr_ColumnImpl<uint32_t>(
                      nrows, std::move(offsets), std::move(strdata)));
    }
    // Otherwise, offsets need to be recoded into a uint64_t array
    offsets = _recode_offsets_to_u64(offsets);
  }
  return Column(
      new SentinelStr_ColumnImpl<uint64_t>(
                      nrows, std::move(offsets), std::move(strdata))
  );
}


Sentinel_ColumnImpl::Sentinel_ColumnImpl(size_t nrows, SType stype)
  : ColumnImpl(nrows, stype) {}


bool Sentinel_ColumnImpl::is_virtual() const noexcept {
  return false;
}

NaStorage Sentinel_ColumnImpl::get_na_storage_method() const noexcept {
  return NaStorage::SENTINEL;
}





}  // namespace dt
