//------------------------------------------------------------------------------
// Copyright 2017 data.table authors
// (https://github.com/Rdatatable/data.table/DESCRIPTION)
//
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v.2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//------------------------------------------------------------------------------
#include "csv/fread.h"
#include "csv/freadLookups.h"
#include "csv/reader.h"
#include "csv/reader_fread.h"
#ifdef WIN32             // means WIN64, too, oddly
  #include <windows.h>
#else
  #include <ctype.h>     // isspace
  #include <errno.h>     // errno
  #include <string.h>    // strerror
  #include <stdarg.h>    // va_list, va_start
  #include <stdio.h>     // vsnprintf
  #include <math.h>      // ceil, sqrt, isfinite
#endif
#include <stdbool.h>     // bool, true, false
#include <string>        // std::string
#include "utils/file.h"


// Private globals to save passing all of them through to highly iterated field processors
static char sep;
static char whiteChar; // what to consider as whitespace to skip: ' ', '\t' or 0 means both (when sep!=' ' && sep!='\t')
static char quote, dec;
static const char* eof;

// Quote rule:
//   0 = Fields may be quoted, any quote inside the field is doubled. This is
//       the CSV standard. For example: <<...,"hello ""world""",...>>
//   1 = Fields may be quoted, any quotes inside are escaped with a backslash.
//       For example: <<...,"hello \"world\"",...>>
//   2 = Fields may be quoted, but any quotes inside will appear verbatim and
//       not escaped in any way. It is not always possible to parse the file
//       unambiguously, but we give it a try anyways. A quote will be presumed
//       to mark the end of the field iff it is followed by the field separator.
//       Under this rule EOL characters cannot appear inside the field.
//       For example: <<...,"hello "world"",...>>
//   3 = Fields are not quoted at all. Any quote characters appearing anywhere
//       inside the field will be treated as any other regular characters.
//       Example: <<...,hello "world",...>>
//
static int quoteRule;
static const char* const* NAstrings;
static bool any_number_like_NAstrings = false;
static bool blank_is_a_NAstring = false;
static bool stripWhite = true;  // only applies to character columns; numeric fields always stripped
static bool skipEmptyLines = false;
static bool fill = false;
static bool LFpresent = false;

#define JUMPLINES 100    // at each of the 100 jumps how many lines to guess column types (10,000 sample lines)

const char typeSymbols[NUMTYPE]  = {'x',    'b',     'b',     'b',     'b',     'i',     'I',     'h',       'd',       'D',       'H',       's'};
const char typeName[NUMTYPE][10] = {"drop", "bool8", "bool8", "bool8", "bool8", "int32", "int64", "float32", "float64", "float64", "float64", "string"};
int8_t     typeSize[NUMTYPE]     = { 0,      1,      1,        1,       1,       4,       8,      4,         8,         8,         8,         8       };

// NAN and INFINITY constants are float, so cast to double once up front.
static double NA_FLOAT64;  // takes fread.h:NA_FLOAT64_VALUE
static float NA_FLOAT32;
static const double NAND = (double)NAN;
static const double INFD = (double)INFINITY;


typedef struct FieldParseContext {
  // Pointer to the current parsing location
  const char*& ch;

  // Parse target buffer
  field64* target;

  // String "anchor" for `Field()` parser -- the difference `ch - anchor` will
  // be written out as the string offset.
  const char* anchor;
} FieldParseContext;


// Forward declarations
static void parse_string(FieldParseContext&);
void parser_Bool01(FieldParseContext&);
void parser_BoolU(FieldParseContext&);
void parser_BoolL(FieldParseContext&);
void parser_BoolT(FieldParseContext&);



//=================================================================================================
//
//   Utility functions
//
//=================================================================================================

/**
 * Free any resources / memory buffers allocated by the fread() function, and
 * bring all global variables to a "clean slate". This function must always be
 * executed when fread() exits, either successfully or not.
 */
void FreadReader::freadCleanup(void)
{
  sep = whiteChar = quote = dec = '\0';
  quoteRule = -1;
  any_number_like_NAstrings = false;
  blank_is_a_NAstring = false;
  stripWhite = true;
  skipEmptyLines = false;
  fill = false;
  // following are borrowed references: do not free
  NAstrings = NULL;
}

#define ASSERT(test) do { \
  if (!(test)) \
    STOP("Assertion violation at line %d, please report at " \
         "https://github.com/h2oai/datatable", __LINE__); \
} while(0)

#define CEIL(x)  ((size_t)(double)ceil(x))
static inline size_t umax(size_t a, size_t b) { return a > b ? a : b; }
static inline int imin(int a, int b) { return a < b ? a : b; }

/** Return value of `x` clamped to the range [upper, lower] */
static inline size_t clamp_szt(size_t x, size_t lower, size_t upper) {
  return x < lower ? lower : x > upper? upper : x;
}

/**
 * eol() accepts a position and, if any of the following line endings, moves to the end of that sequence
 * and returns true. Repeated \\r are considered one. At most one \\n will be moved over.
 * 1. \\n        Unix
 * 2. \\r\\n     Windows
 * 3. \\r\\r\\n  R's download.file() in text mode doubling up \\r; also some email programs mangling the attached files
 * 4. \\r        Old MacOS 9 format discontinued in 2002 but then #2347 was raised straight away when I tried not to support it
 * 5. \\n\\r     Acorn BBC (!) and RISC OS according to Wikipedia.
 */
// TODO: change semantics so that eol() skips over the newline (rather than stumbles upon the last character)
static inline bool eol(const char** pch) {
  // we call eol() when we expect to be at a newline, so optimize as if we are at the end of line
  const char* ch = *pch;
  if (*ch=='\n') {
    *pch += (ch[1]=='\r');  // 1 & 5
    return true;
  }
  if (*ch=='\r') {
    if (LFpresent) {
      // \n is present in the file, so standalone \r is NOT considered a newline.
      // Thus, we attempt to match a sequence '\r+\n' here
      while (*ch=='\r') ch++;  // consume multiple \r
      if (*ch=='\n') {
        *pch = ch;
        return true;
      } else {
        // 1 or more \r's were not followed by \n -- do not consider this a newline
        return false;
      }
    } else {
      // \n does not appear anywhere in the file: \r is a newline
      *pch = ch;
      return true;
    }
  }
  return false;
}


/**
 * Return True iff `ch` is a valid field terminator character: either a field
 * separator or a newline.
 */
static inline bool end_of_field(const char* ch) {
  // \r is 13, \n is 10, and \0 is 0. The second part is optimized based on the
  // fact that the characters in the ASCII range 0..13 are very rare, so a
  // single check `ch<=13` is almost equivalent to checking whether `ch` is one
  // of \r, \n, \0. We cast to unsigned first because `char` type is signed by
  // default, and therefore characters in the range 0x80-0xFF are negative.
  // We use eol() because that looks at LFpresent inside it w.r.t. \r
  return *ch==sep || ((uint8_t)*ch<=13 && (*ch=='\0' || eol(&ch)));
}


static inline const char* end_NA_string(const char* fieldStart) {
  const char* const* nastr = NAstrings;
  const char* mostConsumed = fieldStart; // tests 1550* includes both 'na' and 'nan' in nastrings. Don't stop after 'na' if 'nan' can be consumed too.
  while (*nastr) {
    const char* ch1 = fieldStart;
    const char* ch2 = *nastr;
    while (*ch1==*ch2 && *ch2!='\0') { ch1++; ch2++; }
    if (*ch2=='\0' && ch1>mostConsumed) mostConsumed=ch1;
    nastr++;
  }
  return mostConsumed;
}


/**
 * Helper for error and warning messages to extract an input line starting at
 * `*ch` and until an end of line, but no longer than `limit` characters.
 * This function returns the string copied into an internal static buffer. Cannot
 * be called more than twice per single printf() invocation.
 * Parameter `limit` cannot exceed 500.
 */
static const char* strlim(const char* ch, size_t limit) {
  static char buf[1002];
  static int flip = 0;
  char* ptr = buf + 501 * flip;
  flip = 1 - flip;
  char* ch2 = ptr;
  size_t width = 0;
  while ((*ch>'\r' || (*ch!='\0' && *ch!='\r' && *ch!='\n')) && width++<limit) *ch2++ = *ch++;
  *ch2 = '\0';
  return ptr;
}


const char* FreadReader::printTypes(int ncol) const {
  // e.g. files with 10,000 columns, don't print all of it to verbose output.
  static char out[111];
  char* ch = out;
  if (types) {
    int tt = ncol<=110? ncol : 90;
    for (int i=0; i<tt; i++) {
      *ch++ = typeSymbols[types[i]];
    }
    if (ncol>110) {
      *ch++ = '.';
      *ch++ = '.';
      *ch++ = '.';
      for (int i=ncol-10; i<ncol; i++)
        *ch++ = typeSymbols[types[i]];
    }
  }
  *ch = '\0';
  return out;
}


static inline void skip_white(const char** pch) {
  // skip space so long as sep isn't space and skip tab so long as sep isn't tab
  const char* ch = *pch;
  if (whiteChar == 0) {   // whiteChar==0 means skip both ' ' and '\t';  sep is neither ' ' nor '\t'.
    while (*ch == ' ' || *ch == '\t') ch++;
  } else {
    while (*ch == whiteChar) ch++;  // sep is ' ' or '\t' so just skip the other one.
  }
  *pch = ch;
}


/**
 * Compute the number of fields on the current line (taking into account the
 * global `sep`, and `quoteRule`), and move the parsing location to the
 * beginning of the next line.
 * Returns the number of fields on the current line, or -1 if the line cannot
 * be parsed using current settings, or 0 if the line is empty (even though an
 * empty line may be viewed as a single field).
 */
static inline int countfields(const char** pch)
{
  field64 trash;
  const char* ch = *pch;
  FieldParseContext ctx = {
    .ch = ch,
    .target = &trash,
    .anchor = NULL,
  };

  if (sep==' ') while (*ch==' ') ch++;  // multiple sep==' ' at the start does not mean sep
  skip_white(&ch);
  if (eol(&ch) || ch==eof) {
    *pch = ch + 1;
    return 0;
  }
  int ncol = 1;
  while (ch < eof) {
    parse_string(ctx);
    // Field() leaves *ch resting on sep, \r, \n or *eof=='\0'
    if (sep==' ' && *ch==sep) {
      while (ch[1]==' ') ch++;
      if (ch[1]=='\r' || ch[1]=='\n' || ch[1]=='\0') {
        // reached end of line. Ignore padding spaces at the end of line.
        ch++;  // Move onto end of line character
      }
    }
    if (*ch==sep && sep!='\n') {
      ch++;
      ncol++;
      continue;
    }
    if (eol(&ch)) {
      *pch = ch + 1;
      return ncol;
    }
    if (*ch!='\0') return -1;  // -1 means this line not valid for this sep and quote rule
    break;
  }
  *pch = ch;
  return ncol;
}


