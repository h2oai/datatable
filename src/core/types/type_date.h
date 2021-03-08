//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#ifndef dt_TYPES_TYPE_DATE_h
#define dt_TYPES_TYPE_DATE_h
#include "frame/py_frame.h"
#include "stype.h"
#include "types/type_impl.h"
namespace dt {



class Type_Date32 : public TypeImpl {
  public:
    Type_Date32() : TypeImpl(SType::DATE32) {}

    bool can_be_read_as_int32() const override { return true; }
    std::string to_string() const override { return "date32"; }

    py::oobj min() const override {
      return _wrap_value(-std::numeric_limits<int>::max(), "min");
    }
    py::oobj max() const override {
      return _wrap_value(std::numeric_limits<int>::max() - 719468, "max");
    }

  private:
    // The min/max value is returned as a 1x1 frame instead of a python
    // datetime.date object because the latter can only accommodate year
    // range 1 .. 9999
    //
    py::oobj _wrap_value(int32_t value, const char* name) const {
      Column col = Column::new_data_column(1, SType::DATE32);
      static_cast<int32_t*>(col.get_data_editable())[0] = value;
      return py::Frame::oframe(new DataTable(
        {std::move(col)}, {name}
      ));
    }
};



}  // namespace dt
#endif
