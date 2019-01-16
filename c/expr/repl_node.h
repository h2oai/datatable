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
#ifndef dt_EXPR_REPL_NODE_h
#define dt_EXPR_REPL_NODE_h
#include <memory>
#include "python/obj.h"
namespace dt {


class repl_node;
using repl_node_ptr = std::unique_ptr<repl_node>;


class repl_node {
  public:
    virtual ~repl_node();
    static repl_node_ptr make(workframe& wf, py::oobj src);

    /**
     * Check whether this replacement node is valid for replacing a rectangular
     * subset of data of shape [lrows x lcols]. If valid, this function simply
     * returns, otherwise an exception is raised.
     */
    virtual void check_compatibility(size_t lrows, size_t lcols) const = 0;

    /**
     * Replace the columns of `dt0` (taken from the workframe) at indices
     * `ind` with the values from this replacement node. The columns are
     * replaced as whole.
     *
     * This method is used when `ri0` from the workframe is empty (meaning
     * all rows should be used).
     */
    virtual void replace_columns(workframe& wf, const intvec& ind) const = 0;

    /**
     * Replace the values in `dt0[ri0, ind]` with the values from this
     * replacement node. Thus, only a subset of data in the Frame will be
     * modified. Here `dt0` and `ri0` are taken from the workframe.
     *
     * This method is used when `ri0` is not empty.
     */
    virtual void replace_values(workframe&, const intvec&) const = 0;
};



}
#endif