static inline bool nextGoodLine(const char** pch, int ncol)
{
  const char* ch = *pch;
  // we may have landed inside quoted field containing embedded sep and/or embedded \n
  // find next \n and see if 5 good lines follow. If not try next \n, and so on, until we find the real \n
  // We don't know which line number this is, either, because we jumped straight to it. So return true/false for
  // the line number and error message to be worked out up there.
  int attempts = 0;
  while (ch<eof && attempts++<30) {
    while (*ch!='\0' && *ch!='\n' && *ch!='\r') ch++;
    if (*ch=='\0') return false;
    eol(&ch);  // move to last byte of the line ending sequence
    ch++;      // move to first byte of next line
    int i = 0;
    const char* ch2 = ch;
    while (i<5 && countfields(&ch2)==ncol) i++;
    if (i==5) break;
  }
  if (*ch!='\0' && attempts<30) { *pch = ch; return true; }
  return false;
}



//=================================================================================================
//
//   Field parsers
//
//=================================================================================================

static void parse_string(FieldParseContext& ctx)
{
  const char* ch = ctx.ch;

  // need to skip_white first for the reason that a quoted field might have space before the
  // quote; e.g. test 1609. We need to skip the space(s) to then switch on quote or not.
  if (*ch==' ' && stripWhite) while(*++ch==' ');  // if sep==' ' the space would have been skipped already and we wouldn't be on space now.
  const char* fieldStart = ch;
  if (*ch!=quote || quoteRule==3) {
    // Most common case. Unambiguously not quoted. Simply search for sep|eol. If field contains sep|eol then it should have been quoted and we do not try to heal that.
    while(!end_of_field(ch)) ch++;  // sep, \r, \n or \0 will end
    ctx.ch = ch;
    int fieldLen = (int)(ch-fieldStart);
    if (stripWhite) {   // TODO:  do this if and the next one together once in bulk afterwards before push
      while(fieldLen>0 && ch[-1]==' ') { fieldLen--; ch--; }
      // this space can't be sep otherwise it would have stopped the field earlier inside end_of_field()
    }
    if ((fieldLen==0 && blank_is_a_NAstring) || (fieldLen && end_NA_string(fieldStart)==ch)) fieldLen=INT32_MIN;  // TODO - speed up by avoiding end_NA_string when there are none
    ctx.target->str32.off = (int32_t)(fieldStart - ctx.anchor);
    ctx.target->str32.len = fieldLen;
    return;
  }
  // else *ch==quote (we don't mind that quoted fields are a little slower e.g. no desire to save switch)
  //    the field is quoted and quotes are correctly escaped (quoteRule 0 and 1)
  // or the field is quoted but quotes are not escaped (quoteRule 2)
  // or the field is not quoted but the data contains a quote at the start (quoteRule 2 too)
  int eolCount = 0;
  fieldStart++;  // step over opening quote
  switch(quoteRule) {
  case 0:  // quoted with embedded quotes doubled; the final unescaped " must be followed by sep|eol
    while (*++ch) {
      if (*ch=='\n' && ++eolCount==100) return;  // TODO: expose this 100 to user to allow them to control limiting runaway fields
      if (*ch==quote) {
        if (ch[1]==quote) { ch++; continue; }
        break;  // found undoubled closing quote
      }
    }
    break;
  case 1:  // quoted with embedded quotes escaped; the final unescaped " must be followed by sep|eol
    while (*++ch) {
      if (*ch=='\n' && ++eolCount==100) return;
      if (*ch=='\\' && (ch[1]==quote || ch[1]=='\\')) { ch++; continue; }
      if (*ch==quote) break;
    }
    break;
  case 2:
    // (i) quoted (perhaps because the source system knows sep is present) but any quotes were not escaped at all,
    // so look for ", to define the end.   (There might not be any quotes present to worry about, anyway).
    // (ii) not-quoted but there is a quote at the beginning so it should have been; look for , at the end
    // If no eol are present inside quoted fields (i.e. rows are simple rows), then this should work ok e.g. test 1453
    // since we look for ", and the source system quoted when , is present, looking for ", should work well.
    // Under this rule, no eol may occur inside fields.
    {
      const char* ch2 = ch;
      while (*++ch && *ch!='\n' && *ch!='\r') {
        if (*ch==quote && end_of_field(ch+1)) {ch2=ch; break;}  // (*1) regular ", ending; leave *ch on closing quote
        if (*ch==sep) {
          // first sep in this field
          // if there is a ", afterwards but before the next \n, use that; the field was quoted and it's still case (i) above.
          // Otherwise break here at this first sep as it's case (ii) above (the data contains a quote at the start and no sep)
          ch2 = ch;
          while (*++ch2 && *ch2!='\n' && *ch2!='\r') {
            if (*ch2==quote && end_of_field(ch2+1)) {
              ch = ch2;                                          // (*2) move on to that first ", -- that's this field's ending
              break;
            }
          }
          break;
        }
      }
      if (ch!=ch2) fieldStart--;   // field ending is this sep|eol; neither (*1) or (*2) happened; opening quote wasn't really an opening quote
    }
    break;
  default:
    return;  // Internal error: undefined quote rule
  }
  ctx.target->str32.len = (int32_t)(ch - fieldStart);
  ctx.target->str32.off = (int32_t)(fieldStart - ctx.anchor);
  if (*ch==quote) {
    ch++;
    skip_white(&ch);
    ctx.ch = ch;
  } else {
    ctx.ch = ch;
    if (*ch=='\0') {
      if (quoteRule!=2) {  // see test 1324 where final field has open quote but not ending quote; include the open quote like quote rule 2
        ctx.target->str32.off--;
        ctx.target->str32.len++;
      }
    }
    if (stripWhite) {  // see test 1551.6; trailing whitespace in field [67,V37] == "\"\"A\"\" ST       "
      while (ctx.target->str32.len>0 && ch[-1]==' ') {
        ctx.target->str32.len--;
        ch--;
      }
    }
  }
}


static void parse_int32(FieldParseContext& ctx)
{
  const char* ch = ctx.ch;

  bool neg = *ch=='-';
  ch += (neg || *ch=='+');
  const char* start = ch;  // to know if at least one digit is present
  // acc needs to be 64bit so that 5bn (still 10 digits but greater than 4bn) does not overflow. It could be
  //   signed but we use unsigned to be clear it will never be negative
  uint_fast64_t acc = 0;
  uint_fast8_t digit;
  // sep, \r, \n and *eof=='\0' all serve as valid terminators here by dint of being !=[0-9]
  // see init.c for checks of unsigned uint_fast8_t cast
  // optimizer should implement 10* as ((x<<2 + x)<<1) or (x<<3 + x<<1)

  /*if (loseLeadingZeroOption)*/ while (*ch=='0') ch++;
  // number significant figures = digits from the first non-zero onwards including trailing zeros
  uint_fast32_t sf = 0;
  while ( (digit=(uint_fast8_t)(ch[sf]-'0'))<10 ) {
    acc = 10*acc + digit;
    sf++;
  }
  ch += sf;
  // INT32 range is NA==-2147483648(INT32_MIN) then symmetric [-2147483647,+2147483647] so we can just test INT32_MAX
  // The max (2147483647) happens to be 10 digits long, hence <=10.
  // Leading 0 (such as 001 and 099 but not 0, +0 or -0) will cause type bump to _full which has the
  // option to treat as integer or string with further cost.
  // if ( (acc && *start!='0' && acc<=INT32_MAX && (ch-start)<=10) ||
  //     (acc==0 && ch-start==1) ) {
  if ((sf || ch>start) && sf<=10 && acc<=INT32_MAX) {
    ctx.target->int32 = neg ? -(int32_t)acc : (int32_t)acc;
    ctx.ch = ch;
  } else {
    ctx.target->int32 = NA_INT32;  // empty field ideally, contains NA and fall through to check if NA (in which case this write is important), or just plain invalid
  }
}


static void parse_int64(FieldParseContext& ctx)
{
  const char* ch = ctx.ch;

  bool neg = *ch=='-';
  ch += (neg || *ch=='+');
  const char* start = ch;
  while (*ch=='0') ch++;
  uint_fast64_t acc = 0;  // important unsigned not signed here; we now need the full unsigned range
  uint_fast8_t digit;
  uint_fast32_t sf = 0;
  while ( (digit=(uint_fast8_t)(ch[sf]-'0')) < 10 ) {
    acc = 10*acc + digit;
    sf++;
  }
  ch += sf;
  // INT64 range is NA==-9223372036854775808(INT64_MIN) then symmetric [-9223372036854775807,+9223372036854775807].
  // A 20+ digit number is caught as too large via the field width check <=19, since leading zeros trigger character type not numeric
  // TODO Check tests exist that +9223372036854775808 and +9999999999999999999 are caught as too large. They are stll 19 wide
  //   and, fortunately, uint64 can hold 9999999999999999999 (19 9's) so that doesn't overflow uint64.
  //if ( (acc && *start!='0' && acc<=INT64_MAX && (ch-start)<=19) ||
  //     (acc==0 && ch-start==1) ) {
  if ((sf || ch>start) && sf<=19 && acc<=INT64_MAX) {
    ctx.target->int64 = neg ? -(int64_t)acc : (int64_t)acc;
    ctx.ch = ch;
  } else {
    ctx.target->int64 = NA_INT64;
  }
}


/**
 * Parse "usual" double literals, in the form
 *
 *   [+|-] (NNN|NNN.|.MMM|NNN.MMM) [(E|e) [+|-] EEE]
 *
 * where `NNN`, `MMM`, `EEE` are one or more decimal digits, representing the
 * whole part, fractional part, and the exponent respectively.
 *
 * Right now we do not parse floating numbers that would incur significant loss
 * of precision, for example `1.2439827340958723094785103` will not be parsed
 * as a double.
 */
