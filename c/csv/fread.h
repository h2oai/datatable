//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_FREAD_h
#define dt_FREAD_h
#include <stdint.h>  // uint32_t
#include <stdlib.h>  // size_t
#include "utils.h"
#include "utils/parallel.h"
#include "memrange.h"
#include "csv/reader.h"
#include "read/field64.h"

extern const long double pow10lookup[701];
extern const uint8_t hexdigits[256];
extern const uint8_t allowedseps[128];
namespace dt {
namespace read {
  struct ChunkCoordinates;
}}


struct FreadTokenizer {
  // Pointer to the current parsing location
  const char* ch;

  // Where to write the parsed value. The pointer will be incremented after
  // each successful read.
  dt::read::field64* target;

  // Anchor pointer for string parser, this pointer is the starting point
  // relative to which `str32.offset` is defined.
  const char* anchor;

  const char* eof;

  const char* const* NAstrings;

  // what to consider as whitespace to skip: ' ', '\t' or 0 means both
  // (when sep!=' ' && sep!='\t')
  char whiteChar;

  // Decimal separator for parsing floats. The default value is '.', but
  // in some cases ',' may also be used.
  char dec;

  // Field separator
  char sep;

  // Character used for field quoting.
  char quote;

  // How the fields are quoted.
  // TODO: split quoteRule differences into separate parsers.
  int8_t quoteRule;

  // Should white space be removed?
  bool strip_whitespace;

  // Do we consider blank as NA string?
  bool blank_is_na;

  bool LFpresent;

  void skip_whitespace();
  void skip_whitespace_at_line_start();
  bool end_of_field();
  const char* end_NA_string(const char*);
  int countfields();
  bool skip_eol();
  bool at_eof() const { return ch == eof; }

  bool next_good_line_start(
    const dt::read::ChunkCoordinates& cc, int ncols, bool fill, bool skipEmptyLines);


};


#endif
