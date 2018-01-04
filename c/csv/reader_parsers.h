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
    float   float32;
    double  float64;
    RelStr  str32;
  }* target;

  // Anchor pointer for string parser, this pointer is the starting point
  // relative to which `str32.offset` is defined.
  const char* anchor;
};


typedef void (*parser_fn)(FieldParseContext& ctx);


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


#undef
