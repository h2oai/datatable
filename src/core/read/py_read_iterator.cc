//------------------------------------------------------------------------------
// Copyright 2019-2021 H2O.ai
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
#include "csv/reader.h"
#include "read/py_read_iterator.h"
namespace py {


void ReadIterator::m__init__(const PKArgs&) {}

void ReadIterator::m__dealloc__() {
  multisource_ = nullptr;
}


oobj ReadIterator::m__next__() {
  return multisource_->read_next();
}


oobj ReadIterator::make(std::unique_ptr<dt::read::MultiSource>&& multisource) {
  oobj resobj = robj(ReadIterator::typePtr).call();
  ReadIterator* iterator = ReadIterator::cast_from(resobj);
  iterator->multisource_ = std::move(multisource);
  return resobj;
}


void ReadIterator::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("read_iterator");

  static PKArgs args_init(0, 0, 0, false, false, {}, "__init__", nullptr);
  xt.add(CONSTRUCTOR(&ReadIterator::m__init__, args_init));
  xt.add(DESTRUCTOR(&ReadIterator::m__dealloc__));
  xt.add(METHOD__NEXT__(&ReadIterator::m__next__));
}




} // namespace py
