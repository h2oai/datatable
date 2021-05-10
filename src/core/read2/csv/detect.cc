//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#include "read2/_declarations.h"
namespace dt {
namespace read2 {

struct ScanContext;
static int parseLine(ScanContext&);
static int parseNormal(ScanContext&);
static int parseQuoted(ScanContext&);


//------------------------------------------------------------------------------
// Helper structures
//------------------------------------------------------------------------------

// Possible return values from various parse* functions.
enum : int {
  END_OF_DATA,
  END_OF_LINE,
  END_OF_STRING,
  INVALID_STRING,
  NEWLINE,
  QUOTE,
  START_OVER,
};


// Autodetection: if *only* '\r's are encountered, then use ANY mode;
// otherwise use the NOCR mode.
enum class NewlineKind : int8_t {
  AUTO,   // auto-detect: either ANY or NOCR
  NOCR,   // \n | \r\n
  ANY,    // \n | \r | \r\n
  LF,     // \n
  CR,     // \r
  CRLF,   // \r\n
};


enum QuoteRule : int8_t {
  AUTO,
  ESCAPED,
  DOUBLED
};


struct ScanContext {
  const char* ch;
  const char* eof;
  int counts[];
  bool eofExact;

  NewlineKind newline;
  QuoteRule qr;
  char quoteChar;
  bool autoNewline;
  bool autoQuoteRule;
  bool autoQuoteChar;
  bool singleQuoteForbidden;
  bool doubleQuoteForbidden;
};



//------------------------------------------------------------------------------
// Parsing
//------------------------------------------------------------------------------

// Parse the input until one of the special characters is encountered.
// Special characters are: '\n', '\r', '"', '\''. The tally of
// all ASCII characters scanned is collected in array `counts[]`,
// which must have size 128.
//
// This function will stop at the first encountered special character,
// end return one of the following statuses:
//    - NEWLINE
//    - QUOTE
//    - END_OF_DATA (eof reached)
//
static int parseNormal(ScanContext& ctx) {
  const char*& ch = ctx.ch;
  const char* eof = ctx.eof;
  while (ch < eof) {
    char c = *ch;
    if (c >= 0) {
      if (c == '\n') return NEWLINE;
      if (c == '\r') return NEWLINE;
      if (c == '\'') return QUOTE;
      if (c == '"')  return QUOTE;
      ctx.counts[c]++;
    }
    ch++;
  }
  return END_OF_DATA;
}


// Parse a quoted field (first quote character was already skipped).
//
// Possible return statuses:
//    - INVALID_STRING
//    - END_OF_STRING
//    - END_OF_DATA
//
// In case of success (END_OF_STRING), `ctx.ch` is moved to the next
// character after the closing quote.
//
static int parseQuoted(ScanContext& ctx) {
  const char quote = opts.quoteChar;
  const char*& ch = ctx.ch;
  const char* sof = ch;
  const char* eof = ctx.eof;
  while (ch < eof) {
    char c = *ch++;
    if (c == quote) {
      bool nextCharIsAlsoQuote = (ch < eof) && (*ch == quote);
      if (nextCharIsAlsoQuote) {
        if (opts.quoteRule == QuoteRule::AUTO) {
          opts.quoteRule = QuoteRule::DOUBLED;
        }
        if (opts.quoteRule == QuoteRule::ESCAPED) {
          ch = sof;
          return INVALID_STRING;
        }
        ch++;  // skip the second quote char
      } else if (ch == eof && opts.quoteRule != QuoteRule::ESCAPED && !opts.eofExact) {
        // Last character in the data chunk is '"', which could potentially be a
        // doubled quote if only we could see the next byte... Return END_OF_DATA
        // status to request more data.
        return END_OF_DATA;
      } else {
        return END_OF_STRING;
      }
    }
    if (c == '\\') {
      switch (opts.quoteRule) {
        case QuoteRule::DOUBLED: break;
        case QuoteRule::ESCAPED: {
          ch++;   // skip the next character
          break;
        }
        case QuoteRule::AUTO: {
          // Only r"\'" sequence triggers detection of the quote rule
          bool nextCharIsQuote = (ch < eof) && (*ch == quote);
          bool nextNextCharIsQuote = (ch + 1 < eof) && (*ch == quote);
          if (nextCharIsQuote) {
            // Sequence r'\""' will be interpreted as DOUBLED rule, not
            // ESCAPED
            if (nextNextCharIsQuote) {
              opts.quoteRule = QuoteRule::DOUBLED;
              ch++;
            } else {
              opts.quoteRule = QuoteRule::ESCAPED;
            }
          }
          ch++;  // skip the next character regardless
          break;
        }
      }
    }
  }
  if (opts.eofExact) {
    ch = sof;
    return INVALID_STRING;
  } else {
    ch = eof;
    return END_OF_DATA;
  }
}


// Possible return statuses:
//   - END_OF_LINE  (normal)
//   - END_OF_DATA  (eof reached before the end of line)
//   - START_OVER   (all parsing must restart)
//
static int parseLine(ScanContext& ctx) {
  const char*& ch = ctx.ch;
  const char* eof = ctx.eof;
  while (ch < eof) {
    const int ret = scanNormal(ctx);
    xassert((ret == END_OF_DATA)? (ch == eof) : (ch < eof));
    if (ret == NEWLINE) {
      char newlineChar = *ch++;
      if (newlineChar == '\n') {
        if (opts.autoNewline && opts.newline == NewlineKind.ANY) {
          // Previously we saw a \r character, and assumed it was a newline.
          // But now that we encountered \n, we realize that that assumption
          // was wrong, and thus we need to restart all parsing.
          opts.newline = NewlineKind::NOCR;
          return START_OVER;
        }
        switch (opts.newline) {
          case NewlineKind::AUTO: opts.newline = NewlineKind::NOCR;  FALLTHROUGH
          case NewlineKind::ANY:
          case NewlineKind::NOCR:
          case NewlineKind::LF:   return END_OF_LINE;
          case NewlineKind::CR:
          case NewlineKind::CRLF: break;  // ignore this newline
        }
      }
      else {
        xassert(newlineChar == '\r');
        bool crlf = (ch < eof && *ch == '\n');  // is this a '\r\n' combo?
        switch (opts.newline) {
          case NewlineKind::AUTO: {
            opts.newline = crlf? NewlineKind::NOCR : NewlineKind::ANY;
            FALLTHROUGH
          }
          case NewlineKind::ANY: {
            ch += crlf;
            return END_OF_LINE;
          }
          case NewlineKind::NOCR:
          case NewlineKind::CRLF: {
            if (crlf) {
              ch++;
              return END_OF_LINE;
            }
            break;
          }
          case NewlineKind::CR: return END_OF_LINE;
          case NewlineKind::LF: break;
        }
      }
    }
    if (ret == QUOTE) {
      char quote = *ch++;
      xassert(quote == '"' || quote == '\'');
      if (opts.autoQuoteChar) {
        if (quote != opts.quoteChar) {
          if (opts.quoteChar) continue;
          bool forbidden = (quote == '"')?  opts.doubleQuoteForbidden :
                           (quote == '\'')? opts.singleQuoteForbidden : false;
          if (forbidden) continue;
          opts.quoteChar = quote;
        }
      }
      if (quote == opts.quoteChar) {
        int r = scanQuoted(opts, &ch, eof);
        // scan quoted field
      }
    }
  }
  return ctx.eofExact? END_OF_LINE : END_OF_DATA;
}



}}  // namespace dt::read2
