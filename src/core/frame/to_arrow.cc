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
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "stype.h"
#include "utils/arrow_structs.h"
namespace py {


static const char* doc_to_arrow =
R"(to_arrow(self)
--

Convert this frame into an arrow ``Table``.

Parameters
----------
return: pyarrow.Table

except: ImportError
    If the `pyarrow` module is not installed.
)";

static PKArgs args_to_arrow(
    0, 0, 0, false, false, {}, "to_arrow", doc_to_arrow);


oobj Frame::to_arrow(const PKArgs&) {
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
          oint(aarr->intptr()),
          oint(osch->intptr())
        }));
    // At this point ownership of the ArrowArray* struct produced by
    // the call col.to_arrow() belongs to the pa.Array object in the
    // list `arrays`. We therefore need to release the unique_ptr so
    // that the object wouldn't be deleted prematurely.
    aarr.release()->ouroboros();
    osch.release();
  }

  otuple names = dt->get_pynames();
  oobj res = pa_Table.invoke("from_arrays", {arrays, names});

  return res;
}





void Frame::_init_toarrow(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::to_arrow, args_to_arrow));
}



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
  size_t n_buffers = arrow_impl->num_buffers();
  size_t n_children = arrow_impl->num_children();
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
      const void* buffer_ptr = arrow_impl->get_buffer(i);
      data->buffers().push_back( buffer_ptr );
    }
    (*aarr)->buffers = data->buffers().data();
  }
  (*aarr)->private_data = data.release();
  (*aarr)->release = release_arrow_array;

  return aarr;
}



static void release_arrow_schema(dt::ArrowSchema* schema) {
  delete schema;
}

std::unique_ptr<dt::OArrowSchema> Column::to_arrow_schema() const {
  auto osch = std::unique_ptr<dt::OArrowSchema>(new dt::OArrowSchema());
  switch (stype()) {
    case dt::SType::VOID:    (*osch)->format = "n"; break;
    case dt::SType::BOOL:    (*osch)->format = "b"; break;
    case dt::SType::INT8:    (*osch)->format = "c"; break;
    case dt::SType::INT16:   (*osch)->format = "s"; break;
    case dt::SType::INT32:   (*osch)->format = "i"; break;
    case dt::SType::INT64:   (*osch)->format = "l"; break;
    case dt::SType::FLOAT32: (*osch)->format = "f"; break;
    case dt::SType::FLOAT64: (*osch)->format = "g"; break;
    case dt::SType::STR32:   (*osch)->format = "u"; break;
    case dt::SType::STR64:   (*osch)->format = "U"; break;
    default:
      throw NotImplError() << "Cannot convert column of type " << stype()
                           << " into arrow";
  }
  (*osch)->flags = ARROW_FLAG_NULLABLE;
  (*osch)->release = release_arrow_schema;
  return osch;
}



Column dt::ColumnImpl::_as_arrow_bool() const {
  xassert(stype_ == SType::BOOL);
  Buffer validity_buffer = Buffer::mem((nrows_ + 7) / 8);
  Buffer data_buffer = Buffer::mem((nrows_ + 7) / 8);
  auto validity = static_cast<uint8_t*>(validity_buffer.xptr());
  auto data = static_cast<uint8_t*>(data_buffer.xptr());

  parallel_for_static(nrows_, dt::NThreads(allow_parallel_access()),
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
  Buffer validity_buffer = Buffer::mem((nrows_ + 7) / 8);
  Buffer data_buffer = Buffer::mem(nrows_ * sizeof(T));
  auto validity = static_cast<uint8_t*>(validity_buffer.xptr());
  auto data = static_cast<T*>(data_buffer.xptr());

  parallel_for_static(nrows_, dt::NThreads(allow_parallel_access()),
    [=](size_t i) {
      bool isvalid = this->get_element(i, &data[i]);
      if (isvalid) {
        validity[i/8] |= 1 << (i&7);
      }
    });

  return Column(new ArrowFw_ColumnImpl(
      nrows_, stype_, std::move(validity_buffer), std::move(data_buffer))
  );
}


// template <typename T>
// Column dt::ColumnImpl::_as_arrow_str() const {

// }


/**
  * Convert this column into a column whose implementation is
  * Arrow_ColumnImpl.
  */
Column dt::ColumnImpl::as_arrow() const {
  switch (stype_) {
    case SType::BOOL: return _as_arrow_bool();
    case SType::INT8: return _as_arrow_fw<int8_t>();
    case SType::INT16: return _as_arrow_fw<int16_t>();
    case SType::INT32: return _as_arrow_fw<int32_t>();
    case SType::INT64: return _as_arrow_fw<int64_t>();
    case SType::FLOAT32: return _as_arrow_fw<float>();
    case SType::FLOAT64: return _as_arrow_fw<double>();
    default: break;
  }
  throw NotImplError() << "Cannot convert column of type " << stype_ << " into arrow";
}
