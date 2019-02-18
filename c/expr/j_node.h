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
#ifndef dt_EXPR_J_NODE_h
#define dt_EXPR_J_NODE_h
namespace dt {


class j_node;
class repl_node;
class workframe;
using j_node_ptr = std::unique_ptr<dt::j_node>;

/**
 * This class handles the `j` part of the `DT[i, j, ...]` expression.
 */
class j_node {
  public:
    static j_node_ptr make(py::robj src, workframe& wf);

    j_node();
    virtual ~j_node();
    virtual GroupbyMode get_groupby_mode(workframe&) = 0;
    virtual void select(workframe&) = 0;
    virtual void delete_(workframe&) = 0;
    virtual void update(workframe&, repl_node*) = 0;
};



}  // namespace dt
#endif