static void parse_double_regular(FieldParseContext& ctx)
{
  //
  const char* ch = ctx.ch;

  bool neg, Eneg;
  double r;
  ch += (neg = *ch=='-') + (*ch=='+');

  const char* start = ch;
  uint_fast64_t acc = 0;  // holds NNN.MMM as NNNMMM
  int_fast32_t e = 0;     // width of MMM to adjust NNNMMM by dec location
  uint_fast8_t digit;
  while (*ch=='0') ch++;

  uint_fast32_t sf = 0;
  while ( (digit=(uint_fast8_t)(ch[sf]-'0'))<10 ) {
    acc = 10*acc + digit;
    sf++;
  }
  ch += sf;
  if (*ch==dec) {
    ch++;
    // Numbers like 0.00000000000000000000000000000000004 can be read without
    // loss of precision as 4e-35  (test 1817)
    if (sf==0 && *ch=='0') {
      while (ch[e]=='0') e++;
      ch += e;
      e = -e;
    }
    uint_fast32_t k = 0;
    while ( (digit=(uint_fast8_t)(ch[k]-'0'))<10 ) {
      acc = 10*acc + digit;
      k++;
    }
    ch += k;
    sf += k;
    e -= k;
  }
  if (sf>18) goto fail;  // Too much precision for double. TODO: reduce to 15(?) and discard trailing 0's.
  if (*ch=='E' || *ch=='e') {
    if (ch==start) goto fail;  // something valid must be between [+|-] and E, character E alone is invalid.
    ch += 1/*E*/ + (Eneg = ch[1]=='-') + (ch[1]=='+');
    int E=0, max_digits=3;
    while ( max_digits && (digit=(uint_fast8_t)(*ch-'0'))<10 ) {
      E = 10*E + digit;
      ch++;
      max_digits--;
    }
    e += Eneg? -E : E;
  }
  e += 350; // lookup table is arranged from -350 (0) to +350 (700)
  if (e<0 || e>700 || ch==start) goto fail;

  r = (double)((long double)acc * pow10lookup[e]);
  ctx.target->float64 = neg? -r : r;
  ctx.ch = ch;
  return;

  fail:
    ctx.target->float64 = NA_FLOAT64;
}


/**
 * Parses double values, but also understands various forms of NAN literals
 * (each can possibly be preceded with a `+` or `-` sign):
 *
 *   nan, inf, NaN, NAN, NaN%, NaNQ, NaNS, qNaN, sNaN, NaN12345, sNaN54321,
 *   1.#SNAN, 1.#QNAN, 1.#IND, 1.#INF, INF, Inf, Infinity,
 *   #DIV/0!, #VALUE!, #NULL!, #NAME?, #NUM!, #REF!, #N/A
 *
 */
static void parse_double_extended(FieldParseContext& ctx)
{
  const char* ch = ctx.ch;
  bool neg, quoted;
  ch += (quoted = (*ch==quote));
  ch += (neg = (*ch=='-')) + (*ch=='+');

  if (ch[0]=='n' && ch[1]=='a' && ch[2]=='n' && (ch += 3)) goto return_nan;
  if (ch[0]=='i' && ch[1]=='n' && ch[2]=='f' && (ch += 3)) goto return_inf;
  if (ch[0]=='I' && ch[1]=='N' && ch[2]=='F' && (ch += 3)) goto return_inf;
  if (ch[0]=='I' && ch[1]=='n' && ch[2]=='f' && (ch += 3)) {
    if (ch[0]=='i' && ch[1]=='n' && ch[2]=='i' && ch[3]=='t' && ch[4]=='y') ch += 5;
    goto return_inf;
  }
  if (ch[0]=='N' && (ch[1]=='A' || ch[1]=='a') && ch[2]=='N' && (ch += 3)) {
    if (ch[-2]=='a' && (*ch=='%' || *ch=='Q' || *ch=='S')) ch++;
    while ((uint_fast8_t)(*ch-'0') < 10) ch++;
    goto return_nan;
  }
  if ((ch[0]=='q' || ch[0]=='s') && ch[1]=='N' && ch[2]=='a' && ch[3]=='N' && (ch += 4)) {
    while ((uint_fast8_t)(*ch-'0') < 10) ch++;
    goto return_nan;
  }
  if (ch[0]=='1' && ch[1]=='.' && ch[2]=='#') {
    if ((ch[3]=='S' || ch[3]=='Q') && ch[4]=='N' && ch[5]=='A' && ch[6]=='N' && (ch += 7)) goto return_nan;
    if (ch[3]=='I' && ch[4]=='N' && ch[5]=='D' && (ch += 6)) goto return_nan;
    if (ch[3]=='I' && ch[4]=='N' && ch[5]=='F' && (ch += 6)) goto return_inf;
  }
  if (ch[0]=='#') {  // Excel-specific "numbers"
    if (ch[1]=='D' && ch[2]=='I' && ch[3]=='V' && ch[4]=='/' && ch[5]=='0' && ch[6]=='!' && (ch += 7)) goto return_nan;
    if (ch[1]=='V' && ch[2]=='A' && ch[3]=='L' && ch[4]=='U' && ch[5]=='E' && ch[6]=='!' && (ch += 7)) goto return_nan;
    if (ch[1]=='N' && ch[2]=='U' && ch[3]=='L' && ch[4]=='L' && ch[5]=='!' && (ch += 6)) goto return_na;
    if (ch[1]=='N' && ch[2]=='A' && ch[3]=='M' && ch[4]=='E' && ch[5]=='?' && (ch += 6)) goto return_na;
    if (ch[1]=='N' && ch[2]=='U' && ch[3]=='M' && ch[4]=='!' && (ch += 5)) goto return_na;
    if (ch[1]=='R' && ch[2]=='E' && ch[3]=='F' && ch[4]=='!' && (ch += 5)) goto return_na;
    if (ch[1]=='N' && ch[2]=='/' && ch[3]=='A' && (ch += 4)) goto return_na;
  }
  parse_double_regular(ctx);
  return;

  return_inf:
    ctx.target->float64 = neg? -INFD : INFD;
    goto ok;
  return_nan:
    ctx.target->float64 = NAND;
    goto ok;
  return_na:
    ctx.target->float64 = NA_FLOAT64;
  ok:
    if (quoted && *ch!=quote) {
      ctx.target->float64 = NA_FLOAT64;
    } else {
      ctx.ch = ch + quoted;
    }
}


/**
 * Parser for hexadecimal doubles. This format is used in Java (via
 * `Double.toHexString(x)`), in C (`printf("%a", x)`), and in Python
 * (`x.hex()`).
 *
 * The numbers are in the following format:
 *
 *   [+|-] (0x|0X) (0.|1.) HexDigits (p|P) [+|-] DecExponent
 *
 * Thus the number has optional sign; followed by hex prefix `0x` or `0X`;
 * followed by hex significand which may be in the form of either `0.HHHHH...`
 * or `1.HHHHH...` where `H` are hex-digits (there can be no more than 13
 * digits; first form is used for subnormal numbers, second for normal ones);
 * followed by exponent indicator `p` or `P`; followed by optional exponent
 * sign; and lastly followed by the exponent which is a decimal number.
 *
 * This can be directly converted into IEEE-754 double representation:
 *
 *   <1 bit: sign> <11 bits: exp+1022> <52 bits: significand>
 *
 * This parser also recognizes literals "NaN" and "Infinity" which can be
 * produced by Java.
 *
 * @see http://docs.oracle.com/javase/specs/jls/se8/html/jls-3.html#jls-3.10.2
 * @see https://en.wikipedia.org/wiki/IEEE_754-1985
 */
static void parse_double_hexadecimal(FieldParseContext& ctx)
{
  const char* ch = ctx.ch;
  uint64_t neg;
  uint8_t digit;
  bool Eneg, subnormal = 0;
  ch += (neg = (*ch=='-')) + (*ch=='+');

  if (ch[0]=='0' && (ch[1]=='x' || ch[1]=='X') && (ch[2]=='1' || (subnormal = ch[2]=='0'))) {
    ch += 3;
    uint64_t acc = 0;
    if (*ch == '.') {
      ch++;
      int ndigits = 0;
      while ((digit = hexdigits[static_cast<uint8_t>(*ch)]) < 16) {
        acc = (acc << 4) + digit;
        ch++;
        ndigits++;
      }
      if (ndigits > 13) goto fail;
      acc <<= (13 - ndigits) * 4;
    }
    if (*ch!='p' && *ch!='P') goto fail;
    ch += 1 + (Eneg = ch[1]=='-') + (ch[1]=='+');
    uint64_t E = 0;
    while ( (digit = static_cast<uint8_t>(*ch - '0')) < 10 ) {
      E = 10*E + digit;
      ch++;
    }
    if (subnormal) {
      if (E == 0 && acc == 0) /* zero */;
      else if (E == 1022 && Eneg && acc) /* subnormal */ E = 0;
      else goto fail;
    } else {
      E = 1023 + (E ^ -Eneg) + Eneg;
      if (E < 1 || E > 2046) goto fail;
    }
    ctx.target->uint64 = (neg << 63) | (E << 52) | (acc);
    ctx.ch = ch;
    return;
  }
  if (ch[0]=='N' && ch[1]=='a' && ch[2]=='N') {
    ctx.target->float64 = NA_FLOAT64;
    ctx.ch = ch + 3;
    return;
  }
  if (ch[0]=='I' && ch[1]=='n' && ch[2]=='f' && ch[3]=='i' &&
      ch[4]=='n' && ch[5]=='i' && ch[6]=='t' && ch[7]=='y') {
    ctx.target->float64 = neg ? -INFD : INFD;
    ctx.ch = ch + 8;
    return;
  }

  fail:
    ctx.target->float64 = NA_FLOAT64;
}


