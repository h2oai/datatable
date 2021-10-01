//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#include <cstring>                       // std::memcpy
#include <iostream>
#include "_dt.h"
#include "encodings.h"
#include "py_encodings.h"
#include "read/field64.h"                // field64
#include "read/parse_context.h"          // ParseContext
#include "read/parsers/info.h"
#include "utils/assert.h"
namespace dt {
namespace read {


static constexpr int SIMPLE = 0;
static constexpr int DOUBLED = 1;
static constexpr int ESCAPED = 2;


//------------------------------------------------------------------------------
// helper functions
//------------------------------------------------------------------------------

static void save_plain_string(const ParseContext& ctx,
                              const char* start, const char* end)
{
  xassert(end >= start);
  auto len = static_cast<size_t>(end - start);
  auto pos = ctx.bytes_written;
  if (len) {
    ctx.strbuf.ensuresize(pos + len);
    std::memcpy(ctx.strbuf.xptr(pos), start, len);
    ctx.bytes_written += len;
  }
  ctx.target->str32.offset = static_cast<uint32_t>(pos);
  ctx.target->str32.length = static_cast<int32_t>(len);
}



static void write_utf8_codepoint(int32_t cp, char** dest) {
  uint8_t* ch = reinterpret_cast<uint8_t*>(*dest);
  if (cp <= 0x7F) {
    *ch++ = static_cast<uint8_t>(cp);
  } else if (cp <= 0x7FF) {
    *ch++ = 0xC0 | static_cast<uint8_t>(cp >> 6);
    *ch++ = 0x80 | static_cast<uint8_t>(cp & 0x3F);
  } else if (cp <= 0xFFFF) {
    *ch++ = 0xE0 | static_cast<uint8_t>(cp >> 12);
    *ch++ = 0x80 | static_cast<uint8_t>((cp >> 6) & 0x3F);
    *ch++ = 0x80 | static_cast<uint8_t>(cp & 0x3F);
  } else {
    *ch++ = 0xF0 | static_cast<uint8_t>(cp >> 18);
    *ch++ = 0x80 | static_cast<uint8_t>((cp >> 12) & 0x3F);
    *ch++ = 0x80 | static_cast<uint8_t>((cp >> 6) & 0x3F);
    *ch++ = 0x80 | static_cast<uint8_t>(cp & 0x3F);
  }
  *dest = reinterpret_cast<char*>(ch);
}



template <int MODE>
static void save_unescaped_string(const ParseContext& ctx,
                                  const char* start, const char* end)
{
  xassert(end >= start);
  char quote = ctx.quote;
  auto pos = ctx.bytes_written;
  ctx.strbuf.ensuresize(pos + static_cast<size_t>(end - start));
  char* dest = static_cast<char*>(ctx.strbuf.xptr(pos));
  char* dest0 = dest;

  const char* src = start;
  while (src < end) {
    start_loop:
    char c = *src++;
    if (MODE == DOUBLED) {
      if (c == quote) src++;
      *dest++ = c;
    }
    if (MODE == ESCAPED) {
      if (c == '\\') {
        c = *src++;
        switch (c) {
          case 'a': *dest++ = '\a'; break;
          case 'b': *dest++ = '\b'; break;
          case 'f': *dest++ = '\f'; break;
          case 'n': *dest++ = '\n'; break;
          case 'r': *dest++ = '\r'; break;
          case 't': *dest++ = '\t'; break;
          case 'v': *dest++ = '\v'; break;
          case '0': case '1': case '2': case '3':
          case '4': case '5': case '6': case '7': {
            // Octal escape sequence
            uint8_t chd = static_cast<uint8_t>(c - '0');
            int32_t v = static_cast<int32_t>(chd);
            if (src < end && (chd = static_cast<uint8_t>(*src-'0')) <= 7) {
              v = v * 8 + chd;
              src++;
            }
            if (src < end && (chd = static_cast<uint8_t>(*src-'0')) <= 7) {
              v = v * 8 + chd;
              src++;
            }
            write_utf8_codepoint(v, &dest);
            break;
          }
          case 'x': case 'u': case 'U': {
            // Hex-sequence
            int32_t v = 0;
            int nhexdigits = (c == 'x')? 2 : (c == 'u')? 4 : 8;
            if (src + nhexdigits <= end) {
              for (int i = 0; i < nhexdigits; i++) {
                c = *src++;
                if      (static_cast<uint8_t>(c - '0') < 10) v = v*16 + (c - '0');
                else if (static_cast<uint8_t>(c - 'A') < 6)  v = v*16 + (c - 'A' + 10);
                else if (static_cast<uint8_t>(c - 'a') < 6)  v = v*16 + (c - 'a' + 10);
                else {
                  *dest++ = '\\';
                  src -= i + 2;
                  goto start_loop;
                }
              }
              write_utf8_codepoint(v, &dest);
            } else {
              *dest++ = '\\';
              src--;
            }
            break;
          }
          default: {
            *dest++ = c;
            break;
          }
        }
      } else {
        *dest++ = c;
      }
    }
  }
  ctx.bytes_written += static_cast<size_t>(dest - dest0);
  ctx.target->str32.offset = static_cast<uint32_t>(pos);
  ctx.target->str32.length = static_cast<int32_t>(dest - dest0);
}




//------------------------------------------------------------------------------
// Parsers
//------------------------------------------------------------------------------

/**
  * Parse simple unquoted string field. The field terminates when we
  * encounter either sep or a newline.
  *
  * The QUOTES_FORBIDDEN flag controls the meaning of the quotes
  * found inside the field. If the flag is true, then any quotes will
  * result in the error condition (target set to NA, `ch` not
  * advanced); if the flag is false then the quote chars are treated
  * as any other regular character.
  *
  * This function
  *   - WILL NOT check for NA strings;
  *   - WILL NOT check for UTF8 validity;
  *   - WILL strip the leading/trailing whitespace if requested.
  */
template <bool QUOTES_FORBIDDEN>
static void parse_string_unquoted(const ParseContext& ctx) {
  const char* ch = ctx.ch;
  const char* end = ctx.eof;
  const char quote = ctx.quote;
  const char sep = ctx.sep;

  if (ctx.strip_whitespace) {
    while (ch < end && *ch == ' ') ch++;
  }
  const char* field_start = ch;
  while (ch < end) {
    char c = *ch;
    if (c == sep) break;  // end of field
    if (static_cast<uint8_t>(c) <= 13) {  // probably a newline
      if (c == '\n') {
        // Move back to the beginning of \r+\n sequence
        while (ch > field_start && ch[-1] == '\r') ch--;
        break;
      }
      if (c == '\r' && ctx.cr_is_newline) break;
    }
    else if (c == quote && QUOTES_FORBIDDEN) {
      ctx.target->str32.setna();
      return;
    }
    ch++;
  }
  // end of field reached
  auto field_size = ch - field_start;
  if (ctx.strip_whitespace) {
    const char* fch = ch - 1;
    while (field_size > 0 && *fch == ' ') {
      fch--;
      field_size--;
    }
  }
  save_plain_string(ctx, field_start, field_start + field_size);
  ctx.ch = ch;
}



/**
  * Parse a "quoted" string, this function handles QRs 1 and 2. More
  * precisely, if the current field begins with a quote, then it will
  * be parsed as a quoted field, using the supplied QR MODE. However,
  * if the field does not start with a quote, then we will defer to
  * the `parse_string_unquoted<false>()` function instead.
  *
  * The following MODEs are supported:
  *   - SIMPLE: no quotes inside field are allowed,
  *   - DOUBLED: any quotes inside the field are doubled,
  *   - ESCAPED: any quotes inside the field are escaped with a
  *              backslash.
  */
template <int MODE>
static void parse_string_quoted(const ParseContext& ctx) {
  const char* ch = ctx.ch;
  const char* end = ctx.eof;
  const char quote = ctx.quote;

  if (ctx.strip_whitespace) {
    while (ch < end && *ch == ' ') ch++;
  }
  if (ch < end && *ch == quote) {
    ch++;  // skip the quote symbol
    const char* field_start = ch;
    size_t n_escapes = 0;
    while (ch < end) {
      if (*ch == quote) {
        if (MODE == DOUBLED && ch + 1 < end && ch[1] == quote) {
          ch++;
          n_escapes++;
        } else {
          break;  // undoubled quote: end of field
        }
      }
      if (MODE == ESCAPED && *ch == '\\') {
        ch++;
        n_escapes++;
      }
      ch++;
    }
    if (ch >= end) {
      ctx.target->str32.setna();
      return;
    }
    if (MODE != SIMPLE && n_escapes) {
      save_unescaped_string<MODE>(ctx, field_start, ch);
    } else {
      save_plain_string(ctx, field_start, ch);
    }

    xassert(*ch == quote);
    ch++;  // skip over the quote
    if (ctx.strip_whitespace) {
      while (ch < end && *ch == ' ') ch++;
    }
    ctx.ch = ch;
  }
  else {
    parse_string_unquoted<true>(ctx);
  }
}



/**
  * Parse a "naively" quoted string. This quoting rule means that
  * the string which potentially has embedded quotes was written
  * without any consideration for the need to escape inner quote
  * marks. Such string is obviously broken. This parse rule attempts
  * to fix this situation by following a heuristic:
  *
  *   - assume the field has no newlines;
  *   - the field may or may not contain quote marks;
  *   - if the field contains a quote mark, it is not followed
  *     by the sep;
  *   - when we see quote+(sep|eol) in the input, it means this is
  *     the actual field end; any quote that is not followed by
  *     either sep or eol is assumed to be part of the field;
  *   - if the input starts and ends with a quote, those quotes are
  *     not considered part of the field;
  *   - if the input starts with a quote but doesn't end with a
  *     quote, then the first quote is presumed to be part of the
  *     field.
  *
  * Note: this parser is very hacky, and might as well be removed
  * in the future entirely.
  */
static void parse_string_naive(const ParseContext& ctx) {
  const char* ch = ctx.ch;
  const char* end = ctx.eof;
  const char quote = ctx.quote;
  const char sep = ctx.sep;

  if (ctx.strip_whitespace) {
    while (ch < end && *ch == ' ') ch++;
  }
  const char* field_end = nullptr;
  const char* field_start = ch;
  bool quoted = false;
  if (ch < end && *ch == quote) {
    quoted = true;
    ch++;
  }
  while (ch < end) {
    char c = *ch;
    if (c == sep) {
      // this is a field end if either (1) the field did not start with a quote,
      // or (2) a matching closing quote will not be found on the line
      if (!field_end) field_end = ch;  // tentative
      if (!quoted) break;
    }
    else if (c == quote && quoted) {
      // a quote closes the field only if the field started with a quote, and
      // only if this quote is followed with a valid sep
      if (ch + 1 == end || ch[1] == sep || ch[1] == '\n' || ch[1] == '\r') {
        field_end = ch;
        field_start++;
        ch++;  // skip over the final quote
        break;
      }
    }
    else if (static_cast<uint8_t>(c) <= 13) {  // probably a newline
      if (c == '\n') {
        // Move back to the beginning of \r+\n sequence
        while (ch > field_start && ch[-1] == '\r') ch--;
        break;
      }
      if (c == '\r' && ctx.cr_is_newline) break;
    }
    ch++;
  }
  if (!field_end) field_end = ch;
  ctx.ch = ch;
  save_plain_string(ctx, field_start, field_end);
}





void parse_string(const ParseContext& ctx) {
  switch (ctx.quoteRule) {
    case 0: parse_string_quoted<DOUBLED>(ctx); break;
    case 1: parse_string_quoted<ESCAPED>(ctx); break;
    case 2: parse_string_unquoted<false>(ctx); break;
    case 3: parse_string_naive(ctx);           break;
    default: throw RuntimeError()
        << "Invalid quoteRule: " << static_cast<int>(ctx.quoteRule);
  }
  auto len = ctx.target->str32.length;
  auto off = ctx.target->str32.offset;
  auto ptr = static_cast<const char*>(ctx.strbuf.rptr(off));
  xassert(len >= 0 || ctx.target->str32.isna());
  if (len == 0) {
    if (ctx.blank_is_na) {
      ctx.target->str32.setna();
    }
  }
  else if (len > 0) {
    if (ctx.is_na_string(ptr, ptr + len)) {
      ctx.target->str32.setna();
    }
  }
  if (!ctx.target->str32.isna()) {
    auto zlen = static_cast<size_t>(len);
    if (!is_valid_utf8(ptr, zlen)) {
      ctx.strbuf.ensuresize(ctx.bytes_written + zlen * 3);
      auto newoff = ctx.bytes_written;
      auto ptr0 = static_cast<char*>(ctx.strbuf.xptr());
      auto newlen = decode_win1252(ptr0 + off, len, ptr0 + newoff);
      xassert(newlen >= 0);
      ctx.bytes_written += static_cast<size_t>(newlen);
      ctx.target->str32.length = static_cast<int32_t>(newlen);
      ctx.target->str32.offset = static_cast<uint32_t>(newoff);
    }
  }
}


REGISTER_PARSER(PT::Str32)
    ->parser(parse_string)
    ->name("Str32")
    ->code('s')
    ->type(Type::str32())
    ->successors({});




}}  // namespace dt::read
