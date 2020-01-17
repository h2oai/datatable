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
#include "frame/py_frame.h"
#include "python/_all.h"
#include "python/xobject.h"
#include "utils/assert.h"
namespace py {


class FrameIterator : public XObject<FrameIterator>
{
  private:
    oobj frame;
    DataTable* dt;
    size_t iteration_index;
    bool reverse;
    size_t : 56;

    void m__init__(const PKArgs& args) {
      frame = args[0].to_oobj();
      reverse = args[1].to_bool_strict();
      dt = frame.to_datatable();
      iteration_index = 0;
    }

    void m__dealloc__() {
      frame = nullptr;
      dt = nullptr;
    }

    // See PEP-424
    // Note: the underlying DataTable may get modified while iterating
    oobj m__length_hint__() {
      if (iteration_index >= dt->ncols()) return oint(0);
      return oint(dt->ncols() - iteration_index);
    }

    oobj m__next__() {
      if (iteration_index >= dt->ncols()) {
        return oobj();
      }
      size_t i = (iteration_index++);
      if (reverse) i = dt->ncols() - i - 1;
      return Frame::oframe(dt->extract_column(i));
    }

  public:
    static void impl_init_type(XTypeMaker& xt) {
      xt.set_class_name("frame_iterator");

      static PKArgs args_init(2, 0, 0, false, false, {"frame", "reversed"},
                              "__init__", nullptr);
      xt.add(CONSTRUCTOR(&FrameIterator::m__init__, args_init));
      xt.add(DESTRUCTOR(&FrameIterator::m__dealloc__));
      xt.add(METHOD__LENGTH_HINT__(&FrameIterator::m__length_hint__));
      xt.add(METHOD__NEXT__(&FrameIterator::m__next__));
    }
};


oobj Frame::m__iter__() {
  return FrameIterator::make(oobj(this), obool(false));
}

oobj Frame::m__reversed__() {
  return FrameIterator::make(oobj(this), obool(true));
}

void Frame::_init_iter(XTypeMaker& xt) {
  FrameIterator::init_type(nullptr);
  xt.add(METHOD__ITER__(&Frame::m__iter__));
  xt.add(METHOD__REVERSED__(&Frame::m__reversed__));
}


} // namespace py
