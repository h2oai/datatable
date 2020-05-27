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
#include "read/chunk_coordinates.h"
#include "read/parse_context.h"
namespace dt {
namespace read {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

ParseContext::ParseContext()
  : ch(nullptr),
    eof(nullptr),
    target(nullptr),
    strbuf(),
    bytes_written(0),
    NAstrings(nullptr),
    whiteChar('\0'),
    dec('.'),
    sep(','),
    quote('"'),
    quoteRule(0),
    strip_whitespace(true),
    blank_is_na(false),
    cr_is_newline(true) {}


ParseContext::ParseContext(const ParseContext& o)
  : ch(o.ch),
    eof(o.eof),
    target(o.target),
    strbuf(),
    bytes_written(0),
    NAstrings(o.NAstrings),
    whiteChar(o.whiteChar),
    dec(o.dec),
    sep(o.sep),
    quote(o.quote),
    quoteRule(o.quoteRule),
    strip_whitespace(o.strip_whitespace),
    blank_is_na(o.blank_is_na),
    cr_is_newline(o.cr_is_newline) {}


ParseContext& ParseContext::operator=(const ParseContext& o) {
  ch = o.ch;
  eof = o.eof;
  target = o.target;
  // strbuf is not assigned, just "emptied"
  bytes_written = 0;
  NAstrings = o.NAstrings;
  whiteChar = o.whiteChar;
  dec = o.dec;
  sep = o.sep;
  quote = o.quote;
  quoteRule = o.quoteRule;
  strip_whitespace = o.strip_whitespace;
  blank_is_na = o.blank_is_na;
  cr_is_newline = o.cr_is_newline;
  return *this;
}




//------------------------------------------------------------------------------
// Parse helpers
//------------------------------------------------------------------------------

/**
 * skip_eol() is used to consume a "newline" token from the current parsing
 * location (`ch`). Specifically,
 *   (1) if there is a newline sequence at the current parsing position, this
 *       function advances the parsing position past the newline and returns
 *       true;
 *   (2) otherwise it returns false and the current parsing location remains
 *       unchanged.
 *
 * We recognize the following sequences as newlines (where "LF" is byte 0x0A
 * or '\\n', and "CR" is 0x0D or '\\r'):
 *     CR CR LF
 *     CR LF
 *     LF CR
 *     LF
 *     CR  (only if `cr_is_newline` is true)
 *
 * Here LF and CR-LF are the most commonly used line endings, while LF-CR and
 * CR are encountered much less frequently. The sequence CR-CR-LF is not
 * usually recognized as a single newline by most text editors. However we find
 * that occasionally a file with CR-LF endings gets recoded into CR-CR-LF line
 * endings by buggy software.
 *
 * In addition, CR (\\r) is treated specially: it is considered a newline only
 * when `cr_is_newline` is true. This is because it is common to find files
 * created by programs that don't account for '\\r's and fail to quote fields
 * containing these characters. If we were to treat these '\\r's as newlines,
 * the data would be parsed incorrectly. On the other hand, there are files
 * where '\\r's are used as valid newlines. In order to handle both of these
 * cases, we introduce parameter `cr_is_newline` which is set to false if there
 * is any '\\n' found in the file, in which case a standalone '\\r' will not be
 * considered a newline.
 */
bool ParseContext::skip_eol() const {
  if (ch < eof && *ch == '\n') {       // '\n\r' or '\n'
    ch += 1 + (ch + 1 < eof && ch[1] == '\r');
    return true;
  }
  if (ch < eof && *ch == '\r') {
    if (ch + 1 < eof && ch[1] == '\n') {   // '\r\n'
      ch += 2;
      return true;
    }
    if (ch + 2 < eof && ch[1] == '\r' && ch[2] == '\n') {  // '\r\r\n'
      ch += 3;
      return true;
    }
    if (cr_is_newline) {      // '\r'
      ch++;
      return true;
    }
  }
  return false;
}


/**
 * Return true iff the tokenizer's current position `ch` is a valid field
 * terminator (either a `sep` or a newline). This does not advance the tokenizer
 * position.
 */
bool ParseContext::at_end_of_field() const {
  // \r is 13, \n is 10, and \0 is 0. The second part is optimized based on the
  // fact that the characters in the ASCII range 0..13 are very rare, so a
  // single check `tch<=13` is almost equivalent to checking whether `tch` is
  // one of \r, \n, \0. We cast to unsigned first because `char` type is signed
  // by default, and therefore characters in the range 0x80-0xFF are negative.
  if (ch == eof) return true;
  char c = *ch;
  if (c == sep) return true;

  if (static_cast<uint8_t>(c) > 13) return false;
  if (c == '\n') return true;
  if (c == '\r') {
    if (cr_is_newline) return true;
    if ((ch + 1 < eof) && ch[1] == '\n') return true;
    if ((ch + 2 < eof) && ch[1] == '\r' && ch[2] == '\n') return true;
  }
  return false;
}



bool ParseContext::is_na_string(const char* start, const char* end) const {
  const char* const* nastr = NAstrings;
  while (*nastr) {
    const char* ch1 = start;
    const char* ch2 = *nastr;
    while (ch1 < end && *ch1 == *ch2 && *ch2) {
      ch1++;
      ch2++;
    }
    if (*ch2 == '\0' && ch1 == end) return true;
    nastr++;
  }
  return false;
}



const char* ParseContext::end_NA_string(const char* fieldStart) const {
  const char* const* nastr = NAstrings;
  const char* mostConsumed = fieldStart;
  while (*nastr) {
    const char* ch1 = fieldStart;
    const char* ch2 = *nastr;
    while (ch1 < eof && *ch1==*ch2 && *ch2!='\0') { ch1++; ch2++; }
    if (*ch2=='\0' && ch1>mostConsumed) mostConsumed=ch1;
    nastr++;
  }
  return mostConsumed;
}


/**
 * Skip whitespace at the beginning/end of a field.
 *
 * If `sep=' '` (Space), then whitespace shouldn't be skipped at all.
 * If `sep='\\t'` (Tab), then only ' ' characters are considered whitespace.
 * For all other seps we assume that both ' ' and '\\t' characters are
 * whitespace to be skipped.
 */
void ParseContext::skip_whitespace() const {
  // skip space so long as sep isn't space and skip tab so long as sep isn't tab
  if (whiteChar == 0) {   // whiteChar==0 means skip both ' ' and '\t';  sep is neither ' ' nor '\t'.
    while (ch < eof && (*ch == ' ' || *ch == '\t')) ch++;
  } else {
    while (ch < eof && *ch == whiteChar) ch++;  // sep is ' ' or '\t' so just skip the other one.
  }
}


/**
 * Skip whitespace at the beginning of a line. This whitespace does not count
 * as a separator even if `sep=' '`.
 */
void ParseContext::skip_whitespace_at_line_start() const {
  if (sep == '\t') {
    while (ch < eof && *ch == ' ') ch++;
  } else {
    while (ch < eof && (*ch == ' ' || *ch == '\t')) ch++;
  }
}


/**
 * Compute the number of fields on the current line (taking into account the
 * global `sep`, and `quoteRule`), and move the parsing location to the
 * beginning of the next line.
 * Returns the number of fields on the current line, or -1 if the line cannot
 * be parsed using current settings, or 0 if the line is empty (even though an
 * empty line may be viewed as a single field).
 */
int ParseContext::countfields() const {
  const char* ch0 = ch;
  if (sep==' ') while (ch < eof && *ch==' ') ch++;  // multiple sep==' ' at the start does not mean sep
  skip_whitespace();
  if (skip_eol() || ch==eof) {
    return 0;
  }
  int ncol = 1;
  while (ch < eof) {
    parse_string(*this);
    // Field() leaves *ch resting on sep, eol or eof
    if (ch < eof && *ch == sep) {
      if (sep == ' ') {
        while (ch < eof && *ch == ' ') ch++;
        if (ch == eof || skip_eol()) break;
        ncol++;
        continue;
      } else if (sep != '\n') {
        ch++;
        ncol++;
        continue;
      }
    }
    if (ch == eof || skip_eol()) {
      break;
    }
    ch = ch0;
    return -1;  // -1 means this line not valid for this sep and quote rule
  }
  return ncol;
}


// Find the next "good line", in the sense that we find at least 5 lines
// with `ncols` fields from that point on.
bool ParseContext::next_good_line_start(
  const ChunkCoordinates& cc, int ncols, bool fill, bool skipEmptyLines) const
{
  // int ncols = static_cast<int>(f.get_ncols());
  // bool fill = f.fill;
  // bool skipEmptyLines = f.skip_blank_lines;
  ch = cc.get_start();
  const char* end = cc.get_end();
  int attempts = 0;
  while (ch < end && attempts++ < 10) {
    while (ch < end && *ch != '\n' && *ch != '\r') ch++;
    if (ch == end) break;
    skip_eol();  // updates `ch`
    // countfields() below moves the parse location, so store it in `ch1` in
    // order to revert to the current parsing location later.
    const char* ch1 = ch;
    int i = 0;
    for (; i < 5; ++i) {
      // `countfields()` advances `ch` to the beginning of the next line
      int n = countfields();
      if (n != ncols &&
          !(ncols == 1 && n == 0) &&
          !(skipEmptyLines && n == 0) &&
          !(fill && n < ncols)) break;
    }
    ch = ch1;
    // `i` is the count of consecutive consistent rows
    if (i == 5) return true;
  }
  return false;
}




}}  // namespace dt::read::
