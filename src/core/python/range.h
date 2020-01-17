//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#ifndef dt_PYTHON_RANGE_h
#define dt_PYTHON_RANGE_h
#include "python/obj.h"
namespace py {


/**
 * Python range(start, stop, step) object.
 *
 * Bounds that are None are represented as GETNA<int64_t>().
 */
class orange : public oobj {
  public:
    orange() = default;
    orange(const orange&) = default;
    orange(orange&&) = default;
    orange& operator=(const orange&) = default;
    orange& operator=(orange&&) = default;

    orange(int64_t stop);
    orange(int64_t start, int64_t stop);
    orange(int64_t start, int64_t stop, int64_t step);

    int64_t start() const;
    int64_t stop()  const;
    int64_t step()  const;

    bool normalize(
        size_t len, size_t* pstart, size_t* pcount, size_t* pstep) const;

    static bool normalize(
        size_t len,
        int64_t istart, int64_t istop, int64_t istep,
        size_t* ostart, size_t* ocount, size_t* ostep);

  private:
    // Private constructor used from `_obj`.
    orange(const robj& src);
    friend class _obj;
};



}  // namespace py
#endif
