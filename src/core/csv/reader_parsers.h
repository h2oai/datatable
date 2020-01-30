//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_CSV_READER_PARSERS_h
#define dt_CSV_READER_PARSERS_h
#include <string>
#include <vector>
#include "types.h"

namespace dt {
namespace read {
  class Column;
  struct FreadTokenizer;
}}
typedef void (*ParserFnPtr)(dt::read::FreadTokenizer& ctx);
typedef PyObject* (*FormatGeneratorFn)(dt::read::Column& col);


// In order to add a new type:
//   - implement a new parser function `void (*)(dt::read::FreadTokenizer&)`
//   - add a new identifier into `enum PT`
//   - declare this parser in `ParserLibrary::init_parsers()`
//   - update `test_fread_fillna1` in test_fread.py to include the new type
//

void parse_mu(dt::read::FreadTokenizer&);
void parse_bool8_numeric(dt::read::FreadTokenizer&);
void parse_bool8_uppercase(dt::read::FreadTokenizer&);
void parse_bool8_lowercase(dt::read::FreadTokenizer&);
void parse_bool8_titlecase(dt::read::FreadTokenizer&);
void parse_int32_simple(dt::read::FreadTokenizer&);
void parse_int64_simple(dt::read::FreadTokenizer&);
void parse_float32_hex(dt::read::FreadTokenizer&);
void parse_float64_simple(dt::read::FreadTokenizer& ctx);
void parse_float64_extended(dt::read::FreadTokenizer& ctx);
void parse_float64_hex(dt::read::FreadTokenizer&);
void parse_string(dt::read::FreadTokenizer&);


//------------------------------------------------------------------------------
// Do not use "enum class" here: we want these enums to be implicitly
// convertible into integers, so that we can use them as array indices.

/**
 * Parse Type -- each identifier corresponds to one of the parser functions
 * listed above.
 */
enum PT : uint8_t {
  Mu,
  Bool01,
  BoolU,
  BoolT,
  BoolL,
  Int32,
  Int32Sep,
  Int64,
  Int64Sep,
  // Float32Plain,
  Float32Hex,
  Float64Plain,
  Float64Ext,
  Float64Hex,
  Str32,
  Str64,
};


/**
 * Bump Type -- this describes transition between different parsers:
 *   Simple - values read by the previous parser can be used as-is with the
 *            new parser. For example, changing from PT::Float64Plain to
 *            PT::Float64Ext.
 *   Normal - values read by the previous parser can be losslessly converted
 *            into the values produced by the new parser. For example,
 *            transition from PT::Int32 to PT::Int64.
 *   Reread - values read by the previous parser cannot be converted, so the
 *            entire column has to be re-read.
 */
enum BT : uint8_t {
  None   = 0,
  Simple = 1,
  Normal = 2,
  Reread = 3,
};


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



//------------------------------------------------------------------------------
// ParserInfo
//------------------------------------------------------------------------------
class ParserLibrary;


class ParserInfo {
  public:
    ParserFnPtr fn;
    std::string name;
    char        code;
    int8_t      elemsize;
    SType       stype;
    PT          id;
    int : 32;

    ParserInfo() : fn(nullptr), code(0) {}
    ParserInfo(PT id_, const char* name_, char code_, int8_t sz_, SType st_,
               ParserFnPtr ptr)
      : fn(ptr), name(name_), code(code_), elemsize(sz_), stype(st_), id(id_) {}

    const char* cname() const { return name.data(); }
    bool isstring() const { return id >= PT::Str32; }
};



//------------------------------------------------------------------------------
// Parser iterators
//------------------------------------------------------------------------------

class ParserIterator {
  private:
    int ipt;
    const uint8_t ptype;
    int : 24;

  public:
    using value_type = PT;
    using category_type = std::input_iterator_tag;

    ParserIterator();
    ParserIterator(PT pt);
    ParserIterator(const ParserIterator&) = default;
    ParserIterator& operator=(const ParserIterator&) = default;
    ~ParserIterator() {}
    ParserIterator& operator++();
    bool operator==(const ParserIterator&) const;
    bool operator!=(const ParserIterator&) const;
    value_type operator*() const;
};

class ParserIterable {
  private:
    const PT ptype;
    int64_t : 56;

  public:
    using iterator = ParserIterator;

    ParserIterable(PT pt);
    iterator begin() const;
    iterator end() const;
};



//------------------------------------------------------------------------------
// ParserLibrary
//------------------------------------------------------------------------------

class ParserLibrary {
  private:
    static ParserInfo* parsers;
    static ParserFnPtr* parser_fns;
    void init_parsers();
    int64_t : 64;

  public:
    static constexpr size_t num_parsers = static_cast<size_t>(PT::Str64) + 1;
    ParserLibrary();
    ParserLibrary(const ParserLibrary&) = delete;
    void operator=(const ParserLibrary&) = delete;

    ParserIterable successor_types(PT pt) const;

    static const ParserFnPtr* get_parser_fns() { return parser_fns; }
    static const ParserInfo* get_parser_infos() { return parsers; }
    static const ParserInfo& info(size_t i) { return parsers[i]; }
    static const ParserInfo& info(PT i) { return parsers[i]; }
};


#endif
