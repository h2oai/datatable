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
#include <memory>      // std::shared_ptr
#include "python/_all.h"
#include "python/xobject.h"
#include "utils/assert.h"
namespace py {


class ReadIterator : public XObject<ReadIterator>
{
  private:
    std::shared_ptr<GenericReader> gr_;

    void m__init__(const PKArgs& args) {}

    void m__dealloc__() {
      gr_ = nullptr;
    }

    // oobj m__length_hint__() {
      // if (iteration_index >= dt->ncols()) return oint(0);
      // return oint(dt->ncols() - iteration_index);
    // }

    oobj m__next__() {
      if (!gr_ || !gr_->has_next()) return py::oobj();
      return gr_->read_next();
    }

    oobj m__getitem__(py::robj item) {
      return py::None();
    }

  public:
    static oobj make(GenericReader* g) {
      robj rtype(reinterpret_cast<PyObject*>(&type));
      oobj res = rtype.call();
      ReadIterator* iterator = ReadIterator::cast_from(res);
      iterator->gr_ = std::shared_ptr<GenericReader>(g);
      return res;
    }

    static void impl_init_type(XTypeMaker& xt) {
      xt.set_class_name("read_iterator");

      static PKArgs args_init(0, 0, 0, false, false, {}, "__init__", nullptr);
      xt.add(CONSTRUCTOR(&ReadIterator::m__init__, args_init));
      xt.add(DESTRUCTOR(&ReadIterator::m__dealloc__));
      // xt.add(METHOD__LENGTH_HINT__(&ReadIterator::m__length_hint__));
      xt.add(METHOD__NEXT__(&ReadIterator::m__next__));
      xt.add(METHOD__GET_ITEM__(&ReadIterator::m__getitem__));
    }
};




} // namespace py
