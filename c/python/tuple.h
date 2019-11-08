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
#ifndef dt_PYTHON_TUPLE_h
#define dt_PYTHON_TUPLE_h
#include <initializer_list>
#include "python/obj.h"

namespace py {


/**
 * Python tuple of elements.
 *
 * This can be either a tuple passed down from Python, or a new tuple object.
 * If `otuple` is created from an existing PyObject, its elements must not be
 * modified. On the contrary, if a tuple is created from scratch, its elements
 * must be properly initialized before the tuple can be returned to Python.
 */
class otuple : public oobj {
  public:
    otuple() = default;
    otuple(const otuple&) = default;
    otuple(otuple&&) = default;
    otuple& operator=(const otuple&) = default;
    otuple& operator=(otuple&&) = default;

    otuple(size_t n);
    otuple(std::initializer_list<oobj>);
    otuple(const _obj&);
    otuple(const _obj&, const _obj&);
    otuple(const _obj&, const _obj&, const _obj&);
    otuple(oobj&&);
    otuple(oobj&&, oobj&&);
    otuple(oobj&&, oobj&&, oobj&&);

    size_t size() const noexcept;

    robj operator[](size_t i) const;

    /**
     * `set(...)` methods should only be used to fill-in a new tuple object.
     * They do not perform any bounds checks; and cannot properly overwrite
     * an existing value.
     */
    void set(size_t i,  const _obj& value);
    void set(size_t i,  oobj&& value);

    /**
     * `replace(...)` methods are safer alternatives to `set(...)`: they
     * perform proper bounds checks, and if an element already exists in the
     * tuple at index `i`, it will be properly disposed of.
     */
    void replace(size_t i,  const _obj& value);
    void replace(size_t i,  oobj&& value);


  private:
    // Private constructors, used from `_obj`. If you need to construct
    // `otuple` from `_obj`, use `_obj.to_otuple()` instead.
    otuple(const robj&);
    friend class _obj;
    void make_editable();
};



/**
 * Same as `otuple`, only the object is owned by reference.
 * The constructor will not check whether the object is created from PyTuple
 * or not -- the caller is expected to do that himself. Thus, this class
 * should be used judiciously.
 */
class rtuple : public robj {
  public:
    rtuple() = default;
    rtuple(const rtuple&) = default;
    rtuple(rtuple&&) = default;
    rtuple& operator=(const rtuple&) = default;
    rtuple& operator=(rtuple&&) = default;

    static rtuple unchecked(const robj&);

    size_t size() const noexcept;

    robj operator[](size_t i) const;

  private:
    rtuple(const robj&);
    friend class _obj;
};


}  // namespace py

#endif
