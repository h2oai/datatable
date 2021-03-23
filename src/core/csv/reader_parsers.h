//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#ifndef dt_CSV_READER_PARSERS_h
#define dt_CSV_READER_PARSERS_h
#include <iterator>    // std::input_iterator_tag
#include "_dt.h"
#include "read/parsers/library.h"
#include "read/parsers/pt.h"

typedef void (*ParserFnPtr)(const dt::read::ParseContext& ctx);


//------------------------------------------------------------------------------
// Do not use "enum class" here: we want these enums to be implicitly
// convertible into integers, so that we can use them as array indices.
namespace dt {
namespace read {



/**
 * Requested Type -- column type as requested by the user; each may correspond
 * to one or more parse types.
 */
enum RT : uint8_t {
  RDrop    = 0,
  RAuto    = 1,
  RBool    = 2,
  RInt     = 3,
  RInt32   = 4,
  RInt64   = 5,
  RFloat   = 6,
  RFloat32 = 7,
  RFloat64 = 8,
  RStr     = 9,
  RStr32   = 10,
  RStr64   = 11,
};



class PtypeIterator {
  private:
    int8_t* pqr;
    RT rtype;
    PT orig_ptype;
    PT curr_ptype;
    int64_t : 40;

  public:
    PtypeIterator(PT pt, RT rt, int8_t* qr_ptr);
    PT operator*() const;
    PtypeIterator& operator++();
    bool has_incremented() const;
    RT get_rtype() const;
};


}}


// unused?
class ParserIterator {
  private:
    int ipt;
    const uint8_t ptype;
    int : 24;

  public:
    using value_type = dt::read::PT;
    using category_type = std::input_iterator_tag;

    ParserIterator();
    ParserIterator(dt::read::PT pt);
    ParserIterator(const ParserIterator&) = default;
    ParserIterator& operator=(const ParserIterator&) = delete;
    ~ParserIterator() {}
    ParserIterator& operator++();
    bool operator==(const ParserIterator&) const;
    bool operator!=(const ParserIterator&) const;
    value_type operator*() const;
};



#endif
