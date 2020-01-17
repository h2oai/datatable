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
#include <unordered_map>
#include "expr/args_registry.h"
#include "utils/assert.h"
namespace dt {
namespace expr {


/**
  * This is a simple mechanism to retrieve an opcode from a py::PKArgs
  * parameter passed to a function. Since all PKArgs are singletons,
  * we just keep a map of which PKArgs corresponds to which Op.
  *
  * An alternative mechanism could be for PKArgs to provide a user-
  * customizable area, where we could store the Op for each PKArgs.
  */

static std::unordered_map<const py::PKArgs*, Op> args2opcodes;


void register_args(const py::PKArgs& args, Op opcode) {
  xassert(args2opcodes.count(&args) == 0);
  args2opcodes[&args] = opcode;
}


Op get_opcode_from_args(const py::PKArgs& args) {
  xassert(args2opcodes.count(&args) == 1);
  return args2opcodes.at(&args);
}




}}
