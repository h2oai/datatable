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


struct RelStr {
  int32_t length;
  int32_t offset;
};


struct FieldParseContext {
  // Pointer to the current parsing location
  const char*& ch;

  // Where to write the parsed value. The pointer will be incremented after
  // each successful read.
  union {
    int8_t  int8;
    int32_t int32;
    int64_t int64;
    uint8_t uint8;
    float   float32;
    double  float64;
    RelStr  str32;
  }* target;

  // Anchor pointer for string parser, this pointer is the starting point
  // relative to which `str32.offset` is defined.
  const char* anchor;
};


typedef void (*ParserFnPtr)(FieldParseContext& ctx);

void parser_Mu(FieldParseContext&);
void parser_Bool01(FieldParseContext&);
void parser_BoolU(FieldParseContext&);
void parser_BoolL(FieldParseContext&);
void parser_BoolT(FieldParseContext&);
void parser_Int32Plain(FieldParseContext&);
void parser_Int64Plain(FieldParseContext&);



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