static void parse_float_hexadecimal(FieldParseContext& ctx)
{
  const char* ch = ctx.ch;
  uint32_t neg;
  uint8_t digit;
  bool Eneg, subnormal = 0;
  ch += (neg = (*ch=='-')) + (*ch=='+');

  if (ch[0]=='0' && (ch[1]=='x' || ch[1]=='X') && (ch[2]=='1' || (subnormal = ch[2]=='0'))) {
    ch += 3;
    uint32_t acc = 0;
    if (*ch == '.') {
      ch++;
      int ndigits = 0;
      while ((digit = hexdigits[static_cast<uint8_t>(*ch)]) < 16) {
        acc = (acc << 4) + digit;
        ch++;
        ndigits++;
      }
      if (ndigits > 6) goto fail;
      acc <<= 24 - ndigits * 4;
      acc >>= 1;
    }
    if (*ch!='p' && *ch!='P') goto fail;
    ch += 1 + (Eneg = ch[1]=='-') + (ch[1]=='+');
    uint32_t E = 0;
    while ( (digit = static_cast<uint8_t>(*ch - '0')) < 10 ) {
      E = 10*E + digit;
      ch++;
    }
    if (subnormal) {
      if (E == 0 && acc == 0) /* zero */;
      else if (E == 126 && Eneg && acc) /* subnormal */ E = 0;
      else goto fail;
    } else {
      E = 127 + (E ^ -Eneg) + Eneg;
      if (E < 1 || E > 254) goto fail;
    }
    ctx.target->uint32 = (neg << 31) | (E << 23) | (acc);
    ctx.ch = ch;
    return;
  }
  if (ch[0]=='N' && ch[1]=='a' && ch[2]=='N') {
    ctx.target->float32 = NA_FLOAT32;
    ctx.ch = ch + 3;
    return;
  }
  if (ch[0]=='I' && ch[1]=='n' && ch[2]=='f' && ch[3]=='i' &&
      ch[4]=='n' && ch[5]=='i' && ch[6]=='t' && ch[7]=='y') {
    ctx.target->float32 = neg ? -INFINITY : INFINITY;
    ctx.ch = ch + 8;
    return;
  }

  fail:
    ctx.target->float32 = NA_FLOAT32;
}




// In order to add a new type:
//   - register new parser in this `parsers` array
//   - add entries in arrays `typeName` / `typeSize` / `typeSymbols` at the top of this file
//   - add entry in array `colType` in "fread.h" and increase NUMTYPE
//   - add record in array `colType_to_stype` in "reader_fread.cc"
//   - add items in `_coltypes_strs` and `_coltypes` in "fread.py"
//   - update `test_fread_fillna1` in test_fread.py to include the new column type
//
typedef void (*reader_fun_t)(FieldParseContext& ctx);
static reader_fun_t parsers[NUMTYPE] = {
  (reader_fun_t) &parse_string,   // CT_DROP
  parser_Bool01,
  parser_BoolU,
  parser_BoolT,
  parser_BoolL,
  (reader_fun_t) &parse_int32,
  (reader_fun_t) &parse_int64,
  (reader_fun_t) &parse_float_hexadecimal,
  (reader_fun_t) &parse_double_regular,
  (reader_fun_t) &parse_double_extended,
  (reader_fun_t) &parse_double_hexadecimal,
  (reader_fun_t) &parse_string
};



