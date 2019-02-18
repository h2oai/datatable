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
#ifndef dt_EXPR_I_NODE_h
#define dt_EXPR_I_NODE_h
#include <memory>             // std::unique_ptr
#include "python/_all.h"      // py::robj, ...
namespace dt {


class i_node;
class workframe;
using i_node_ptr = std::unique_ptr<dt::i_node>;

/**
 * Base class for all "Row Filter" nodes. A row filter node represents the
 * `i` part in a `DT[i, j, ...]` call.
 *
 * When executed, this class will compute a RowIndex and apply it to the
 * provided workframe `wf`.
 */
class i_node {
  public:
    static i_node_ptr make(py::robj src, workframe& wf);

    i_node();
    virtual ~i_node();
    virtual void post_init_check(workframe&);
    virtual void execute(workframe&) = 0;
    virtual void execute_grouped(workframe&) = 0;
};




}  // namespace dt
#endif
