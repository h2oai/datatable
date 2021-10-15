//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#include "_dt.h"
#include "column/arrow.h"
#include "column/arrow_bool.h"
#include "column/arrow_fw.h"
#include "column/arrow_str.h"
#include "column/arrow_void.h"
#include "documentation.h"
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "python/xargs.h"
#include "stype.h"
#include "utils/arrow_structs.h"
namespace py {


oobj Frame::to_arrow(const XArgs&) {
  oobj pyarrow = oobj::import("pyarrow");
  oobj pa_Array = pyarrow.get_attr("Array");
  oobj pa_Table = pyarrow.get_attr("Table");
  size_t n = dt->ncols();

  olist arrays(n);
  for (size_t i = 0; i < n; ++i) {
    const Column& col = dt->get_column(i);
    std::unique_ptr<dt::OArrowArray> aarr = col.to_arrow();
    std::unique_ptr<dt::OArrowSchema> osch = col.to_arrow_schema();
    arrays.set(i,
      pa_Array.invoke("_import_from_c", {
          oint(aarr.release()->intptr()),
          oint(osch.release()->intptr())
        }));
  }

  otuple names = dt->get_pynames();
  oobj res = pa_Table.invoke("from_arrays", {arrays, names});

  return res;
}

DECLARE_METHOD(&Frame::to_arrow)
    ->name("to_arrow")
    ->docs(dt::doc_Frame_to_arrow);




}  // namespace py




static void release_arrow_array(dt::ArrowArray* aarr) {
  if (aarr->private_data) {
    // Note: `aarr` may be owned by the ArrowArrayData object, and deleted
    //       when we delete `data`. Therefore, we cannot use pointer `aarr`
    //       after `data` is deleted.
    auto data = reinterpret_cast<dt::ArrowArrayData*>(aarr->private_data);
    aarr->release = nullptr;
    aarr->private_data = nullptr;
    delete data;
  }
}


/**
  * Return OArrowArray structure that describes the current Column
  * from the point of view of the Arrow C interface.
  *
  * The returned object must be self-contained
  */
std::unique_ptr<dt::OArrowArray> Column::to_arrow() const {
  Column arrow_column = impl_->as_arrow();
  xassert(arrow_column);
  std::unique_ptr<dt::ArrowArrayData> data(
      new dt::ArrowArrayData(std::move(arrow_column)));

  auto arrow_impl = dynamic_cast<const dt::Arrow_ColumnImpl*>(data->column().impl_);
  xassert(arrow_impl);
  size_t na_count = arrow_impl->stats()->nacount();
  size_t n_buffers = arrow_impl->get_num_data_buffers();
  size_t n_children = arrow_impl->n_children();
  xassert(n_children == 0);

  std::unique_ptr<dt::OArrowArray> aarr(new dt::OArrowArray());
  (*aarr)->length = static_cast<int64_t>(nrows());
  (*aarr)->null_count = static_cast<int64_t>(na_count);
  (*aarr)->offset = 0;
  (*aarr)->n_buffers = static_cast<int64_t>(n_buffers);
  (*aarr)->n_children = static_cast<int64_t>(n_children);
  if (n_buffers) {
    data->buffers().reserve(n_buffers);
    for (size_t i = 0; i < n_buffers; ++i) {
      const void* buffer_ptr = arrow_impl->get_data_buffer(i).rptr();
      data->buffers().push_back( buffer_ptr );
    }
    (*aarr)->buffers = data->buffers().data();
  }
  (*aarr)->private_data = data.release();
  (*aarr)->release = release_arrow_array;
  aarr->ouroboros();
  return aarr;
}



static void release_arrow_schema(dt::ArrowSchema* schema) {
  delete schema;
}

