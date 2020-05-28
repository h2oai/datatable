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
#ifndef dt_READ_PARSE_CONTEXT_h
#define dt_READ_PARSE_CONTEXT_h
#include "buffer.h"
#include "_dt.h"
namespace dt {
namespace read {


/**
  * This struct carries information needed by various field parsers
  * to correctly read their values. Different values may be used by
  * different parsers, some may not need any extra values at all.
  *
  * The most important variables, however, which are used by all
  * parsers, are: `ch`, `eof` and `target`.
  */
struct ParseContext
{
  // Pointer to the current parsing location within the input stream.
  // All parsers are expected to advance this pointer when they
  // successfully read a value from the stream.
  mutable const char* ch;

  // The end of the range of bytes available for reading. Only bytes
  // up to but excluding `eof` may be accessed by a parser.
  const char* eof;

  // Where to write the parsed value.
  mutable field64* target;

  // Buffer where string parser will be saving its data. Theoretically,
  // some other parsers may store their values in here too.
  mutable Buffer strbuf;
  mutable size_t bytes_written;

  // TODO: remove from here
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

  // Whether to consider a standalone '\r' a newline character
  bool cr_is_newline;


  public:
    ParseContext();
    ParseContext(const ParseContext& o);
    ParseContext& operator=(const ParseContext& o);

    void skip_whitespace() const;
    void skip_whitespace_at_line_start() const;
    bool at_end_of_field() const;
    const char* end_NA_string(const char*) const;
    bool is_na_string(const char* start, const char* end) const;
    int countfields() const;
    bool skip_eol() const;

    bool next_good_line_start(
      const ChunkCoordinates& cc, int ncols, bool fill,
      bool skipEmptyLines) const;
};



}}  // namespace dt::read::
#endif
