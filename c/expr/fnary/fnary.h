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
#ifndef dt_EXPR_FNARY_FNARY_h
#define dt_EXPR_FNARY_FNARY_h
#include <memory>
#include "expr/declarations.h"
#include "expr/op.h"
#include "python/args.h"
#include "types.h"
namespace dt {
namespace expr {

using colvec = std::vector<Column>;

/**
  * Main method for computing n-ary (rowwise) functions.
  *
  */
Column naryop(Op opcode, colvec&& columns);



//------------------------------------------------------------------------------
// Resolvers [private]
//------------------------------------------------------------------------------

Column naryop_rowall(colvec&&);
Column naryop_rowany(colvec&&);
Column naryop_rowcount(colvec&&);
Column naryop_rowfirstlast(colvec&&, bool FIRST);
Column naryop_rowmean(colvec&&);
Column naryop_rowminmax(colvec&&, bool MIN);
Column naryop_rowsd(colvec&&);
Column naryop_rowsum(colvec&&);

/**
  * For a list of numeric columns, find the largest common stype.
  * Possible return values are: INT32, INT64, FLOAT32 or FLOAT64.
  * If any column in the list is not numeric, then an exception will
  * be thrown. The error message will use `fnname`.
  */
SType detect_common_numeric_stype(const colvec&, const char* fnname);


/**
  * Convert all columns in the list into a common stype.
  */
void promote_columns(colvec& columns, SType target_stype);




//------------------------------------------------------------------------------
// Python interface
//------------------------------------------------------------------------------

extern py::PKArgs args_rowall;
extern py::PKArgs args_rowany;
extern py::PKArgs args_rowcount;
extern py::PKArgs args_rowfirst;
extern py::PKArgs args_rowlast;
extern py::PKArgs args_rowmax;
extern py::PKArgs args_rowmean;
extern py::PKArgs args_rowmin;
extern py::PKArgs args_rowsd;
extern py::PKArgs args_rowsum;




}}  // namespace dt::expr
#endif