//=================================================================================================
//
// Main fread() function that does all the job of reading a text/csv file.
//
// Returns 1 if it finishes successfully, and 0 otherwise.
//
//=================================================================================================
int FreadReader::freadMain()
{
  double t0 = wallclock();
  bool verbose = g.verbose;
  bool warningsAreErrors = g.warnings_to_errors;
  int nth = g.nthreads;
  size_t nrowLimit = (size_t) g.max_nrows;

  uint64_t ui64 = NA_FLOAT64_I64;
  uint32_t ui32 = NA_FLOAT32_I32;
  memcpy(&NA_FLOAT64, &ui64, 8);
  memcpy(&NA_FLOAT32, &ui32, 4);
  assert(sizeof(field64) == 8);

  NAstrings = g.na_strings;
  blank_is_a_NAstring = g.blank_is_na;
  any_number_like_NAstrings = g.number_is_na;
  stripWhite = g.strip_white;
  skipEmptyLines = g.skip_blank_lines;
  fill = g.fill;
  dec = g.dec;
  quote = g.quote;
  int header = g.header;

  size_t fileSize = g.datasize();
  const char* sof = g.dataptr();
  eof = sof + fileSize;
  // TODO: Do not require the extra byte, and do not write into the input stream...
  ASSERT(g.extra_byte_accessible() && fileSize > 0);
  *const_cast<char*>(eof) = '\0';

  // Convenience variable for iterating over the file.
  const char* ch = NULL;
  int line = 1;

  // Test whether '\n's are present in the file at all... If not, then standalone '\r's are valid
  // line endings. However if '\n' exists in the file, then '\r' will be considered as regular
  // characters instead of a line ending.
  int cnt = 0;
  ch = sof;
  while (ch < eof && *ch != '\n' && cnt < 100) {
    cnt += (*ch == '\r');
    ch++;
  }
  LFpresent = (ch < eof && *ch == '\n');
  if (LFpresent) {
    g.trace("LF character (\\n) found in input, \\r-only line endings are prohibited");
  } else {
    g.trace("LF character (\\n) not found in input, CR (\\r) will be considered a line ending");
  }


  //*********************************************************************************************
  // [6] Auto detect separator, quoting rule, first line and ncol, simply,
  //     using jump 0 only.
  //
  //     Always sample as if nrows= wasn't supplied. That's probably *why*
  //     user is setting nrow=0 to get the column names and types, without
  //     actually reading the data yet. Most likely to check consistency
  //     across a set of files.
  //*********************************************************************************************
  const char* firstJumpEnd = NULL; // remember where the winning jumpline from jump 0 ends, to know its size excluding header
  int ncol;  // Detected number of columns in the file
  {
    if (verbose) DTPRINT("[06] Detect separator, quoting rule, and ncolumns");

    int nseps;
    char seps[] = ",|;\t ";  // default seps in order of preference. See ?fread.
    // using seps[] not *seps for writeability (http://stackoverflow.com/a/164258/403310)

    if (g.sep == '\xFF') {   // '\xFF' means 'auto'
      nseps = (int) strlen(seps);
    } else {
      seps[0] = g.sep;
      seps[1] = '\0';
      nseps = 1;
      if (verbose) DTPRINT("  Using supplied sep '%s'", g.sep=='\t' ? "\\t" : seps);
    }

    int topNumLines=0;        // the most number of lines with the same number of fields, so far
    int topNumFields=1;       // how many fields that was, to resolve ties
    char topSep='\n';          // which sep that was, by default \n to mean single-column input (1 field)
    int topQuoteRule=0;       // which quote rule that was
    int topNmax=1;            // for that sep and quote rule, what was the max number of columns (just for fill=true)
                              //   (when fill=true, the max is usually the header row and is the longest but there are more
                              //    lines of fewer)

    // We will scan the input line-by-line (at most `JUMPLINES + 1` lines; "+1"
    // covers the header row, at this stage we don't know if it's present), and
    // detect the number of fields on each line. If several consecutive lines
    // have the same number of fields, we'll call them a "contiguous group of
    // lines". Arrays `numFields` and `numLines` contain information about each
    // contiguous group of lines encountered while scanning the first JUMPLINES
    // + 1 lines: 'numFields` gives the count of fields in each group, and
    // `numLines` has the number of lines in each group.
    int numFields[JUMPLINES+1];
    int numLines[JUMPLINES+1];
    for (int s=0; s<nseps; s++) {
      sep = seps[s];
      whiteChar = (sep==' ' ? '\t' : (sep=='\t' ? ' ' : 0));  // 0 means both ' ' and '\t' to be skipped
      for (quoteRule=0; quoteRule<4; quoteRule++) {  // quote rule in order of preference
        ch = sof;
        // if (verbose) DTPRINT("  Trying sep='%c' with quoteRule %d ...\n", sep, quoteRule);
        for (int i=0; i<=JUMPLINES; i++) { numFields[i]=0; numLines[i]=0; } // clear VLAs
        int i=-1; // The slot we're counting the currently contiguous consistent ncol
        int thisLine=0, lastncol=-1;
        while (ch < eof && thisLine++ < JUMPLINES) {
          // Compute num columns and move `ch` to the start of next line
          int thisncol = countfields(&ch);
          if (thisncol < 0) {
            // invalid file with this sep and quote rule; abort
            numFields[0] = -1;
            break;
          }
          if (thisncol != lastncol) {  // new contiguous consistent ncol started
            numFields[++i] = thisncol;
            lastncol = thisncol;
          }
          numLines[i]++;
        }
        if (numFields[0] == -1) continue;
        if (firstJumpEnd == NULL) firstJumpEnd = ch;  // if this wins (doesn't get updated), it'll be single column input
        bool updated = false;
        int nmax = 0;

        i = -1;
        while (numLines[++i]) {
          if (numFields[i] > nmax) {  // for fill=true to know max number of columns
            nmax = numFields[i];
          }
          if ( numFields[i]>1 &&
              (numLines[i]>1 || (/*blank line after single line*/numFields[i+1]==0)) &&
              ((numLines[i]>topNumLines) ||   // most number of consistent ncol wins
               (numLines[i]==topNumLines && numFields[i]>topNumFields && sep!=topSep && sep!=' '))) {
               //                                       ^ ties in numLines resolved by numFields (more fields win)
               //                                                           ^ but don't resolve a tie with a higher quote
               //                                                             rule unless the sep is different too: #2404, #2839
            topNumLines = numLines[i];
            topNumFields = numFields[i];
            topSep = sep;
            topQuoteRule = quoteRule;
            topNmax = nmax;
            firstJumpEnd = ch;  // So that after the header we know how many bytes jump point 0 is
            updated = true;
            // Two updates can happen for the same sep and quoteRule (e.g. issue_1113_fread.txt where sep=' ') so the
            // updated flag is just to print once.
          }
        }
        if (verbose && updated) {
          DTPRINT(sep<' '? "  sep=%#02x with %d lines of %d fields using quote rule %d" :
                           "  sep='%c' with %d lines of %d fields using quote rule %d",
                  sep, topNumLines, topNumFields, topQuoteRule);
        }
      }
    }
    ASSERT(firstJumpEnd);
    quoteRule = topQuoteRule;
    sep = topSep;
    whiteChar = (sep==' ' ? '\t' : (sep=='\t' ? ' ' : 0));
    if (sep==' ' && !fill) {
      if (verbose) DTPRINT("  sep=' ' detected, setting fill to True\n");
      fill = 1;
    }

    // Find the first line with the consistent number of fields.  There might
    // be irregular header lines above it.
    const char* prevStart = NULL;  // the start of the non-empty line before the first not-ignored row
    if (fill) {
      // start input from first populated line; do not alter sof.
      ncol = topNmax;
    } else {
      ncol = topNumFields;
      int thisLine = -1;
      ch = sof;
      while (ch < eof && ++thisLine < JUMPLINES) {
        const char* lastLineStart = ch;   // lineStart
        int cols = countfields(&ch);  // advances ch to next line
        if (cols == ncol) {
          ch = sof = lastLineStart;
          line += thisLine;
          break;
        } else {
          prevStart = (cols > 0)? lastLineStart : NULL;
        }
      }
    }
    // For standard regular separated files, we're now on the first byte of the file.

    ASSERT(ncol >= 1 && line >= 1);
    ch = sof;
    int tt = countfields(&ch);
    ch = sof;  // move back to start of line since countfields() moved to next
    ASSERT(fill || tt == ncol);
    if (verbose) {
      DTPRINT("  Detected %d columns on line %d. This line is either column "
              "names or first data row. Line starts as: \"%s\"",
              tt, line, strlim(sof, 30));
      DTPRINT("  Quote rule picked = %d", quoteRule);
      if (fill) DTPRINT("  fill=true and the most number of columns found is %d", ncol);
    }

    // Now check previous line which is being discarded and give helpful message to user
    if (prevStart) {
      ch = prevStart;
      int ttt = countfields(&ch);
      ASSERT(ttt != ncol);
      if (ttt > 1) {
        DTWARN("Starting data input on line %d <<%s>> with %d fields and discarding "
               "line %d <<%s>> before it because it has a different number of fields (%d).",
               line, strlim(sof, 30), ncol, line-1, strlim(prevStart, 30), ttt);
      }
    }
    ASSERT(ch==sof);
  }


  //*********************************************************************************************
  // [7] Detect column types, good nrow estimate and whether first row is column names.
  //     At the same time, calc mean and sd of row lengths in sample for very
  //     good nrow estimate.
  //*********************************************************************************************
  size_t allocnrow;       // Number of rows in the allocated DataTable
  double meanLineLen;     // Average length (in bytes) of a single line in the input file
  int nJumps;             // How many jumps to use when pre-scanning the file
  size_t sampleLines;     // How many lines were sampled during the initial pre-scan
  size_t bytesRead;       // Bytes in the whole data section
  const char* lastRowEnd; // Pointer to the end of the data section
  {
    if (verbose) DTPRINT("[07] Detect column types, and whether first row contains column names");
    types = new int8_t[ncol];
    sizes = new int8_t[ncol];
    tmpTypes = new int8_t[ncol];

    int8_t type0 = 1;
    // while (disabled_parsers[type0]) type0++;
    for (int j = 0; j < ncol; j++) {
      // initialize with the first (lowest) type
      types[j] = type0;
      tmpTypes[j] = type0;
    }
    field64 trash;
    FieldParseContext fctx = {
      .ch = ch,
      .target = &trash,
      .anchor = NULL,
    };

    // the size in bytes of the first JUMPLINES from the start (jump point 0)
    size_t jump0size = (size_t)(firstJumpEnd - sof);
    // how many places in the file to jump to and test types there (the very end is added as 11th or 101th)
    // not too many though so as not to slow down wide files; e.g. 10,000 columns.  But for such large files (50GB) it is
    // worth spending a few extra seconds sampling 10,000 rows to decrease a chance of costly reread even further.
    nJumps = 0;
    size_t sz = (size_t)(eof - sof);
    if (jump0size>0) {
      if (jump0size*100*2 < sz) nJumps=100;  // 100 jumps * 100 lines = 10,000 line sample
      else if (jump0size*10*2 < sz) nJumps=10;
      // *2 to get a good spacing. We don't want overlaps resulting in double counting.
      // nJumps==1 means the whole (small) file will be sampled with one thread
    }
    nJumps++; // the extra sample at the very end (up to eof) is sampled and format checked but not jumped to when reading
    if (verbose) {
      if (jump0size==0)
        DTPRINT("  Number of sampling jump points = %d because jump0size==0", nJumps);
      else
        DTPRINT("  Number of sampling jump points = %d because (%zd bytes from row 1 to eof) / (2 * %zd jump0size) == %zd",
                nJumps, sz, jump0size, sz/(2*jump0size));
    }

    sampleLines = 0;
    int row1Line = line;
    double sumLen = 0.0;
    double sumLenSq = 0.0;
    int minLen = INT32_MAX;   // int_max so the first if(thisLen<minLen) is always true; similarly for max
    int maxLen = -1;
    lastRowEnd = sof;
    bool firstDataRowAfterPotentialColumnNames = false;  // for test 1585.7
    bool lastSampleJumpOk = false;   // it won't be ok if its nextGoodLine returns false as testing in test 1768
    for (int j=0; j<nJumps; j++) {
      ch = (j == 0) ? sof :
           (j == nJumps-1) ? eof - (size_t)(0.5*jump0size) :
                             sof + (size_t)j*(sz/(size_t)(nJumps-1));
      if (ch < lastRowEnd) ch = lastRowEnd;  // Overlap when apx 1,200 lines (just over 11*100) with short lines at the beginning and longer lines near the end, #2157
      if (ch >= eof) break;                  // The 9th jump could reach the end in the same situation and that's ok. As long as the end is sampled is what we want.
      if (j > 0 && !nextGoodLine(&ch, ncol)) {
        // skip this jump for sampling. Very unusual and in such unusual cases, we don't mind a slightly worse guess.
        continue;
      }
      bool bumped = false;  // did this jump find any different types; to reduce verbose output to relevant lines
      bool skip = false;
      int jline = 0;  // line from this jump point

      while (ch < eof && (jline<JUMPLINES || j==nJumps-1)) {
        // nJumps==1 implies sample all of input to eof; last jump to eof too
        const char* jlineStart = ch;
        if (sep==' ') while (ch<eof && *ch==' ') ch++;  // multiple sep=' ' at the jlineStart does not mean sep(!)
        // detect blank lines
        skip_white(&ch);
        if (ch == eof) break;
        if (ncol > 1 && eol(&ch)) {
          ch++;
          if (skipEmptyLines) continue;
          if (!fill) break;
          sampleLines++;
          lastRowEnd = ch;
          continue;
        }
        jline++;
        int field = 0;
        const char* fieldStart = NULL;  // Needed outside loop for error messages below
        ch--;
        while (field<ncol) {
          ch++;
          skip_white(&ch);
          fieldStart = ch;
          bool thisColumnNameWasString = false;
          if (firstDataRowAfterPotentialColumnNames) {
            // 2nd non-blank row is being read now.
            // 1st row's type is remembered and compared (a little lower down) to second row to decide if 1st row is column names or not
            thisColumnNameWasString = (tmpTypes[field]==CT_STRING);
            tmpTypes[field] = type0;  // re-initialize for 2nd row onwards
          }
          while (tmpTypes[field]<=CT_STRING) {
            parsers[tmpTypes[field]](fctx);
            skip_white(&ch);
            if (end_of_field(ch)) break;
            ch = end_NA_string(fieldStart);
            if (end_of_field(ch)) break;
            if (tmpTypes[field]<CT_STRING) {
              ch = fieldStart;
              if (*ch==quote) {
                ch++;
                parsers[tmpTypes[field]](fctx);
                if (*ch==quote) {
                  ch++;
                  skip_white(&ch);
                  if (end_of_field(ch)) break;
                }
              }
              tmpTypes[field]++;
              // while (disabled_parsers[tmpTypes[field]]) tmpTypes[field]++;
            } else {
              // the field could not be read with this quote rule, try again with next one
              // Trying the next rule will only be successful if the number of fields is consistent with it
              ASSERT(quoteRule < 3);
              if (verbose)
                DTPRINT("Bumping quote rule from %d to %d due to field %d on line %d of sampling jump %d starting \"%s\"",
                        quoteRule, quoteRule+1, field+1, jline, j, strlim(fieldStart,200));
              quoteRule++;
            }
            bumped = true;
            ch = fieldStart;
          }
          if (header==NA_BOOL8 && thisColumnNameWasString && tmpTypes[field] < CT_STRING) {
            header = true;
            g.trace("header determined to be True due to column %d containing a string on row 1 and a lower type (%s) on row 2",
                    field + 1, typeName[tmpTypes[field]]);
          }
          if (*ch!=sep || *ch=='\n' || *ch=='\r') break;
          if (sep==' ') {
            while (ch[1]==' ') ch++;
            if (ch[1]=='\r' || ch[1]=='\n' || ch[1]=='\0') { ch++; break; }
          }
          field++;
        }
        eol(&ch);
        if (field < ncol-1 && !fill) {
          ASSERT(ch==eof || *ch=='\n' || *ch=='\r');
          STOP("Line %d has too few fields when detecting types. Use fill=True to pad with NA. "
               "Expecting %d fields but found %d: \"%s\"", jline, ncol, field+1, strlim(jlineStart, 200));
        }
        if (field>=ncol || (*ch!='\n' && *ch!='\r' && *ch!='\0')) {   // >=ncol covers ==ncol. We do not expect >ncol to ever happen.
          if (j==0) {
            STOP("Line %d starting <<%s>> has more than the expected %d fields. "
               "Separator '%c' occurs at position %d which is character %d of the last field: <<%s>>. "
               "Consider setting 'comment.char=' if there is a trailing comment to be ignored.",
               jline, strlim(jlineStart,10), ncol, *ch, (int)(ch-jlineStart+1), (int)(ch-fieldStart+1), strlim(fieldStart,200));
          }
          g.trace("  Not using sample from jump %d. Looks like a complicated file where nextGoodLine could not establish the true line start.", j);
          skip = true;
          break;
        }
        if (firstDataRowAfterPotentialColumnNames) {
          if (fill) {
            for (int jj=field+1; jj<ncol; jj++) tmpTypes[jj] = type0;
          }
          firstDataRowAfterPotentialColumnNames = false;
        } else if (sampleLines==0) {
          // To trigger 2nd row starting from type 1 again to compare to 1st row to decide if column names present
          firstDataRowAfterPotentialColumnNames = true;
        }
        ch += (*ch=='\n' || *ch=='\r');

        lastRowEnd = ch;
        int thisLineLen = (int)(ch-jlineStart);  // ch is now on start of next line so this includes EOLLEN already
        ASSERT(thisLineLen >= 0);
        sampleLines++;
        sumLen += thisLineLen;
        sumLenSq += thisLineLen*thisLineLen;
        if (thisLineLen<minLen) minLen=thisLineLen;
        if (thisLineLen>maxLen) maxLen=thisLineLen;
      }
      if (skip) continue;
      if (j==nJumps-1) lastSampleJumpOk = true;
      if (bumped) memcpy(types, tmpTypes, (size_t)ncol);
      if (verbose && (bumped || j==0 || j==nJumps-1)) {
        DTPRINT("  Type codes (jump %03d): %s  Quote rule %d", j, printTypes(ncol), quoteRule);
      }
    }
    if (lastSampleJumpOk) {
      while (ch < eof && isspace(*ch)) ch++;
      if (ch < eof) {
        DTWARN("Found the last consistent line but text exists afterwards (discarded): \"%s\"", strlim(ch, 200));
      }
    } else {
      // nextGoodLine() was false for the last (extra) jump to check the end
      // must set lastRowEnd to eof accordingly otherwise it'll be left wherever the last good jump finished
      lastRowEnd = eof;
    }
    eof = lastRowEnd;

    size_t estnrow = 1;
    allocnrow = 1;
    meanLineLen = 0;
    bytesRead = 0;

    if (header == NA_BOOL8) {
      header = true;
      for (int j=0; j<ncol; j++) {
        if (types[j] < CT_STRING) {
          header = false;
          break;
        }
      }
      if (verbose) {
        g.trace("header detetected to be %s because %s",
                header? "True" : "False",
                sampleLines <= 1 ?
                  (header? "there are numeric fields in the first and only row" :
                           "all fields in the first and only row are of string type") :
                  (header? "all columns are of string type, and a better guess is not possible" :
                           "there are some columns containing only numeric data (even in the first row)"));
      }
    }

    if (sampleLines <= 1) {
      if (header == 1) {
        // A single-row input, and that row is the header. Reset all types to
        // boolean (lowest type possible, a better guess than "string").
        for (int j = 0; j < ncol; j++) {
          types[j] = type0;
        }
        allocnrow = 0;
      }
    } else {
      bytesRead = (size_t)(lastRowEnd - sof);
      meanLineLen = (double)sumLen/sampleLines;
      estnrow = CEIL(bytesRead/meanLineLen);  // only used for progress meter and verbose line below
      double sd = sqrt( (sumLenSq - (sumLen*sumLen)/sampleLines)/(sampleLines-1) );
      allocnrow = clamp_szt((size_t)(bytesRead / fmax(meanLineLen - 2*sd, minLen)),
                            (size_t)(1.1*estnrow), 2*estnrow);
      // sd can be very close to 0.0 sometimes, so apply a +10% minimum
      // blank lines have length 1 so for fill=true apply a +100% maximum. It'll be grown if needed.
      if (verbose) {
        DTPRINT("  =====");
        DTPRINT("  Sampled %zd rows (handled \\n inside quoted fields) at %d jump point(s)", sampleLines, nJumps);
        DTPRINT("  Bytes from first data row on line %d to the end of last row: %zd", row1Line, bytesRead);
        DTPRINT("  Line length: mean=%.2f sd=%.2f min=%d max=%d", meanLineLen, sd, minLen, maxLen);
        DTPRINT("  Estimated number of rows: %zd / %.2f = %zd", bytesRead, meanLineLen, estnrow);
        DTPRINT("  Initial alloc = %zd rows (%zd + %d%%) using bytes/max(mean-2*sd,min) clamped between [1.1*estn, 2.0*estn]",
                 allocnrow, estnrow, (int)(100.0*allocnrow/estnrow-100.0));
      }
      if (nJumps==1) {
        if (header == 1) sampleLines--;
        estnrow = allocnrow = sampleLines;
        g.trace("All rows were sampled since file is small so we know nrow=%zd exactly", estnrow);
      } else {
        ASSERT(sampleLines <= allocnrow);
      }
      if (nrowLimit < allocnrow) {
        g.trace("Alloc limited to nrows=%zd according to the provided max_nrows argument.", nrowLimit);
        estnrow = allocnrow = nrowLimit;
      }
      g.trace("=====");
    }
  }


  //*********************************************************************************************
  // [8] Assign column names (if present)
  //
  //     This section also moves the `sof` pointer to point at the first row
  //     of data ("removing" the column names).
  //*********************************************************************************************
  double tLayout;  // Timer for assigning column names
  const char* colNamesAnchor = sof;
  {
    g.trace("[08] Assign column names");

    ch = sof;  // back to start of first row (likely column names)
    colNames = new lenOff[ncol];
    for (int i = 0; i < ncol; i++) {
      colNames[i].len = 0;
      colNames[i].off = 0;
    }

    if (header == 1) {
      line++;
      if (sep==' ') while (*ch==' ') ch++;
      FieldParseContext fctx = {
        .ch = ch,
        .target = (field64*)colNames,
        .anchor = colNamesAnchor,
      };
      ch--;
      for (int i=0; i<ncol; i++) {
        // Use Field() here as it handles quotes, leading space etc inside it
        ch++;
        parse_string(fctx);  // stores the string length and offset as <uint,uint> in colNames[i]
        fctx.target++;
        if (*ch!=sep) break;
        if (sep==' ') {
          while (ch[1]==' ') ch++;
          if (ch[1]=='\r' || ch[1]=='\n' || ch[1]=='\0') { ch++; break; }
        }
      }
      if (eol(&ch)) {
        sof = ++ch;
      } else {
        ASSERT(*ch=='\0');
        sof = ch;
      }
      // now on first data row (row after column names)
      // when fill=TRUE and column names shorter (test 1635.2), leave calloc initialized lenOff.len==0
    }
    tLayout = wallclock();
  }


  //*********************************************************************************************
  // [9] Allow user to override column types; then allocate the DataTable
  //*********************************************************************************************
  double tColType;    // Timer for applying user column class overrides
  double tAlloc;      // Timer for allocating the DataTable
  int ndrop;          // Number of columns that will be dropped from the file being read
  int nStringCols;    // Number of string columns in the file
  int nNonStringCols; // Number of all other columns in the file
  size_t rowSize;
  size_t DTbytes;     // Size of the allocated DataTable, in bytes
  {
    if (verbose) DTPRINT("[09] Apply user overrides on column types");
    ch = sof;
    memcpy(tmpTypes, types, (size_t)ncol);      // copy types => tmpTypes
    userOverride(types, colNamesAnchor, ncol, quoteRule, quote);  // colNames must not be changed but types[] can be

    int nUserBumped = 0;
    ndrop = 0;
    rowSize = 0;
    nStringCols = 0;
    nNonStringCols = 0;
    for (int j = 0; j < ncol; j++) {
      sizes[j] = typeSize[types[j]];
      if (types[j] == CT_DROP) {
        ndrop++;
        continue;
      }
      rowSize += 8;
      if (types[j] < tmpTypes[j]) {
        // FIXME: if the user wants to override the type, let them
        STOP("Attempt to override column %d \"%.*s\" of inherent type '%s' down to '%s' which will lose accuracy. " \
             "If this was intended, please coerce to the lower type afterwards. Only overrides to a higher type are permitted.",
             j+1, colNames[j].len, colNamesAnchor+colNames[j].off, typeName[tmpTypes[j]], typeName[types[j]]);
      }
      nUserBumped += (types[j] > tmpTypes[j]);
      if (types[j] == CT_STRING) {
        nStringCols++;
      } else {
        nNonStringCols++;
      }
    }
    if (verbose) {
      DTPRINT("  After %d type and %d drop user overrides : %s",
              nUserBumped, ndrop, printTypes(ncol));
    }
    tColType = wallclock();

    if (verbose) {
      DTPRINT("  Allocating %d column slots (%d - %d dropped) with %zd rows",
              ncol-ndrop, ncol, ndrop, allocnrow);
    }
    DTbytes = allocateDT(ncol, ndrop, allocnrow);
    tAlloc = wallclock();
  }


  //*********************************************************************************************
  // [11] Read the data
  //*********************************************************************************************
  bool stopTeam = false;  // bool for MT-safey (cannot ever read half written bool value)
  bool firstTime = true;
  int nTypeBump = 0;
  int nTypeBumpCols = 0;
  double tRead = 0.0;
  double tReread = 0.0;
  double thNextGoodLine = 0.0;
  double thRead = 0.0;
  double thPush = 0.0;  // reductions of timings within the parallel region
  char* typeBumpMsg = NULL;
  size_t typeBumpMsgSize = 0;
  int typeCounts[NUMTYPE];  // used for verbose output; needs populating after first read and before reread (if any) -- see later comment
  #define stopErrSize 1000
  char stopErr[stopErrSize+1] = "";  // must be compile time size: the message is generated and we can't free before STOP
  size_t DTi = 0;   // the current row number in DT that we are writing to
  const char* prevJumpEnd;  // the position after the last line the last thread processed (for checking)
  int buffGrown = 0;
  // chunkBytes is the distance between each jump point; it decides the number of jumps
  // We may want each chunk to write to its own page of the final column, hence 1000*maxLen
  // For the 44GB file with 12875 columns, the max line len is 108,497. We may want each chunk to write to its
  // own page (4k) of the final column, hence 1000 rows of the smallest type (4 byte int) is just
  // under 4096 to leave space for R's header + malloc's header.
  size_t chunkBytes = umax((size_t)(1000*meanLineLen), 1ULL/*MB*/ *1024*1024);
  // Index of the first jump to read. May be modified if we ever need to restart
  // reading from the middle of the file.
  int jump0 = 0;
  // If we need to restart reading the file because we ran out of allocation
  // space, then this variable will tell how many new rows has to be allocated.
  size_t extraAllocRows = 0;
  bool fillme = fill || (ncol==1 && !skipEmptyLines);

  if (nJumps/*from sampling*/ > 1) {
    // ensure data size is split into same sized chunks (no remainder in last chunk) and a multiple of nth
    // when nth==1 we still split by chunk for consistency (testing) and code sanity
    nJumps = (int)(bytesRead/chunkBytes);
    if (nJumps==0) nJumps=1;
    else if (nJumps>nth) nJumps = nth*(1+(nJumps-1)/nth);
    chunkBytes = bytesRead / (size_t)nJumps;
  } else {
    nJumps = 1;
  }
  size_t initialBuffRows = allocnrow / (size_t)nJumps;
  if (initialBuffRows < 4) initialBuffRows = 4;
  ASSERT(initialBuffRows <= INT32_MAX);
  nth = imin(nJumps, nth);

  read:  // we'll return here to reread any columns with out-of-sample type exceptions
  g.trace("[11] Read the data");
  g.trace("jumps=[%d..%d), chunk_size=%zu, total_size=%zd",
          jump0, nJumps, chunkBytes, lastRowEnd-sof);
  ASSERT(allocnrow <= nrowLimit);
  prevJumpEnd = sof;
  if (sep == '\n') sep = '\xFF';

  //-- Start parallel ----------------
  #pragma omp parallel num_threads(nth)
  {
    int me = omp_get_thread_num();
    bool myShowProgress = false;
    #pragma omp master
    {
      nth = omp_get_num_threads();
      myShowProgress = g.show_progress;  // only call `progress()` within the master thread
    }
    const char* thisJumpStart = NULL;  // The first good start-of-line after the jump point
    const char* nextJump = NULL;
    const char* tch = NULL;
    size_t myNrow = 0; // the number of rows in my chunk
    size_t myBuffRows = initialBuffRows;  // Upon realloc, myBuffRows will increase to grown capacity

    // Allocate thread-private row-major myBuffs
    ThreadLocalFreadParsingContext ctx = {
      .anchor = NULL,
      .buff = (field64*) malloc(myBuffRows * rowSize + 8),
      .rowSize = rowSize,
      .DTi = 0,  // which row in the final DT result I should start writing my chunk to
      .nRows = allocnrow,
      .stopTeam = &stopTeam,
      .threadn = me,
      .quoteRule = quoteRule,
      .quote = quote,
      #ifndef DTPY
      .nStringCols = nStringCols,
      .nNonStringCols = nNonStringCols
      #endif
    };
    if (ncol && !ctx.buff) {
      stopTeam = true;
    }
    prepareThreadContext(&ctx);

    FieldParseContext fctx = {
      .ch = tch,
      .target = ctx.buff,
      .anchor = thisJumpStart,
    };

    #pragma omp for ordered schedule(dynamic) reduction(+:thNextGoodLine,thRead,thPush)
    for (int jump = jump0; jump < nJumps; jump++) {
      if (stopTeam) continue;
      double tLast = 0.0;
      if (verbose) tLast = wallclock();
      if (myNrow) {
        // On the 2nd iteration onwards for this thread, push the data from the previous jump
        // Convoluted because the ordered section has to be last in some OpenMP implementations :
        // http://stackoverflow.com/questions/43540605/must-ordered-be-at-the-end
        // Hence why loop goes to nJumps+nth. Logically, this clause belongs after the ordered section.

        // Push buffer now to impl so that :
        //   i) lenoff.off can be "just" 32bit int from a local anchor rather than a 64bit offset from a global anchor
        //  ii) impl can do it in parallel if it wishes, otherwise it can have an orphan critical directive
        // iii) myBuff is hot, so this is the best time to transpose it to result, and first time possible as soon
        //      as we know the previous jump's number of rows.
        //  iv) so that myBuff can be small
        pushBuffer(&ctx);
        myNrow = 0;
        if (verbose || myShowProgress) {
          double now = wallclock();
          thPush += now - tLast;
          tLast = now;
          if (myShowProgress && thPush>=0.75) {
            // Important for thread safety inside progess() that this is called not just from critical but that
            // it's the master thread too, hence me==0. OpenMP doesn't allow '#pragma omp master' here, but we
            // did check above that master's me==0.
            // int ETA = (int)(((now-tAlloc)/jump) * (nJumps-jump));
            progress(100.0*jump/nJumps);
          }
        }
      }

      fctx.target = ctx.buff;
      tch = sof + (size_t)jump * chunkBytes;
      nextJump = jump<nJumps-1 ? tch+chunkBytes+1 : lastRowEnd;
      // +1 is for when nextJump happens to fall exactly on a \n. The
      // next thread will start one line later because nextGoodLine() starts by finding next EOL
      if (jump>0 && !nextGoodLine(&tch, ncol)) {
        #pragma omp critical
        if (!stopTeam) {
          stopTeam = true;
          snprintf(stopErr, stopErrSize, "No good line could be found from jump point %d\n",jump);
        }
        continue;
      }
      thisJumpStart = tch;
      fctx.anchor = tch;

      if (verbose) {
        double now = wallclock();
        thNextGoodLine += now - tLast;
        tLast = now;
      }

      while (tch < nextJump) {  // && myNrow < nrowLimit - myDTi
        if (myNrow == myBuffRows) {
          // buffer full due to unusually short lines in this chunk vs the sample; e.g. #2070
          myBuffRows *= 1.5;
          #pragma omp atomic
          buffGrown++;
          ctx.buff = (field64*) realloc(ctx.buff, rowSize * myBuffRows + 8);
          if (ncols && !ctx.buff) {
            stopTeam = true;
            break;
          }
          // shift current buffer positions, since `myBuffX`s were probably moved by realloc
          fctx.target = ctx.buff + myNrow * (rowSize / 8);
        }
        const char* tlineStart = tch;  // for error message
        const char* fieldStart = tch;
        int j = 0;

        //*** START HOT ***//
        if (sep!=' ' && !any_number_like_NAstrings) {  // TODO:  can this 'if' be dropped somehow? Can numeric NAstrings be dealt with afterwards in one go as numeric comparison?
          // Try most common and fastest branch first: no whitespace, no quoted numeric, ",," means NA
          while (j < ncol) {
            fieldStart = tch;
            // fetch shared type once. Cannot read half-written byte is one reason type's type is single byte to avoid atomic read here.
            int8_t thisType = types[j];
            parsers[abs(thisType)](fctx);
            if (*tch != sep) break;
            if (sizes[j]) {
              fctx.target++;
            }
            tch++;
            j++;
          }
          //*** END HOT. START TEPID ***//
          if (tch == tlineStart) {
            skip_white(&tch);
            if (*tch=='\0') break;  // empty last line
            if (eol(&tch) && skipEmptyLines) { tch++; continue; }
            tch = tlineStart;  // in case white space at the beginning may need to be included in field
          }
          else if (eol(&tch)) {
            if (sizes[j]) {
              fctx.target++;
            }
            j++;
            if (j==ncol) { tch++; myNrow++; continue; }  // next line. Back up to while (tch<nextJump). Usually happens, fastest path
          }
          else {
            tch = fieldStart; // restart field as int processor could have moved to A in ",123A,"
          }
          // if *tch=='\0' then *eof in mind, fall through to below and, if finalByte is set, reread final field
        }
        //*** END TEPID. NOW COLD.

        // Either whitespace surrounds field in which case the processor will fault very quickly, it's numeric but quoted (quote will fault the non-string processor),
        // it contains an NA string, or there's an out-of-sample type bump needed.
        // In all those cases we're ok to be a bit slower. The rest of this line will be processed using the slower version.
        // (End-of-file) is also dealt with now, as could be the highly unusual line ending /n/r
        // This way (each line has new opportunity of the fast path) if only a little bit of the file is quoted (e.g. just when commas are present as fwrite does)
        // then a penalty isn't paid everywhere.
        // TODO: reduce(slowerBranch++). So we can see in verbose mode if this is happening too much.

        if (sep==' ') {
          while (*tch==' ') tch++;  // multiple sep=' ' at the tlineStart does not mean sep. We're at tLineStart because the fast branch above doesn't run when sep=' '
          fieldStart = tch;
          if (eol(&tch) && skipEmptyLines) { tch++; continue; }
        }

        if (fillme || (*tch!='\n' && *tch!='\r')) {  // also includes the case when sep==' '
          while (j < ncol) {
            fieldStart = tch;
            int8_t joldType = types[j];
            int8_t thisType = joldType;  // to know if it was bumped in (rare) out-of-sample type exceptions
            int8_t absType = (int8_t)abs(thisType);

            while (absType < NUMTYPE) {
              tch = fieldStart;
              bool quoted = false;
              if (absType < CT_STRING && absType > CT_DROP) {
                skip_white(&tch);
                const char* afterSpace = tch;
                tch = end_NA_string(fieldStart);
                skip_white(&tch);
                if (!end_of_field(tch)) tch = afterSpace; // else it is the field_end, we're on closing sep|eol and we'll let processor write appropriate NA as if field was empty
                if (*tch==quote) { quoted=true; tch++; }
              } // else Field() handles NA inside it unlike other processors e.g. ,, is interpretted as "" or NA depending on option read inside Field()
              parsers[absType](fctx);
              if (quoted && *tch==quote) tch++;
              skip_white(&tch);
              if (end_of_field(tch)) {
                if (sep==' ' && *tch==' ') {
                  while (tch[1]==' ') tch++;  // multiple space considered one sep so move to last
                  if (tch[1]=='\r' || tch[1]=='\n' || tch[1]=='\0') tch++;
                }
                break;
              }

              // guess is insufficient out-of-sample, type is changed to negative sign and then bumped. Continue to
              // check that the new type is sufficient for the rest of the column (and any other columns also in out-of-sample bump status) to be
              // sure a single re-read will definitely work.
              absType++;
              // while (disabled_parsers[absType]) absType++;
              thisType = -absType;
              tch = fieldStart;
            }

            if (thisType != joldType) {          // rare out-of-sample type exception.
              #pragma omp critical
              {
                joldType = types[j];  // fetch shared value again in case another thread bumped it while I was waiting.
                // Can't print because we're likely not master. So accumulate message and print afterwards.
                if (thisType < joldType) {   // thisType<0 (type-exception)
                  if (verbose) {
                    char temp[1001];
                    int len = snprintf(temp, 1000,
                      "Column %d (\"%.*s\") bumped from '%s' to '%s' due to <<%.*s>> on row %llu\n",
                      j+1, colNames[j].len, colNamesAnchor + colNames[j].off,
                      typeName[abs(joldType)], typeName[abs(thisType)],
                      (int)(tch-fieldStart), fieldStart, (llu)(ctx.DTi+myNrow));
                    typeBumpMsg = (char*) realloc(typeBumpMsg, typeBumpMsgSize + (size_t)len + 1);
                    strcpy(typeBumpMsg+typeBumpMsgSize, temp);
                    typeBumpMsgSize += (size_t)len;
                  }
                  nTypeBump++;
                  if (joldType>0) nTypeBumpCols++;
                  types[j] = thisType;
                } // else another thread just bumped to a (negative) higher or equal type while I was waiting, so do nothing
              }
            }
            if (sizes[j]) {
              fctx.target++;
            }
            j++;
            if (*tch==sep) { tch++; continue; }
            if (fill && (*tch=='\n' || *tch=='\r' || *tch=='\0') && j <= ncol) {
              // Reuse processors to write appropriate NA to target; saves maintenance of a type switch down here.
              // This works for all processors except CT_STRING, which write "" value instead of NA -- hence this
              // case should be handled explicitly.
              if (joldType == CT_STRING && sizes[j-1] == 8 && fctx.target[-1].str32.len == 0) {
                fctx.target[-1].str32.len = INT32_MIN;
              }
              continue;
            }
            break;
          }
        }

        if (j < ncol)  {
          // not enough columns observed (including empty line). If fill==true, fields should already have been filled above due to continue inside while(j<ncol)
          #pragma omp critical
          if (!stopTeam) {
            stopTeam = true;
            snprintf(stopErr, stopErrSize,
              "Expecting %d cols but row %zu contains only %d cols (sep='%c'). "
              "Consider fill=true. \"%s\"",
              ncol, ctx.DTi, j, sep, strlim(tlineStart, 500));
          }
          break;
        }
        if (!eol(&tch) && *tch!='\0') {
          #pragma omp critical
          if (!stopTeam) {
            stopTeam = true;
            snprintf(stopErr, stopErrSize,
              "Too many fields on out-of-sample row %zu from jump %d. Read all %d "
              "expected columns but more are present. \"%s\"",
              ctx.DTi, jump, ncol, strlim(tlineStart, 500));
          }
          break;
        }
        if (*tch!='\0') tch++;
        myNrow++;
      }
      if (verbose) {
        double now = wallclock();
        thRead += now - tLast;
        tLast = now;
      }
      ctx.anchor = thisJumpStart;
      ctx.nRows = myNrow;
      postprocessBuffer(&ctx);

      #pragma omp ordered
      {
        // stopTeam could be true if a previous thread already stopped while I was waiting my turn
        if (!stopTeam && prevJumpEnd != thisJumpStart && jump > jump0) {
          snprintf(stopErr, stopErrSize,
            "Jump %d did not finish counting rows exactly where jump %d found its first good line start: "
            "prevEnd(%p)\"%s\" != thisStart(prevEnd%+d)\"%s\"",
            jump-1, jump, (const void*)prevJumpEnd, strlim(prevJumpEnd, 50),
            (int)(thisJumpStart-prevJumpEnd), strlim(thisJumpStart, 50));
          stopTeam = true;
        }
        ctx.DTi = DTi;  // fetch shared DTi (where to write my results to the answer). The previous thread just told me.
        if (ctx.DTi >= allocnrow) {  // a previous thread has already reached the `allocnrow` limit
          stopTeam = true;
          myNrow = 0;
        } else if (myNrow + ctx.DTi > allocnrow) {  // current thread has reached `allocnrow` limit
          if (allocnrow == nrowLimit) {
            // allocnrow is the same as nrowLimit, no need to reallocate the DT,
            // just truncate the rows in the current chunk.
            myNrow = nrowLimit - ctx.DTi;
          } else {
            // We reached `allocnrow` limit, but there are more data to read
            // left. In this case we arrange to terminate all threads but
            // remember the position where the previous thread has finished. We
            // will reallocate the DT and restart reading from the same point.
            jump0 = jump;
            if (jump < nJumps - 1) {
              extraAllocRows = (size_t)((double)(DTi+myNrow)*nJumps/(jump+1) * 1.2) - allocnrow;
              if (extraAllocRows < 1024) extraAllocRows = 1024;
            } else {
              // If we're on the last jump, then we know exactly how many extra rows is needed.
              extraAllocRows = DTi + myNrow - allocnrow;
            }
            myNrow = 0;
            stopTeam = true;
          }
        }
                           // tell next thread (she not me) 2 things :
        prevJumpEnd = tch; // i) the \n I finished on so she can check (above) she started exactly on that \n good line start
        DTi += myNrow;     // ii) which row in the final result she should start writing to since now I know myNrow.
        ctx.nRows = myNrow;
        if (!stopTeam) orderBuffer(&ctx);
      }
      // END ORDERED.
      // Next thread can now start its ordered section and write its results to the final DT at the same time as me.
      // Ordered has to be last in some OpenMP implementations currently. Logically though, pushBuffer happens now.
    }
    // Push out all buffers one last time.
    if (myNrow) {
      double now = verbose? wallclock() : 0;
      pushBuffer(&ctx);
      if (verbose) thRead += wallclock() - now;
    }
    // Done reading the file: each thread should now clean up its own buffers.
    free(ctx.buff); ctx.buff = NULL;
    freeThreadContext(&ctx);
  }
  //-- end parallel ------------------


  if (stopTeam && stopErr[0]!='\0') {
    STOP(stopErr);
  } else {
    // nrowLimit applied and stopped early normally
    stopTeam = false;
  }
  if (DTi>allocnrow && nrowLimit>allocnrow) {
    STOP("Internal error: DTi(%llu) > allocnrow(%llu) but nrows=%llu (not limited)",
         (llu)DTi, (llu)allocnrow, (llu)nrowLimit);
    // for the last jump that fills nrow limit, then ansi is +=buffi which is >allocnrow and correct
  }

  if (extraAllocRows) {
    allocnrow += extraAllocRows;
    if (allocnrow > nrowLimit) allocnrow = nrowLimit;
    if (verbose) {
      DTPRINT("  Too few rows allocated. Allocating additional %llu rows "
              "(now nrows=%llu) and continue reading from jump point %d",
              (llu)extraAllocRows, (llu)allocnrow, jump0);
    }
    allocateDT(ncol, ncol - nStringCols - nNonStringCols, allocnrow);
    extraAllocRows = 0;
    stopTeam = false;
    goto read;   // jump0>0 at this point, set above
  }

  // tell progress meter to finish up; e.g. write final newline
  // if there's a reread, the progress meter will start again from 0
  if (g.show_progress && thPush >= 0.75) progress(100.0);
  setFinalNrow(DTi);

  if (firstTime) {
    tReread = tRead = wallclock();

    // if nTypeBump>0, not-bumped columns are about to be assigned parse type -CT_STRING for the reread, so we have to count
    // parse types now (for log). We can't count final column types afterwards because many parse types map to the same column type.
    for (int i=0; i<NUMTYPE; i++) typeCounts[i] = 0;
    for (int i=0; i<ncol; i++) typeCounts[ abs(types[i]) ]++;

    if (nTypeBump) {
      rowSize = 0;
      nStringCols = 0;
      nNonStringCols = 0;
      for (int j=0, resj=-1; j<ncol; j++) {
        if (types[j] == CT_DROP) continue;
        resj++;
        if (types[j]<0) {
          // column was bumped due to out-of-sample type exception
          types[j] = -types[j];
          sizes[j] = typeSize[types[j]];
          assert(sizes[j] != 0);
          rowSize += 8;
          if (types[j] == CT_STRING) nStringCols++; else nNonStringCols++;
        } else if (types[j]>=1) {
          // we'll skip over non-bumped columns in the rerun, whilst still incrementing resi (hence not CT_DROP)
          // not -type[i] either because that would reprocess the contents of not-bumped columns wastefully
          types[j] = -CT_STRING;
          sizes[j] = 0;
        }
      }
      allocateDT(ncol, ncol - nStringCols - nNonStringCols, DTi);
      // reread from the beginning
      DTi = 0;
      prevJumpEnd = sof;
      firstTime = false;
      nTypeBump = 0;   // for test 1328.1. Otherwise the last field would get shifted forwards again.
      jump0 = 0;       // for #2486
      goto read;
    }
  } else {
    tReread = wallclock();
  }

  double tTot = tReread-t0;  // tReread==tRead when there was no reread
  g.trace("Read %zu rows x %d columns from %s file in %02d:%06.3f wall clock time",
          DTi, ncol-ndrop, filesize_to_str(fileSize), (int)tTot/60, fmod(tTot,60.0));



  //*********************************************************************************************
  // [12] Finalize the datatable
  //*********************************************************************************************
  g.trace("[12] Finalizing the datatable");
  // setFinalNrow(DTi);

  if (verbose) {
    DTPRINT("=============================");
    if (tTot < 0.000001) tTot = 0.000001;  // to avoid nan% output in some trivially small tests where tot==0.000s
    DTPRINT("%8.3fs (%3.0f%%) sep, ncol and header detection", tLayout-t0, 100.0*(tLayout-t0)/tTot);
    DTPRINT("%8.3fs (%3.0f%%) Column type detection using %zd sample rows",
            tColType-tLayout, 100.0*(tColType-tLayout)/tTot, sampleLines);
    DTPRINT("%8.3fs (%3.0f%%) Allocation of %llu rows x %d cols (%.3fGB) of which %llu (%3.0f%%) rows used",
            tAlloc-tColType, 100.0*(tAlloc-tColType)/tTot, (llu)allocnrow, ncol,
            DTbytes/(1024.0*1024*1024), (llu)DTi, 100.0*DTi/allocnrow);
    thNextGoodLine /= nth;
    thRead /= nth;
    thPush /= nth;
    double thWaiting = tReread - tAlloc - thNextGoodLine - thRead - thPush;
    DTPRINT("%8.3fs (%3.0f%%) Reading %d chunks of %.3fMB (%d rows) using %d threads",
            tReread-tAlloc, 100.0*(tReread-tAlloc)/tTot, nJumps, (double)chunkBytes/(1024*1024),
            (int)(chunkBytes/meanLineLen), nth);
    DTPRINT("   = %8.3fs (%3.0f%%) Finding first non-embedded \\n after each jump",
            thNextGoodLine, 100.0*thNextGoodLine/tTot);
    DTPRINT("   + %8.3fs (%3.0f%%) Parse to row-major thread buffers (grown %d times)",
            thRead, 100.0*thRead/tTot, buffGrown);
    DTPRINT("   + %8.3fs (%3.0f%%) Transpose", thPush, 100.0*thPush/tTot);
    DTPRINT("   + %8.3fs (%3.0f%%) Waiting", thWaiting, 100.0*thWaiting/tTot);
    DTPRINT("%8.3fs (%3.0f%%) Rereading %d columns due to out-of-sample type exceptions",
            tReread-tRead, 100.0*(tReread-tRead)/tTot, nTypeBumpCols);
    DTPRINT("%8.3fs        Total", tTot);
    if (typeBumpMsg) {
      // if type bumps happened, it's useful to see them at the end after the timing 2 lines up showing the reread time
      // TODO - construct and output the copy and pastable colClasses argument so user can avoid the reread time if they are
      //        reading this file or files formatted like it many times (say in a production environment).
      DTPRINT("%s", typeBumpMsg);
      free(typeBumpMsg);  // local scope and only populated in verbose mode
    }
  }
  freadCleanup();
  return 1;
}
