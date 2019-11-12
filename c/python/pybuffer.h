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
#ifndef dt_PYTHON_PYBUFFER_h
#define dt_PYTHON_PYBUFFER_h
#include "column/view.h"
#include "python/obj.h"
#include "buffer.h"
#include "column.h"
namespace py {



/**
  * This class is a wrapper around a `Py_buffer` struct. It is NOT
  * a pyobject!
  *
  * The purpose of this class is to automatically release the
  * underlying resources when this object goes out of scope.
  */
class buffer
{
  private:
    Py_buffer* info_;  // owned
    size_t stride_;

  public:
    // Fill-in the buffer from the object `obj`.
    // The buffer is first queried with flags PyBUF_FORMAT|PyBUF_STRIDES,
    // and if that fails, with flags PyBUF_FORMAT|PyBUF_ND.
    // If both attempts fail, an exception is thrown.
    //
    buffer(robj obj);
    buffer(const buffer&) = delete;
    buffer(buffer&&) noexcept;
    ~buffer();


    //---- Properties -------

    // Return the underlying data buffer. This buffer should be viewed
    // as T*, where sizeof(T) == itemsize().
    // The byte size of this buffer is `itemsize() * nelements() * stride()`.
    void* data() const noexcept;

    // Byte size of a single element in the `data()` buffer.
    size_t itemsize() const noexcept;

    // The number of elements in the buffer. The elements may not be
    // contiguous.
    size_t nelements() const noexcept;

    // The step at which the elements should be accessed. In particular,
    // the array `static_cast<T*>(data())` may be addressed at indices
    //
    //   i = 0, stride(), ..., stride() * (nelements() - 1)
    //
    size_t stride() const noexcept;


    SType stype() const;

    Column to_column() &&;


  private:
    void _normalize_dimensions();
};



}  // namespace py
#endif
