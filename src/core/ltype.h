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
#ifndef dt_LTYPE_h
#define dt_LTYPE_h
#include "_dt.h"           // size_t, uint8_t
#include "python/python.h" // PyObject
namespace dt {


/**
  * "Logical" type of a data column.
  *
  * Logical type is supposed to match the user's notion of a column
  * type. For example logical "LType::INT" type corresponds to the
  * mathematical set of integers, and thus reflects the usual notion
  * of what the "integer" *is*.
  *
  * Each logical type has multiple underlying "storage" types, that
  * describe how the type is actually stored in memory. For example,
  * LType::INT can be stored as an 8-, 16-, 32- or a 64-bit integer.
  * All "storage" types within a single logical type should be freely
  * interchangeable: operators or functions that accept certain
  * logical type should be able to work with any its storage subtype.
  *
  * Different logical types may or may not be interchangeable,
  * depending on the use case. For example, most binary operators
  * would promote boolean -> integer -> real; however some operators
  * / functions may not. For example, bit shift operators require
  * integer (or boolean) arguments.
  *
  *
  * LType::MU
  *     type for a column that has unknown type. Such column may
  *     contain only NA values.
  *
  * LType::BOOL
  *     column for storing boolean (0/1) values. Right now we only
  *     allow to store booleans as 1-byte signed chars. In most
  *     arithmetic expressions booleans are automatically promoted
  *     to integers (or reals) if needed.
  *
  * LType::INT
  *     integer values, equivalent of ℤ in mathematics. We support
  *     multiple storage sizes for integers: from 8 bits to 64 bits,
  *     but do not allow arbitrary-length integers. In most
  *     expressions integers will be automatically promoted to reals
  *     if needed.
  *
  * LType::REAL
  *     real values, equivalent of ℝ in mathematics. We store these
  *     in either fixed- or floating-point formats.
  *
  * LType::STRING
  *     all strings are encoded in UTF-8. We allow either variable-
  *     width strings or fixed-width.
  *
  * LType::DATETIME
  * LType::DURATION
  *
  * LType::OBJECT
  *     column for storing all other values of arbitrary (possibly
  *     heterogeneous) types. Each element is a `PyObject*`. Missing
  *     values are `Py_None`s.
  */
enum class LType : uint8_t {
  MU       = 0,
  BOOL     = 1,
  INT      = 2,
  REAL     = 3,
  STRING   = 4,
  DATETIME = 5,
  DURATION = 6,
  OBJECT   = 7,
  INVALID  = 8
};

constexpr size_t LTYPES_COUNT = static_cast<size_t>(LType::INVALID) + 1;



const char* ltype_name(LType);
py::oobj ltype_to_pyobj(LType);


// Used by obj.cc exclusively
bool is_ltype_object(PyObject*);
int ltype_from_pyobject(PyObject*);

// Invoked once during module initialization
void init_py_ltype_objs(PyObject* ltype_enum);


bool ltype_is_numeric(LType);


}  // namespace dt
#endif