std::unique_ptr<dt::OArrowSchema> Column::to_arrow_schema() const {
  auto osch = std::unique_ptr<dt::OArrowSchema>(new dt::OArrowSchema());
  // The formats are listed in pyarrow's [CDataInterface / format strings]
  // manual: https://arrow.apache.org/docs/format/CDataInterface.html
  switch (stype()) {
    case dt::SType::VOID:    (*osch)->format = "n"; break;
    case dt::SType::BOOL:    (*osch)->format = "b"; break;
    case dt::SType::INT8:    (*osch)->format = "c"; break;
    case dt::SType::INT16:   (*osch)->format = "s"; break;
    case dt::SType::INT32:   (*osch)->format = "i"; break;
    case dt::SType::INT64:   (*osch)->format = "l"; break;
    case dt::SType::FLOAT32: (*osch)->format = "f"; break;
    case dt::SType::FLOAT64: (*osch)->format = "g"; break;
    case dt::SType::DATE32:  (*osch)->format = "tdD"; break;
    case dt::SType::TIME64:  (*osch)->format = "tsn:"; break;  // timezone after :
    case dt::SType::STR32:   (*osch)->format = "u"; break;
    case dt::SType::STR64:   (*osch)->format = "U"; break;
    case dt::SType::ARR32:   (*osch)->format = "+l"; break;
    case dt::SType::ARR64:   (*osch)->format = "+L"; break;
    default:
      throw NotImplError() << "Cannot convert column of type " << type()
                           << " into arrow";
  }
  (*osch)->flags = ARROW_FLAG_NULLABLE;
  (*osch)->release = release_arrow_schema;
  return osch;
}

bool Column::is_arrow() const {
  return dynamic_cast<const dt::Arrow_ColumnImpl*>(impl_) != nullptr;
}

Column Column::as_arrow() const {
  return impl_->as_arrow();
}


static void _clear_validity_buffer(size_t n, size_t* data) {
  dt::parallel_for_static(n, [=](size_t i){ data[i] = 0; });
}


Column dt::ColumnImpl::_as_arrow_void() const {
  xassert(stype() == SType::VOID);
  size_t bufsize = (nrows_ + 63)/64 * 8;
  Buffer validity_buffer = Buffer::mem(bufsize);
  auto validity_data = static_cast<size_t*>(validity_buffer.xptr());
  _clear_validity_buffer(bufsize/8, validity_data);
  return Column(new ArrowVoid_ColumnImpl(nrows_, std::move(validity_buffer)));
}


Column dt::ColumnImpl::_as_arrow_bool() const {
  xassert(stype() == SType::BOOL);
  // The buffers must be aligned at 8-bytes boundary
  size_t bufsize = (nrows_ + 63)/64 * 8;
  Buffer validity_buffer = Buffer::mem(bufsize);
  Buffer data_buffer = Buffer::mem(bufsize);
  auto validity = static_cast<uint8_t*>(validity_buffer.xptr());
  auto data = static_cast<uint8_t*>(data_buffer.xptr());
  _clear_validity_buffer(bufsize/8, reinterpret_cast<size_t*>(validity));
  _clear_validity_buffer(bufsize/8, reinterpret_cast<size_t*>(data));

  parallel_for_static(nrows_,
    dt::ChunkSize(64),
    dt::NThreads(allow_parallel_access()),
    [=](size_t i) {
      int8_t value;
      bool isvalid = this->get_element(i, &value);
      if (isvalid) {
        validity[i/8] |= 1 << (i&7);
        data[i/8] |= value << (i&7);
      }
    });

  return Column(new ArrowBool_ColumnImpl(
      nrows_, std::move(validity_buffer), std::move(data_buffer))
  );
}


template <typename T>
Column dt::ColumnImpl::_as_arrow_fw() const {
  size_t validity_bufsize = (nrows_ + 63)/64 * 8;
  Buffer validity_buffer = Buffer::mem(validity_bufsize);
  Buffer data_buffer = Buffer::mem(nrows_ * sizeof(T));
  auto validity = static_cast<uint8_t*>(validity_buffer.xptr());
  auto data = static_cast<T*>(data_buffer.xptr());
  _clear_validity_buffer(validity_bufsize/8, reinterpret_cast<size_t*>(validity));

  parallel_for_static(nrows_,
    dt::ChunkSize(64),
    dt::NThreads(allow_parallel_access()),
    [=](size_t i) {
      bool isvalid = this->get_element(i, &data[i]);
      if (isvalid) {
        validity[i/8] |= 1 << (i&7);
      }
    });

  return Column(new ArrowFw_ColumnImpl(
      nrows_, stype(), std::move(validity_buffer), std::move(data_buffer))
  );
}



