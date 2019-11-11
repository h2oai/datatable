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
//
// See: https://docs.python.org/3/c-api/buffer.html
//
//------------------------------------------------------------------------------
#include "python/pybuffer.h"
#include "utils/assert.h"
#include "buffer.h"
namespace py {



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

buffer::buffer(robj obj) {
  info_ = new Py_buffer();
  stride_ = 1;

  // int PyObject_GetBuffer(PyObject *exporter, Py_buffer *view, int flags)
  //
  //   Send a request to exporter to fill in view as specified by flags. If
  //   the exporter cannot provide a buffer of the exact type, it MUST raise
  //   PyExc_BufferError, set view->obj to NULL and return -1.
  //
  //   On success, fill in view, set view->obj to a new reference to exporter
  //   and return 0. In the case of chained buffer providers that redirect
  //   requests to a single object, view->obj MAY refer to this object
  //   instead of exporter (See Buffer Object Structures).
  //
  //   Successful calls to PyObject_GetBuffer() must be paired with calls to
  //   PyBuffer_Release(), similar to malloc() and free(). Thus, after the
  //   consumer is done with the buffer, PyBuffer_Release() must be called
  //   exactly once.
  //
  PyObject* exporter = obj.to_borrowed_ref();
  int ret = PyObject_GetBuffer(exporter, info_, PyBUF_FORMAT|PyBUF_STRIDES);
  if (ret != 0) {
    PyErr_Clear();
    ret = PyObject_GetBuffer(exporter, info_, PyBUF_FORMAT|PyBUF_ND);
  }
  if (ret != 0) {
    delete info_;
    info_ = nullptr;
    throw PyError();
  }
  _normalize_dimensions();
}


buffer::buffer(buffer&& other) noexcept {
  info_ = other.info_;
  stride_ = other.stride_;
  other.info_ = nullptr;
}


buffer::~buffer() {
  if (info_) {
    PyBuffer_Release(info_);
    delete info_;
    info_ = nullptr;
  }
}


void buffer::_normalize_dimensions() {
  auto itemsize = info_->itemsize;
  auto len = info_->len;
  auto ndim = info_->ndim;
  xassert(itemsize > 0);
  xassert(len % itemsize == 0);
  xassert(ndim >= 0);

  if (len == 0) {}
  else if (ndim == 0) {
    xassert(len == itemsize);
  }
  else if (ndim == 1) {
    if (info_->shape) xassert(info_->shape[0] * itemsize == len);
    if (info_->strides) {
      xassert(info_->strides[0] % itemsize == 0);
      stride_ = static_cast<size_t>(info_->strides[0] / itemsize);
    }
  }
  else {
    xassert(info_->shape && info_->strides);
    bool dim_found = false;
    for (int i = 0; i < ndim; ++i) {
      auto dim = info_->shape[i];
      auto step = info_->strides[i];
      xassert(dim > 0);
      xassert(step % itemsize == 0);
      if (dim == 1) continue;
      if (dim_found) {
        throw ValueError() << "Source buffer has more than one non-trivial "
                              "dimension, which is not supported";
      }
      dim_found = true;
      stride_ = static_cast<size_t>(step / itemsize);
    }
  }
}




//------------------------------------------------------------------------------
// Properties
//------------------------------------------------------------------------------

void* buffer::data() const noexcept {
  return info_->buf;
}

size_t buffer::itemsize() const noexcept {
  return static_cast<size_t>(info_->itemsize);
}

size_t buffer::nelements() const noexcept {
  return static_cast<size_t>(info_->len / info_->itemsize);
}

size_t buffer::stride() const noexcept {
  return stride_;
}



SType buffer::stype() const {
  const char* format = info_->format;
  int64_t itemsize = info_->itemsize;

  SType stype = SType::VOID;
  char c = format[0];
  if (c == '@' || c == '=') c = format[1];

  if (c == 'b' || c == 'h' || c == 'i' || c == 'l' || c == 'q' || c == 'n') {
    // These are all various integer types
    stype = itemsize == 1 ? SType::INT8 :
            itemsize == 2 ? SType::INT16 :
            itemsize == 4 ? SType::INT32 :
            itemsize == 8 ? SType::INT64 : SType::VOID;
  }
  else if (c == 'd' || c == 'f') {
    stype = itemsize == 4 ? SType::FLOAT32 :
            itemsize == 8 ? SType::FLOAT64 : SType::VOID;
  }
  else if (c == '?') {
    stype = itemsize == 1 ? SType::BOOL : SType::VOID;
  }
  else if (c == 'O') {
    stype = SType::OBJ;
  }
  else if (c >= '1' && c <= '9') {
    if (format[strlen(format) - 1] == 'w') {
      int numeral = atoi(format);
      if (itemsize == numeral * 4) {
        stype = SType::STR32;
      }
    }
  }
  if (stype == SType::VOID) {
    throw ValueError()
        << "Unknown format '" << format << "' with itemsize " << itemsize;
  }
  if (!::info(stype).is_varwidth()) {
    xassert(::info(stype).elemsize() == static_cast<size_t>(itemsize));
  }
  return stype;
}


Column buffer::to_column() &&
{
  SType  stype = this->stype();
  size_t nrows = this->nelements();
  void*  ptr   = this->data();
  if (nrows == 0) {
    return Column::new_data_column(0, stype);
  }
  else if (stride_ == 1) {
    size_t datasize = itemsize() * nrows;
    Buffer databuf = Buffer::external(ptr, datasize, std::move(*this));
    return Column::new_mbuf_column(nrows, stype, std::move(databuf));
  }
  else if (static_cast<int64_t>(stride_) > 0) {
    size_t datasize = itemsize() * nrows * stride_;
    Buffer databuf = Buffer::external(ptr, datasize, std::move(*this));
    Column internal_col = Column::new_mbuf_column(nrows * stride_, stype,
                                                  std::move(databuf));
    return Column(new dt::SliceView_ColumnImpl(
                          std::move(internal_col),
                          RowIndex(0, nrows, stride_)
                  ));
  }
  else {
    size_t datasize = itemsize() * nrows * (-stride_);
    ptr = static_cast<char*>(ptr) - itemsize() * (nrows - 1) * (-stride_);
    Buffer databuf = Buffer::external(ptr, datasize, std::move(*this));
    Column internal_col = Column::new_mbuf_column(nrows * (-stride_), stype,
                                                  std::move(databuf));
    return Column(new dt::SliceView_ColumnImpl(
                          std::move(internal_col),
                          RowIndex((nrows - 1) * (-stride_), nrows, stride_)
                  ));
  }
}




}  // namespace py
