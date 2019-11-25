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
#include "expr/op.h"
#include "expr/declarations.h"
#include "python/obj.h"
namespace dt {
namespace expr {


static const char* doc_shift =
R"(shift(col, n=1, fill=None)
--

Produce a column obtained from `col` shifting it  `n` rows forward.

The shift amount, `n`, can be both positive and negative. If positive,
a "lag" column is created, if negative it will be a "lead" column.

The shifted column will have the same number of rows as the original
column, with `n` observations in the beginning becoming missing, and
`n` observations at the end discarded. If parameter `fill` is given,
then the missing observations at the beginning will be filled with
the provided value.

This function is group-aware, i.e. in the presence of a groupby it
will perform the shift separately within each group.
)";

py::PKArgs args_shift(
    1, 1, 1, false, false, {"col", "n", "fill"}, "shift", doc_shift);


/**
  * Python-facing function that implements an unary operator / single-
  * argument function. This function can take as an argument either a
  * python scalar, or an f-expression, or a Frame (in which case the
  * function is applied to all elements of the frame).
  */
py::oobj pyfn_shift(const py::PKArgs& args)
{
  const py::Arg& arg_col = args[0];
  const py::Arg& arg_n = args[1];
  const py::Arg& arg_fill = args[2];

  // n
  int shift_amount = arg_n.to<int32_t>(1);


  // py::robj x = args[0].to_robj();
  // if (x.is_dtexpr()) {
  //   return make_pyexpr(opcode, x);
  // }
  // if (x.is_frame()) {
  //   return process_frame(opcode, x);
  // }
  // if (x.is_int())   return unaryop(opcode, x.to_int64_strict());
  // if (x.is_float()) return unaryop(opcode, x.to_double());
  // if (x.is_none())  return unaryop(opcode, nullptr);
  // if (x.is_bool())  return unaryop(opcode, static_cast<bool>(x.to_bool_strict()));
  // if (x.is_string())return unaryop(opcode, x.to_cstring());
  // if (!x) {
  //   throw TypeError() << "Function `" << args.get_short_name() << "()` takes "
  //       "exactly one argument, 0 given";
  // }
  // throw TypeError() << "Function `" << args.get_short_name() << "()` cannot "
  //     "be applied to an argument of type " << x.typeobj();
}






}}  // dt::expr
