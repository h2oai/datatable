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
#include "csv/fread.h"


// In order to add a new type:
//   - implement a new parser function `void (*)(FreadTokenizer&)`
//   - add a new identifier into `enum PT`
//   - declare this parser in `ParserLibrary::init_parsers()`
//   - add items in `_coltypes_strs` and `_coltypes` in "fread.py"
//   - update `test_fread_fillna1` in test_fread.py to include the new type
//

void parse_mu(FreadTokenizer&);
void parse_bool8_numeric(FreadTokenizer&);
void parse_bool8_uppercase(FreadTokenizer&);
void parse_bool8_lowercase(FreadTokenizer&);
void parse_bool8_titlecase(FreadTokenizer&);
void parse_int32_simple(FreadTokenizer&);
void parse_int64_simple(FreadTokenizer&);
void parse_float32_hex(FreadTokenizer&);
void parse_float64_simple(FreadTokenizer& ctx);
void parse_float64_extended(FreadTokenizer& ctx);
void parse_float64_hex(FreadTokenizer&);
void parse_string(FreadTokenizer&);


//------------------------------------------------------------------------------

enum class PT : uint8_t {
  Drop,
  // Mu,
  Bool01,
  BoolU,
  BoolT,
  BoolL,
  Int32,
  Int64,
  // Float32Plain,
  Float32Hex,
  Float64Plain,
  Float64Ext,
  Float64Hex,
  Str32,
  Str64,
};


enum class BT : uint8_t {
  None   = 0,
  Simple = 1,
  Normal = 2,
  Reread = 3,
};



//------------------------------------------------------------------------------
// ParserInfo
//------------------------------------------------------------------------------
class ParserLibrary;


class ParserInfo {
  public:
    ParserFnPtr fn;
    std::string name;
    char code;
    int8_t elemsize;
    SType stype;
    PT id;
    int32_t : 32;

    ParserInfo() : fn(nullptr), code(0), id(PT::Drop) {}
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
    const ParserLibrary& plib;
    const PT ptype;
    int64_t : 56;

  public:
    using iterator = ParserIterator;

    ParserIterable(PT pt, const ParserLibrary& pl);
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
    static const ParserInfo& info(int8_t i) { return parsers[i]; }
};


#endif
