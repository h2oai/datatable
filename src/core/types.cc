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
#include "_dt.h"
#include "ltype.h"
#include "stype.h"



static_assert(INTPTR_MAX == INT64_MAX,
              "Only 64 bit platforms are supported.");

static_assert(sizeof(void*) == 8, "Expected size(void*) to be 8 bytes");
static_assert(sizeof(void*) == sizeof(size_t),
              "size(size_t) != size(void*)");
static_assert(sizeof(void*) == sizeof(int64_t),
              "size(int64_t) != size(void*)");

static_assert(sizeof(int8_t) == 1, "int8_t should be 1-byte");
static_assert(sizeof(int16_t) == 2, "int16_t should be 2-byte");
static_assert(sizeof(int32_t) == 4, "int32_t should be 4-byte");
static_assert(sizeof(int64_t) == 8, "int64_t should be 8-byte");
static_assert(sizeof(float) == 4, "float should be 4-byte");
static_assert(sizeof(double) == 8, "double should be 8-byte");
static_assert(sizeof(char) == sizeof(unsigned char), "char != uchar");
static_assert(sizeof(char) == 1, "sizeof(char) != 1");

static_assert(sizeof(dt::LType) == 1, "LType does not fit in a byte");
static_assert(sizeof(dt::SType) == 1, "SType does not fit in a byte");

static_assert(static_cast<unsigned>(-1) - static_cast<unsigned>(-3) == 2,
              "Unsigned arithmetics check");
static_assert(3u - (0-1u) == 4u, "Unsigned arithmetics check");
static_assert(0-1u == 0xFFFFFFFFu, "Unsigned arithmetics check");

static_assert(sizeof(int64_t) == sizeof(Py_ssize_t),
              "int64_t and Py_ssize_t should refer to the same type");
