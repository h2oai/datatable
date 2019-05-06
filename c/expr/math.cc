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
#include <cmath>
#include <unordered_map>
#include "datatablemodule.h"

struct fninfo {
  const char* name;
};

static std::unordered_map<const py::PKArgs*, fninfo> fninfos;



static py::PKArgs args_acos(
  1, 0, 0, false, false, {"x"}, "acos",
  "Return the arc cosine of x, in radians.");


static py::oobj generic_unary_fn(const py::PKArgs& args) {
  xassert(fninfos.count(&args) == 1);
  auto& arg = args[0];
  if (arg.is_undefined()) {
    throw TypeError() << '`' << fninfos[&args].name << "()` takes exactly one "
        "argument, 0 given";
  }

}




void py::DatatableModule::init_methods_math() {
  ADD_FN(&generic_unary_fn, args_acos);

  fninfos[&args_acos] = {"acos"};
}
