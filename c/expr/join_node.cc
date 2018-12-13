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
#include "expr/join_node.h"
#include "datatable.h"
#include "python/arg.h"



py::PKArgs py::join::Type::args___init__(
    1, 0, 0, false, false, {"frame"}, "__init__", nullptr);

const char* py::join::Type::classname() {
  return "datatable.join";
}

const char* py::join::Type::classdoc() {
  return "join() clause for use in DT[i, j, ...]";
}


void py::join::m__init__(py::PKArgs& args) {
  join_frame = args[0].to_oobj();
  if (!join_frame.is_frame()) {
    throw TypeError() << "The argument to join() must be a Frame";
  }
  DataTable* jdt = join_frame.to_frame();
  if (jdt->get_nkeys() == 0) {
    throw ValueError() << "The join frame is not keyed";
  }
}


void py::join::m__dealloc__() {
  join_frame = nullptr;  // Releases the stored oobj
}


void py::join::Type::init_methods_and_getsets(
    py::ExtType<py::join>::Methods&,
    py::ExtType<py::join>::GetSetters& gs)
{
  gs.add<&py::join::get_frame>("joinframe");
}


py::oobj py::join::get_frame() const {
  return join_frame;
}
