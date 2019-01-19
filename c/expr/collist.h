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
#ifndef dt_EXPR_COLLIST_h
#define dt_EXPR_COLLIST_h
#include <vector>
#include "expr/base_expr.h"
#include "expr/workframe.h"
#include "python/obj.h"

namespace dt {
class collist;


using exprvec = std::vector<std::unique_ptr<dt::base_expr>>;
using intvec = std::vector<size_t>;
using collist_ptr = std::unique_ptr<collist>;



class collist {
  public:
    static collist_ptr make(workframe& wf, py::robj src, const char* srcname);
    virtual ~collist();
};



struct cols_intlist : public collist {
  intvec indices;
  strvec names;

  cols_intlist(intvec&& indices_, strvec&& names_);
  ~cols_intlist() override;
};



struct cols_exprlist : public collist {
  exprvec exprs;
  strvec  names;

	cols_exprlist(exprvec&& exprs_, strvec&& names_);
  ~cols_exprlist() override;
};




}  // namespace dt
#endif
