//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#ifndef dt_CSV_READER_PARSERS_H
#define dt_CSV_READER_PARSERS_H
#include <stdint.h>
#include <string>
#include <vector>
#include "csv/fread.h"


void parse_mu(FieldParseContext&);
void parse_bool8_numeric(FieldParseContext&);
void parse_bool8_uppercase(FieldParseContext&);
void parse_bool8_lowercase(FieldParseContext&);
void parse_bool8_titlecase(FieldParseContext&);
void parse_int32_simple(FieldParseContext&);
void parse_int64_simple(FieldParseContext&);
void parse_float32_hex(FieldParseContext&);
void parse_float64_simple(FieldParseContext& ctx);
void parse_float64_extended(FieldParseContext& ctx);
void parse_float64_hex(FieldParseContext&);
void parse_string(FieldParseContext&);


//------------------------------------------------------------------------------

enum class PT : uint8_t {
  Drop,
  Mu,
  BoolL,
  BoolT,
  BoolU,
  Bool01,
  Int32,
  Int64,
  Float32Plain,
  Float32Hex,
  Float64Plain,
  Float64Ext,
  Float64Hex,
  Str32,
};


class ParserInfo {
  public:
    ParserFnPtr fn;
    std::vector<PT> next_parsers;
    std::string name;
    char code;
    bool enabled;
    PT id;
    int64_t : 40;

    ParserInfo(PT id_, const char* name_, char code_, ParserFnPtr ptr)
        : fn(ptr), name(name_), code(code_), enabled(true), id(id_) {}
};


// Singleton class with information about various parsers
class ParserLibrary {
  private:
    std::vector<ParserInfo> parsers;

    ParserLibrary();
    void add(ParserInfo&& p);

  public:
    static ParserLibrary& get();
    ParserLibrary(const ParserLibrary&) = delete;
    void operator=(const ParserLibrary&) = delete;

    ParserInfo& operator[](size_t i) { return parsers[i]; }
    ~ParserLibrary() {}
};


#endif
