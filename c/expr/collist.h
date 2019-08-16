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
#include <memory>
#include <string>
#include <vector>
#include "python/obj.h"
namespace dt {

class collist;
class base_expr;
class workframe;

using strvec = std::vector<std::string>;
using exprvec = std::vector<std::unique_ptr<dt::expr::base_expr>>;
using intvec = std::vector<size_t>;
using collist_ptr = std::unique_ptr<collist>;



class collist {
  private:
    exprvec exprs;
    intvec indices;
    strvec names;

  public:
    collist(workframe& wf, py::robj src, const char* srcname,
            size_t dt_index = 0);
    collist(exprvec&& exprs_, intvec&& indices_, strvec&& names_);

    bool is_simple_list() const;
    strvec release_names();
    intvec release_indices();
    exprvec release_exprs();

    size_t size() const;
    void append(collist_ptr&&);
    void exclude(collist_ptr&&);

  private:
    void check_integrity();
};




}  // namespace dt
#endif