template <typename T>
Column dt::ColumnImpl::_as_arrow_str() const {
  size_t validity_bufsize = (nrows_ + 63)/64 * 8;
  Buffer validity_buffer = Buffer::mem(validity_bufsize);
  auto validity_data = static_cast<uint8_t*>(validity_buffer.xptr());

  size_t offsets_bufsize = (sizeof(T) == 8)? (nrows_ + 1)*8
                                           : (nrows_ + 2 - (nrows_&1))*4;
  Buffer offsets_buffer = Buffer::mem(offsets_bufsize);
  auto offsets_data = static_cast<T*>(offsets_buffer.xptr());
  *offsets_data++ = 0;

  constexpr size_t chunk_size = 64;  // must be a multiple of 64
  size_t nchunks = (nrows_ + chunk_size - 1)/chunk_size;
  std::vector<Buffer> strdata_chunks(nchunks);
  std::vector<size_t> chunk_sizes(nchunks + 1);

  dt::parallel_for_dynamic(nchunks,
    [&](size_t ichunk){
      size_t i0 = ichunk * chunk_size;
      size_t local_chunk_size = std::min(chunk_size, nrows_ - i0);
      auto local_offsets_data = offsets_data + i0;
      auto local_validity_data = validity_data + (i0 / 8);
      for (size_t k = 0; k < chunk_size / 64; k++) {
        (reinterpret_cast<uint64_t*>(local_validity_data))[k] = 0;
      }
      Buffer strbuffer = Buffer::mem(chunk_size * 8);
      size_t current_offset = 0;
      CString str_value;
      for (size_t j = 0; j < local_chunk_size; j++) {
        bool isvalid = get_element(j + i0, &str_value);
        if (isvalid) {
          local_validity_data[j/8] |= 1 << (j&7);
          strbuffer.ensuresize(current_offset + str_value.size());
          std::memcpy(static_cast<char*>(strbuffer.xptr()) + current_offset,
                      str_value.data(), str_value.size());
          current_offset += str_value.size();
        }
        local_offsets_data[j] = static_cast<T>(current_offset);
      }
      strdata_chunks[ichunk] = std::move(strbuffer);
      chunk_sizes[ichunk] = current_offset;
    });

  size_t total_size = 0;
  for (size_t i = 0; i < nchunks; i++) {
    auto sz = chunk_sizes[i];
    chunk_sizes[i] = total_size;
    total_size += sz;
  }
  chunk_sizes[nchunks] = total_size;
  if (sizeof(T) == 4 && total_size > std::numeric_limits<T>::max()) {
    throw NotImplError() << "Buffer overflow when materializing a string column";
  }

  total_size = (total_size + 7)/8 * 8;
  if (total_size == 0) total_size = 8;
  Buffer strdata_buffer = Buffer::mem(total_size);
  auto strdata = static_cast<char*>(strdata_buffer.xptr());
  dt::parallel_for_dynamic(nchunks,
    [&](size_t ichunk) {
      size_t local_chunk_offset = chunk_sizes[ichunk];
      size_t local_chunk_size = chunk_sizes[ichunk+1] - chunk_sizes[ichunk];
      std::memcpy(strdata + local_chunk_offset,
                  strdata_chunks[ichunk].rptr(),
                  local_chunk_size);
      if (local_chunk_offset > 0) {
        size_t i0 = ichunk * chunk_size;
        size_t i1 = std::min(i0 + chunk_size, nrows_);
        for (size_t i = i0; i < i1; i++) {
          offsets_data[i] += local_chunk_offset;
        }
      }
    }
  );

  return Column(new dt::ArrowStr_ColumnImpl<T>(
      nrows(), stype(), std::move(validity_buffer),
      std::move(offsets_buffer), std::move(strdata_buffer))
  );
}


/**
  * Convert this column into a column whose implementation is
  * Arrow_ColumnImpl.
  */
Column dt::ColumnImpl::as_arrow() const {
  switch (stype()) {
    case SType::VOID:    return _as_arrow_void();
    case SType::BOOL:    return _as_arrow_bool();
    case SType::INT8:    return _as_arrow_fw<int8_t>();
    case SType::INT16:   return _as_arrow_fw<int16_t>();
    case SType::DATE32:
    case SType::INT32:   return _as_arrow_fw<int32_t>();
    case SType::TIME64:
    case SType::INT64:   return _as_arrow_fw<int64_t>();
    case SType::FLOAT32: return _as_arrow_fw<float>();
    case SType::FLOAT64: return _as_arrow_fw<double>();
    case SType::STR32:   return _as_arrow_str<uint32_t>();
    case SType::STR64:   return _as_arrow_str<uint64_t>();
    default: break;
  }
  throw NotImplError() << "Cannot convert column of type "
      << type() << " into arrow";
}
