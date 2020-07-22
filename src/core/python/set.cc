//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include "python/set.h"
#include "python/python.h"
namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

oset::oset() {
  v = PySet_New(nullptr);
  if (!v) throw PyError();
}

oset::oset(const robj& src) : oobj(src) {}



//------------------------------------------------------------------------------
// Accessors
//------------------------------------------------------------------------------

size_t oset::size() const {
  return static_cast<size_t>(PySet_Size(v));
}

/**
 * Test whether the `key` is present in the set. If the lookup raises an error
 * (for example if key is not hashable), discard it and return `false`.
 */
bool oset::has(const _obj& key) const {
  int ret = PySet_Contains(v, key.to_borrowed_ref());
  if (ret == -1) PyErr_Clear();
  return (ret == 1);
}

/**
 * Insert the provided `key` into the set.
 */
void oset::add(const _obj& key) {
  int ret = PySet_Add(v, key.to_borrowed_ref());
  if (ret == -1) throw PyError();
}



}  // namespace py
