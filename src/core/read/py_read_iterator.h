//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
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
#ifndef dt_READ_PY_READ_ITERATOR_h
#define dt_READ_PY_READ_ITERATOR_h
#include <memory>             // std::unique_ptr
#include "python/xobject.h"   // XObject, PKArgs
#include "read/multisource.h" // dt::read::MultiSource, dt::read::GenericReader
namespace py {


class ReadIterator : public XObject<ReadIterator>
{
  private:
    std::unique_ptr<dt::read::MultiSource> multisource_;

    void m__init__(const PKArgs& args);
    void m__dealloc__();
    oobj m__next__();

  public:
    static oobj make(std::unique_ptr<dt::read::MultiSource>&& multisource);
    static void impl_init_type(XTypeMaker& xt);
};




} // namespace py
#endif
