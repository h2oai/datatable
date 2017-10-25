#include "fread.h"
#include "freadLookups.h"
#ifdef WIN32             // means WIN64, too, oddly
  #include <windows.h>
#else
  #include <ctype.h>     // isspace
  #include <errno.h>     // errno
  #include <string.h>    // strerror
  #include <stdarg.h>    // va_list, va_start
  #include <stdio.h>     // vsnprintf
  #include <sys/mman.h>  // mmap
  #include <math.h>      // ceil, sqrt, isfinite
#endif
#include <stdbool.h>     // bool, true, false
#include "file.h"


// Private globals to save passing all of them through to highly iterated field processors
static char sep, eol, eol2;
static char whiteChar; // what to consider as whitespace to skip: ' ', '\t' or 0 means both (when sep!=' ' && sep!='\t')
static int eolLen;
static char quote, dec;

// Quote rule:
//   0 = Fields may be quoted, any quote inside the field is doubled. This is
//       the CSV standard. For example: <<...,"hello ""world""",...>>
//   1 = Fields may be quoted, any quotes inside are escaped with a backslash.
//       For example: <<...,"hello \"world\"",...>>
//   2 = Fields may be quoted, but any quotes inside will appear verbatim and
//       not escaped in any way. It is not always possible to parse the file
//       unambiguously, but we give it a try anyways. A quote will be presumed
//       to mark the end of the field iff it is followed by the field separator.
//       Under this rule eol characters cannot appear inside the field.
//       For example: <<...,"hello "world"",...>>
//   3 = Fields are not quoted at all. Any quote characters appearing anywhere
//       inside the field will be treated as any other regular characters.
//       Example: <<...,hello "world",...>>
//
static int quoteRule;
static const char* const* NAstrings;
static bool any_number_like_NAstrings=false;
static bool blank_is_a_NAstring=false;
static bool stripWhite=true;  // only applies to character columns; numeric fields always stripped
static bool skipEmptyLines=false, fill=false;

static double NA_FLOAT64;  // takes fread.h:NA_FLOAT64_VALUE
static float NA_FLOAT32;

#define JUMPLINES 100    // at each of the 100 jumps how many lines to guess column types (10,000 sample lines)


//------------------------------------------------------------------------------
// Private globals used during the fread-ing session. They will be reset in
// `freadCleanup()` both on successful exit, and on error.
//------------------------------------------------------------------------------
// Pointer to the memory region where the file being read is mapped
static void *mmp = NULL;
// Extra memory-mapped region, used only on Unix platforms and only when file's
// size is a multiple of the page size
static void *xmmp = NULL;

static size_t fileSize;
static char *lineCopy = NULL;
static int8_t *type = NULL, *size = NULL;
static lenOff *colNames = NULL;
static int8_t *oldType = NULL;
static freadMainArgs args;  // global for use by DTPRINT

const char typeName[NUMTYPE][10] = {"drop", "bool8", "int32", "int32", "int64", "float32", "float64", "float64", "float64", "string"};
int8_t     typeSize[NUMTYPE]     = { 0,      1,       4,       4,       8,      4,         8,         8,         8,         8       };
// size_t to prevent potential overflow of n*typeSize[i] (standard practice)

// NAN and INFINITY constants are float, so cast to double once up front.
static const double NAND = (double)NAN;
static const double INFD = (double)INFINITY;

// Forward declarations
static int Field(const char **pch, lenOff *target);
static int parse_string_continue(const char **ptr, lenOff *target);



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
void freadCleanup(void)
{
  free(type); type = NULL;
  free(size); size = NULL;
  free(lineCopy); lineCopy = NULL;
  free(colNames); colNames = NULL;
  free(oldType); oldType = NULL;
  if (mmp != NULL) {
    // Important to unmap as OS keeps internal reference open on file.
    // Note that if there was an error unmapping the view of file, then we should not attempt
    // to call STOP() for 2 reasons: 1) freadCleanup() may have itself been called from STOP(),
    // and we would not want to overwrite the original error message; and 2) STOP() function
    // may call freadCleanup(), thus resulting in an infinite loop.
    #ifdef WIN32
      int ret = UnmapViewOfFile(mmp);
      if (!ret) DTPRINT("System error %d unmapping view of file\n", GetLastError());
    #else
      int ret = munmap(mmp, fileSize);
      if (ret) DTPRINT("System errno %d unmapping file\n", errno);
      if (xmmp) {
        munmap(xmmp, 1);
        xmmp = NULL;
      }
    #endif
    mmp = NULL;
  }
  fileSize = 0;
  sep = whiteChar = eol = eol2 = quote = dec = '\0';
  eolLen = 0;
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
static inline size_t umin(size_t a, size_t b) { return a < b ? a : b; }
static inline int imin(int a, int b) { return a < b ? a : b; }

/** Return value of `x` clamped to the range [upper, lower] */
static inline size_t clamp_szt(size_t x, size_t lower, size_t upper) {
  return x < lower ? lower : x > upper? upper : x;
}


/**
 * Helper for error and warning messages to extract an input line starting at
 * `*ch` and until an end of line, but no longer than `limit` characters.
 * This function returns the string copied into an internal static buffer. Cannot
 * be called more than twice per single printf() invocation.
 * Parameter `limit` cannot exceed 500.
 */
static const char* strlim(const char *ch, size_t limit, const char *eof) {
  static char buf[1002];
  static int flip = 0;
  char *ptr = buf + 501 * flip;
  flip = 1 - flip;
  char *ch2 = ptr;
  size_t width = 0;
  while ((*ch>'\r' || (*ch!='\0' && *ch!='\r' && *ch!='\n')) && width++<limit && ch<eof) *ch2++ = *ch++;
  *ch2 = '\0';
  return ptr;
}



static void printTypes(int ncol) {
  // e.g. files with 10,000 columns, don't print all of it to verbose output.
  int tt=(ncol<=110?ncol:90); for (int i=0; i<tt; i++) DTPRINT("%d",type[i]);
  if (ncol>110) { DTPRINT("..."); for (int i=ncol-10; i<ncol; i++) DTPRINT("%d",type[i]); }
}

static inline void skip_white(const char **pch) {
  // skip space so long as sep isn't space and skip tab so long as sep isn't tab
  const char *ch = *pch;
  if (whiteChar == 0) {   // whiteChar==0 means skip both ' ' and '\t';  sep is neither ' ' nor '\t'.
    while (*ch == ' ' || *ch == '\t') ch++;
  } else {
    while (*ch == whiteChar) ch++;  // sep is ' ' or '\t' so just skip the other one.
  }
  *pch = ch;
}


static inline bool on_sep(const char **pch) {
  const char *ch = *pch;
  if (sep==' ' && *ch==' ') {
    while (*(ch+1)==' ') ch++;  // move to last of this sequence of spaces
    if (*(ch+1)==eol) ch++;    // if that's followed by eol then move over
  }
  *pch = ch;
  return *ch==sep || *ch==eol;
}

static inline void next_sep(const char **pch) {
  const char *ch = *pch;
  while (*ch!=sep && *ch!=eol) ch++;
  on_sep(&ch); // to deal with multiple spaces when sep==' '
  *pch = ch;
}

static inline bool is_NAstring(const char *fieldStart) {
  skip_white(&fieldStart);  // updates local fieldStart
  const char* const* nastr = NAstrings;
  while (*nastr) {
    const char *ch1 = fieldStart;
    const char *ch2 = *nastr;
    while (*ch1 == *ch2) { ch1++; ch2++; }  // not using strncmp due to eof not being '\0'
    if (*ch2=='\0') {
      skip_white(&ch1);
      if (*ch1==sep || *ch1==eol) return true;
      // if "" is in NAstrings then true will be returned as intended
    }
    nastr++;
  }
  return false;
}


/**
 * Compute the number of fields on the current line (taking into account the
 * global `sep`, `eol` and `quoteRule`), and move the parsing location to the
 * beginning of the next line.
 * Returns the number of fields on the current line, or -1 if the line cannot
 * be parsed using current settings.
 */
static inline int countfields(const char **pch, const char **end, const char *soh, const char *eoh)
{
  static lenOff trash;  // target for writing out parsed fields
  const char *tend = *end;
  const char *ch = *pch;
  if (sep==' ') while (*ch==' ') ch++;  // multiple sep==' ' at the start does not mean sep
  skip_white(&ch);

  int ncol = 0;
  if (*ch==eol) {
    *pch = ch + eolLen;
    return 0;
  }
  while (1) {
    int res = Field(&ch, &trash);
    if (res == 1) return -1;
    if (res == 2) {
      int linesCount = 0;
      while (res == 2 && linesCount++ < 100) {
        if (ch == tend) {
          if (eoh && tend != eoh) {
            ch = soh;
            tend = eoh;
          } else {
            return -1;
          }
        }
        res = parse_string_continue(&ch, &trash);
      }
    }
    // Field() leaves *ch resting on sep or eol. Checked inside Field().
    ncol++;
    if (*ch==eol) { ch+=eolLen; break; }
    ch++;  // move over sep (which will already be last ' ' if sep=' ').
  }
  *pch = ch;
  *end = tend;
  return ncol;
}



static inline bool nextGoodLine(const char **pch, int ncol, const char *eof)
{
  const char *ch = *pch;
  // we may have landed inside quoted field containing embedded sep and/or embedded \n
  // find next \n and see if 5 good lines follow. If not try next \n, and so on, until we find the real \n
  // We don't know which line number this is, either, because we jumped straight to it. So return true/false for
  // the line number and error message to be worked out up there.
  int attempts=0;
  while (ch<eof && attempts++<30) {
    while (*ch!=eol) ch++;
    ch += eolLen;
    int i = 0, thisNcol=0;
    const char *ch2 = ch;
    const char *end = eof;
    while (ch2<eof && i<5 && ( (thisNcol=countfields(&ch2, &end, NULL, NULL))==ncol ||
                               (thisNcol==0 && (skipEmptyLines || fill)))) i++;
    if (i==5 || ch2>=eof) break;
  }
  if (ch<eof && attempts<30) { *pch = ch; return true; }
  return false;
}


static int advance_sof_to(const char *newsof, const char **sof,
                          const char **eof, const char **soh, const char **eoh)
{
  intptr_t d = 0;
  if (*sof <= newsof && newsof < *eof) {
    d = newsof - *sof;
    *sof = newsof;
  } else if (newsof == *eof || newsof == *soh) {
    d = *eof - *sof;
    *sof = *soh;
    *eof = *eoh;
    *soh = *eoh = NULL;
  } else if (*soh < newsof && newsof <= *eoh) {
    d = (*eof - *sof) + (newsof - *soh);
    *sof = newsof;
    *eof = *eoh;
    *soh = *eoh = NULL;
  }
  if (d && args.verbose)
    DTPRINT("  Start-of-file pointer moved %zd bytes forward\n", d);
  return 1;
}


static int retreat_eof_to(const char *neweof, const char **sof,
                          const char **eof, const char **soh, const char **eoh)
{
  intptr_t d = 0;
  if (*soh < neweof && neweof <= *eoh) {
    d = *eoh - neweof;
    *eoh = neweof;
  } else if (neweof == *soh || neweof == *eof) {
    d = *eoh - *soh;
    *soh = *eoh = NULL;
  } else if (*sof <= neweof && neweof <= *eof) {
    d = (*eof - neweof) + (*eoh - *soh);
    *eof = neweof;
    *soh = *eoh = NULL;
  }
  if (d && args.verbose)
    DTPRINT("  End-of-file pointer moved %zd bytes backward\n", d);
  return 1;
}



//=================================================================================================
//
//   Field parsers
//
//=================================================================================================

static int Field(const char **pch, lenOff *target)
{
  const char *ch = *pch;
  if (stripWhite) skip_white(&ch);  // before and after quoted field's quotes too (e.g. test 1609) but never inside quoted fields
  const char *fieldStart=ch;
  bool quoted = false;

  if (*ch!=quote || quoteRule==3) {
    // unambiguously not quoted. simply search for sep|eol. If field contains sep|eol then it must be quoted instead.
    while(*ch!=sep && *ch!=eol) ch++;
  } else {
    // the field is quoted and quotes are correctly escaped (quoteRule 0 and 1)
    // or the field is quoted but quotes are not escaped (quoteRule 2)
    // or the field is not quoted but the data contains a quote at the start (quoteRule 2 too)
    quoted = true;
    fieldStart = ch+1; // step over opening quote
    switch(quoteRule) {
      case 0: {
        // Rule 0: the field is quoted and all internal quotes are doubled.
        // The field may have embedded newlines. The field ends when the first
        // undoubled quote character is encountered.
        ch = fieldStart;
        while (*ch!=eol) {
          if (*ch==quote) {
            if (ch[1] == quote) ch++;
            else break;
          }
          ch++;
        }
        if (*ch==eol) {
          target->len = (int32_t)(ch - fieldStart) + eolLen;
          target->off = (int32_t)(fieldStart - *pch);
          *pch = ch + eolLen;
          return 2;
        }
        break;
      }
      case 1: {
        // Rule 1: the field is quoted and all internal quotes are escaped with
        // the backslash character. The field is allowed to have embedded
        // newlines. The field ends when the first unescaped quote is found.
        ch = fieldStart;
        while (*ch!=eol && *ch!=quote) {
          ch += 1 + (*ch == '\\' && ch[1] != eol);
        }
        if (*ch==eol) {
          target->len = (int32_t)(ch - fieldStart) + eolLen;
          target->off = (int32_t)(fieldStart - *pch);
          *pch = ch + eolLen;
          return 2;
        }
        break;
      }
      case 2: {
        // Rule 2: the field is either unquoted (no quotes inside are allowed), or
        // it was quoted but any internal quotation marks were not escaped. This
        // is a "sloppy" rule: it does not allow to parse input unambiguously. We
        // will assume that a quoted field ends when we see a quote character
        // followed by a separator. This rule doesn't allow embedded newlines
        // inside fields.
        const char *ch2 = ch;
        while (*(++ch)!=eol) {
          if (*ch==quote && (*(ch+1)==sep || *(ch+1)==eol)) {ch2=ch; break;}   // (*1) regular ", ending
          if (*ch==sep) {
            // first sep in this field
            // if there is a ", afterwards but before the next \n, use that; the field was quoted and it's still case (i) above.
            // Otherwise break here at this first sep as it's case (ii) above (the data contains a quote at the start and no sep)
            ch2 = ch;
            while (*(++ch2)!=eol) {
              if (*ch2==quote && (*(ch2+1)==sep || *(ch2+1)==eol)) {
                ch = ch2; // (*2) move on to that first ", -- that's this field's ending
                break;
              }
            }
            break;
          }
        }
        if (ch!=ch2) { fieldStart--; quoted=false; } // field ending is this sep (neither (*1) or (*2) happened)
        break;
      }
    }
  }
  int fieldLen = (int)(ch-fieldStart);
  if (quoted) {
    ch++;
    if (stripWhite) skip_white(&ch);
  } else if (stripWhite) {
    // Remove trailing whitespace: note that we don't move ch pointer, merely
    // adjust the field length.
    // This white space (' ' or '\t') can't be sep otherwise it would have
    // stopped the field earlier at the first sep.
    while(fieldLen>0 && (fieldStart[fieldLen-1]==' ' || fieldStart[fieldLen-1]=='\t')) fieldLen--;
  }
  if (!on_sep(&ch)) return 1;  // Field ended unexpectedly: cannot happen under quoteRule 3
  if (fieldLen==0) {
    if (blank_is_a_NAstring) fieldLen=INT32_MIN;
  } else {
    if (is_NAstring(fieldStart)) fieldLen=INT32_MIN;
  }
  target->len = fieldLen;
  target->off = (int32_t)(fieldStart - *pch);
  *pch = ch; // Update caller's ch. This may be after fieldStart+fieldLen due to quotes and/or whitespace
  return 0;
}


static int parse_string_continue(const char **ptr, lenOff *target)
{
  const char *ch = *ptr;
  ASSERT(quoteRule <= 1);
  if (quoteRule == 0) {
    while (*ch != eol) {
      if (*ch == quote) {
        if (ch[1] == quote) ch++;
        else break;
      }
      ch++;
    }
  } else {
    while (*ch != eol && *ch != quote) {
      ch += 1 + (*ch == '\\' && ch[1] != eol);
    }
  }
  if (*ch == eol) {
    target->len += (int32_t)(ch - *ptr + eolLen);
    *ptr = ch + eolLen;
    return 2;
  } else {
    ASSERT(*ch == quote);
    ch++;
    if (stripWhite) skip_white(&ch);
    if (!on_sep(&ch)) {
      return 1;
    }
    target->len += (int32_t)(ch - *ptr - 1);  // -1 removes closing quote
    *ptr = ch;
    return 0;
  }
}


static int StrtoI64(const char **pch, int64_t *target)
{
  // Specialized clib strtoll that :
  // i) skips leading isspace() too but other than field separator and eol (e.g. '\t' and ' \t' in FUT1206.txt)
  // ii) has fewer branches for speed as no need for non decimal base
  // iii) updates global ch directly saving arguments
  // iv) safe for mmap which can't be \0 terminated on Windows (but can be on unix and mac)
  // v) fails if whole field isn't consumed such as "3.14" (strtol consumes the 3 and stops)
  // ... all without needing to read into a buffer at all (reads the mmap directly)
  const char *ch = *pch;
  skip_white(&ch);  //  ',,' or ',   ,' or '\t\t' or '\t   \t' etc => NA
  if (on_sep(&ch)) {  // most often ',,'
    *target = NA_INT64;
    *pch = ch;
    return 0;
  }
  const char *start=ch;
  int sign=1;
  bool quoted = false;
  if (*ch==quote) { quoted=true; ch++; }
  if (*ch=='-' || *ch=='+') sign -= 2*(*ch++=='-');
  bool ok = '0'<=*ch && *ch<='9';  // a single - or + with no [0-9] is !ok and considered type character
  int64_t acc = 0;
  while ('0'<=*ch && *ch<='9' && acc<(INT64_MAX-10)/10) { // compiler should optimize last constant expression
    // Conveniently, INT64_MIN == -9223372036854775808 and INT64_MAX == +9223372036854775807
    // so the full valid range is now symetric [-INT64_MAX,+INT64_MAX] and NA==INT64_MIN==-INT64_MAX-1
    acc *= 10;
    acc += *ch-'0';
    ch++;
  }
  if (quoted) { if (*ch!=quote) return 1; else ch++; }
  // TODO: if (!targetCol) return early?  Most of the time, not though.
  *target = sign * acc;
  skip_white(&ch);
  ok = ok && on_sep(&ch);
  //DTPRINT("StrtoI64 field '%.*s' has len %d\n", lch-ch+1, ch, len);
  *pch = ch;
  if (ok && !any_number_like_NAstrings) return 0;  // most common case, return
  bool na = is_NAstring(start);
  if (ok && !na) return 0;
  *target = NA_INT64;
  next_sep(&ch);  // TODO: can we delete this? consume the remainder of field, if any
  *pch = ch;
  return !na;
}


static int StrtoI32_bare(const char **pch, int32_t *target)
{
  const char *ch = *pch;
  if (*ch==sep || *ch==eol) { *target = NA_INT32; return 0; }
  if (sep==' ') return 1;  // bare doesn't do sep=' '. TODO - remove
  bool neg = *ch=='-';
  ch += (neg || *ch=='+');
  const char *start = ch;  // for overflow guard using field width
  uint_fast64_t acc = 0;   // using unsigned to be clear that acc will never be negative
  uint_fast8_t digit;
  while ( (digit=(uint_fast8_t)(*ch-'0'))<10 ) {  // see init.c for checks of unsigned uint_fast8_t) cast
    acc = acc*10 + digit;     // optimizer has best chance here; e.g. (x<<2 + x)<<1 or x<<3 + x<<1
    ch++;
  }
  *target = neg ? -(int32_t)acc : (int32_t)acc;   // cast 64bit acc to 32bit; range checked in return below
  *pch = ch;
  return (*ch!=sep && *ch!=eol) ||
         (acc ? *start=='0' || acc>INT32_MAX || (ch-start)>10 : ch-start!=1);
  // If false, both *target and where *pch is moved on to, are undefined.
  // INT32 range is NA==-2147483648(INT32_MIN) then symmetric [-2147483647,+2147483647] so we can just test INT32_MAX
  // The max (2147483647) happens to be 10 digits long, hence <=10.
  // Leading 0 (such as 001 and 099 but not 0, +0 or -0) will return false and cause bump to _padded which has the
  // option to treat as integer or string with further cost. Otherwise width would not be sufficient check here in _bare.
}


static int StrtoI32_full(const char **pch, int32_t *target)
{
  // Very similar to StrtoI64 (see it for comments). We can't make a single function and switch on TYPEOF(targetCol) to
  // know I64 or I32 because targetCol is NULL when testing types and when dropping columns.
  const char *ch = *pch;
  skip_white(&ch);
  if (on_sep(&ch)) {  // most often ',,'
    *target = NA_INT32;
    *pch = ch;
    return 0;
  }
  const char *start=ch;
  int sign=1;
  bool quoted = false;
  if (*ch==quote) { quoted=true; ch++; }
  if (*ch=='-' || *ch=='+') sign -= 2*(*ch++=='-');
  bool ok = '0'<=*ch && *ch<='9';
  int acc = 0;
  while ('0'<=*ch && *ch<='9' && acc<(INT32_MAX-10)/10) {  // NA==INT_MIN==-2147483648==-INT_MAX(+2147483647)-1
    acc *= 10;
    acc += *ch-'0';
    ch++;
  }
  if (quoted) { if (*ch!=quote) return 1; else ch++; }
  *target = sign * acc;
  skip_white(&ch);
  ok = ok && on_sep(&ch);
  //DTPRINT("StrtoI32 field '%.*s' has len %d\n", lch-ch+1, ch, len);
  *pch = ch;
  if (ok && !any_number_like_NAstrings) return 0;
  bool na = is_NAstring(start);
  if (ok && !na) return 0;
  *target = NA_INT32;
  next_sep(&ch);
  *pch = ch;
  return !na;
}


// generate freadLookups.h
// TODO: review ERANGE checks and tests; that range outside [1.7e-308,1.7e+308] coerces to [0.0,Inf]
/*
f = "~/data.table/src/freadLookups.h"
cat("const long double pow10lookup[701] = {\n", file=f, append=FALSE)
for (i in (-350):(349)) cat("1.0E",i,"L,\n", sep="", file=f, append=TRUE)
cat("1.0E350L\n};\n", file=f, append=TRUE)
*/

static int StrtoD(const char **pch, double *target)
{
  // [+|-]N.M[E|e][+|-]E or Inf or NAN

  const char *ch = *pch;
  skip_white(&ch);
  if (on_sep(&ch)) {
    *target = NA_FLOAT64;
    *pch = ch;
    return 0;
  }
  bool quoted = false;
  if (*ch==quote) { quoted=true; ch++; }
  int sign=1;
  double d = NAND;
  const char *start=ch;
  if (*ch=='-' || *ch=='+') sign -= 2*(*ch++=='-');
  bool ok = ('0'<=*ch && *ch<='9') || *ch==dec;  // a single - or + with no [0-9] is !ok and considered type character
  if (ok) {
    uint64_t acc = 0;
    while ('0'<=*ch && *ch<='9' && acc<(UINT64_MAX-10)/10) { // compiler should optimize last constant expression
      // UNIT64_MAX == 18446744073709551615
      acc *= 10;
      acc += (uint64_t)(*ch - '0');
      ch++;
    }
    const char *decCh = (*ch==dec) ? ++ch : NULL;
    while ('0'<=*ch && *ch<='9' && acc<(UINT64_MAX-10)/10) {
      acc *= 10;
      acc += (uint64_t)(*ch - '0');
      ch++;
    }
    int e = decCh ? -(int)(ch-decCh) : 0;
    if (decCh) while ('0'<=*ch && *ch<='9') ch++; // lose precision
    else       while ('0'<=*ch && *ch<='9') { e--; ch++; }  // lose precision but retain scale
    if (*ch=='E' || *ch=='e') {
      ch++;
      int esign=1;
      if (*ch=='-' || *ch=='+') esign -= 2*(*ch++=='-');
      int eacc = 0;
      while ('0'<=*ch && *ch<='9' && eacc<(INT32_MAX-10)/10) {
        eacc *= 10;
        eacc += *ch-'0';
        ch++;
      }
      e += esign * eacc;
    }
    d = (unsigned)(e + 350) <= 700 ? (double)(sign * (long double)acc * pow10lookup[350+e])
        : e < -350 ? 0 : sign * INFD;
  }
  if (quoted) { if (*ch!=quote) return 1; else ch++; }
  *target = d;
  skip_white(&ch);
  ok = ok && on_sep(&ch);
  *pch = ch;
  if (ok && !any_number_like_NAstrings) return 0;
  bool na = is_NAstring(start);
  if (ok && !na) return 0;
  *target = NA_FLOAT64;
  next_sep(&ch);
  *pch = ch;
  return !na;
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
static int parse_double_extended(const char **pch, double *target)
{
  const char *ch = *pch;
  skip_white(&ch);
  if (on_sep(&ch)) {
    *target = NA_FLOAT64;
    *pch = ch;
    return 0;
  }
  bool neg, quoted;
  ch += (quoted = (*ch=='"'));
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
    while (static_cast<uint_fast8_t>(*ch-'0') < 10) ch++;
    goto return_nan;
  }
  if ((ch[0]=='q' || ch[0]=='s') && ch[1]=='N' && ch[2]=='a' && ch[3]=='N' && (ch += 4)) {
    while (static_cast<uint_fast8_t>(*ch-'0') < 10) ch++;
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
  return StrtoD(pch, target);

  return_inf:
    *target = neg? -INFD : INFD;
    goto ok;
  return_nan:
    *target = NAND;
    goto ok;
  return_na:
    *target = NA_FLOAT64;
    goto ok;
  ok:
    if (quoted && *ch!='"') {
      // *target = NA_FLOAT64;
      return 1;
    }
    ch += quoted;
    if (!on_sep(&ch)) return 1;
    *pch = ch;
    return 0;
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
static int parse_double_hexadecimal(const char **pch, double *target)
{
  const char *ch = *pch;
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
    *(reinterpret_cast<uint64_t*>(target)) = (neg << 63) | (E << 52) | (acc);
    *pch = ch;
    return 0;
  }
  if (ch[0]=='N' && ch[1]=='a' && ch[2]=='N') {
    *target = NA_FLOAT64;
    *pch = ch + 3;
    return 0;
  }
  if (ch[0]=='I' && ch[1]=='n' && ch[2]=='f' && ch[3]=='i' &&
      ch[4]=='n' && ch[5]=='i' && ch[6]=='t' && ch[7]=='y') {
    *target = neg ? -INFD : INFD;
    *pch = ch + 8;
    return 0;
  }

  fail:
    *target = NA_FLOAT64;
    return 1;
}


static int parse_float_hexadecimal(const char **pch, float *target)
{
  const char *ch = *pch;
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
    *(reinterpret_cast<uint32_t*>(target)) = (neg << 31) | (E << 23) | (acc);
    *pch = ch;
    return 0;
  }
  if (ch[0]=='N' && ch[1]=='a' && ch[2]=='N') {
    *target = NA_FLOAT32;
    *pch = ch + 3;
    return 0;
  }
  if (ch[0]=='I' && ch[1]=='n' && ch[2]=='f' && ch[3]=='i' &&
      ch[4]=='n' && ch[5]=='i' && ch[6]=='t' && ch[7]=='y') {
    *target = neg ? -INFINITY : INFINITY;
    *pch = ch + 8;
    return 0;
  }

  fail:
    *target = NA_FLOAT32;
    return 1;
}


static int StrtoB(const char **pch, int8_t *target)
{
  // These usually come from R when it writes out.
  const char *ch = *pch;
  skip_white(&ch);
  *target = NA_BOOL8;
  if (on_sep(&ch)) { *pch=ch; return 0; }  // empty field ',,'
  const char *start=ch;
  bool quoted = false;
  if (*ch==quote) { quoted=true; ch++; }
  if (quoted && *ch==quote) { ch++; if (on_sep(&ch)) {*pch=ch; return 0;} else return 1; }  // empty quoted field ',"",'
  bool logical01 = false;  // expose to user and should default be true?
  if ( ((*ch=='0' || *ch=='1') && logical01) || (*ch=='N' && *(ch+1)=='A' && ch++)) {
    *target = (*ch=='1' ? 1 : (*ch=='0' ? 0 : NA_BOOL8));
    ch++;
  } else if (*ch=='T' || *ch=='t') {
    *target = 1;
    if ((ch[1]=='R' && ch[2]=='U' && ch[3]=='E') ||
        (ch[1]=='r' && ch[2]=='u' && ch[3]=='e')) ch += 4;
  } else if (*ch=='F' || *ch=='f') {
    *target = 0;
    if ((ch[1] == 'A' && ch[2] == 'L' && ch[3] == 'S' && ch[4] == 'E') ||
        (ch[1] == 'a' && ch[2] == 'l' && ch[3] == 's' && ch[4] == 'e')) ch += 5;
  }
  if (quoted) { if (*ch!=quote) return 1; else ch++; }
  if (on_sep(&ch)) { *pch=ch; return 0; }
  *target = NA_BOOL8;
  next_sep(&ch);
  *pch=ch;
  return !is_NAstring(start);
}


// In order to add a new type:
//   - register new parser in this `fun` array
//   - add entries in arrays `typeName` / `typeSize` at the top of this file
//   - add entry in array `colType` in "fread.h" and increase NUMTYPE
//   - add record in array `colType_to_stype` in "py_fread.c"
//   - add items in `_coltypes_strs` and `_coltypes` in "fread.py"
//
typedef int (*reader_fun_t)(const char **ptr, void *target);
static reader_fun_t fun[NUMTYPE] = {
  (reader_fun_t) &Field,   // CT_DROP
  (reader_fun_t) &StrtoB,
  (reader_fun_t) &StrtoI32_bare,
  (reader_fun_t) &StrtoI32_full,
  (reader_fun_t) &StrtoI64,
  (reader_fun_t) &parse_float_hexadecimal,
  (reader_fun_t) &StrtoD,
  (reader_fun_t) &parse_double_extended,
  (reader_fun_t) &parse_double_hexadecimal,
  (reader_fun_t) &Field
};



//=================================================================================================
//
// Main fread() function that does all the job of reading a text/csv file.
//
// Returns 1 if it finishes successfully, and 0 otherwise.
//
//=================================================================================================
int freadMain(freadMainArgs _args)
{
  args = _args;  // assign to global for use by DTPRINT() in other functions
  double t0 = wallclock();

  //*********************************************************************************************
  // [1] Extract the arguments and check their validity
  //*********************************************************************************************
  bool verbose = args.verbose;
  bool warningsAreErrors = args.warningsAreErrors;
  if (verbose) DTPRINT("[1] Check arguments\n");

  if (mmp || colNames || oldType || lineCopy || type || size) {
    DTWARN("Internal error: Previous fread() session was not cleaned up properly");
    freadCleanup();
  }

  int nth = args.nth;
  {
    int maxth = omp_get_max_threads();
    if (nth > maxth) nth = maxth;
    if (nth <= 0) nth += maxth;
    if (nth <= 0) nth = 1;
    if (verbose) DTPRINT("  Using %d threads (omp_get_max_threads()=%d, nth=%d)\n", nth, maxth, args.nth);
  }

  uint64_t ui64 = NA_FLOAT64_I64;
  uint32_t ui32 = NA_FLOAT32_I32;
  memcpy(&NA_FLOAT64, &ui64, 8);
  memcpy(&NA_FLOAT32, &ui32, 4);

  size_t nrowLimit = (size_t) args.nrowLimit;
  NAstrings = args.NAstrings;
  any_number_like_NAstrings = false;
  blank_is_a_NAstring = false;
  // if we know there are no nastrings which are numbers (like -999999) then in the number
  // field processors we can save an expensive step in checking the NAstrings. If the field parses as a number,
  // we then when any_number_like_nastrings==FALSE we know it can't be NA.
  const char * const* nastr = NAstrings;
  while (*nastr) {
    if (**nastr == '\0') {
      blank_is_a_NAstring = true;
      nastr++;
      continue;
    }
    const char *ch = *nastr;
    size_t nchar = strlen(ch);
    if (isspace(ch[0]) || isspace(ch[nchar-1]))
      STOP("freadMain: NAstring \"%s\" has whitespace at the beginning or end", ch);
    if (strcmp(ch,"T")==0    || strcmp(ch,"F")==0 ||
        strcmp(ch,"TRUE")==0 || strcmp(ch,"FALSE")==0 ||
        strcmp(ch,"True")==0 || strcmp(ch,"False")==0 ||
        strcmp(ch,"1")==0    || strcmp(ch,"0")==0)
      STOP("freadMain: NAstring \"%s\" is recognized as type boolean, this is not permitted.", ch);
    char *end;
    errno = 0;
    strtod(ch, &end);  // careful not to let "" get to here (see continue above) as strtod considers "" numeric
    if (errno==0 && (size_t)(end - ch) == nchar) any_number_like_NAstrings = true;
    nastr++;
  }
  if (verbose) {
    if (*NAstrings == NULL) {
      DTPRINT("  No NAstrings provided.\n");
    } else {
      DTPRINT("  NAstrings = [");
      const char * const* s = NAstrings;
      while (*s++) DTPRINT(*s? "\"%s\", " : "\"%s\"", s[-1]);
      DTPRINT("]\n");
      if (any_number_like_NAstrings)
        DTPRINT("  One or more of the NAstrings looks like a number.\n");
      else
        DTPRINT("  None of the NAstrings look like numbers.\n");
    }
  }
  if (verbose) {
    if (args.skipNrow) DTPRINT("  skip lines = %lld\n", args.skipNrow);
    if (args.skipString) DTPRINT("  skip to string = \"%s\"\n", args.skipString);
    DTPRINT("  showProgress = %d\n", args.showProgress);
  }

  stripWhite = args.stripWhite;
  skipEmptyLines = args.skipEmptyLines;
  fill = args.fill;
  dec = args.dec;
  quote = args.quote;
  if (args.sep == quote && quote!='\0') STOP("sep == quote ('%c') is not allowed", quote);
  if (dec=='\0') STOP("dec='' not allowed. Should be '.' or ','");
  if (args.sep == dec) STOP("sep == dec ('%c') is not allowed", dec);
  if (quote == dec) STOP("quote == dec ('%c') is not allowed", dec);

  // File parsing context: pointer to the start of file, and to the end of
  // the file. The `sof` pointer may be shifted in order to skip over
  // "irrelevant" parts: the BOM mark, the banner, the headers, the skipped
  // lines, etc. Similarly, `eof` may be adjusted to take out the footer of
  // the file.
  // Additionally, we will sometimes need to switch to a different parsing
  // context in order to accommodate for the lack of newline on the last line
  // of file.
  const char *sof = NULL;
  const char *eof = NULL;
  // Convenience variable for iteration over the file.
  const char *ch = NULL, *end = NULL;


  //*********************************************************************************************
  // [2] Open and memory-map the input file, setting up variables `sof` and `eof`.
  //     We also arrange so that `*eof == '\0'`, which means that parsers do not need to check
  //     condition `ch < eof` all the time.
  //*********************************************************************************************
  if (verbose) DTPRINT("[2] Opening the file\n");

  if (args.input) {
    sof = args.input;
    fileSize = strlen(sof);
    if (verbose) {
      eol = '\n';  // just a guess, so that strlim below work correctly
      DTPRINT("  Input is passed as raw text, starting \"%s\"\n",
              strlim(sof, 20, sof+fileSize));
    }
  }
  else if (args.filename) {
    if (verbose) DTPRINT("  Opening file %s\n", args.filename);
    const char* fnam = args.filename;
    {
      File file(fnam, File::READ);
      fileSize = file.size();
      if (fileSize == 0) {
        STOP("File is empty: %s", fnam);
      }
      if (verbose) DTPRINT("  File opened, size = %s.\n", filesize_to_str(fileSize));

      long pageSize = sysconf(_SC_PAGE_SIZE);
      if (verbose) DTPRINT("  System memory page size: %ldB\n", pageSize);

      // Over-allocate by 1 byte and open in "Private, read-write" mode -- so
      // that we can write a single '\0' byte at the end.
      // | MAP_PRIVATE:
      // |   Create a private copy-on-write mapping.  Updates to the mapping
      // |   are not carried through to the underlying file.
      // | MAP_NORESERVE
      // |   Do not reserve swap space for this mapping.  When swap space is
      // |   reserved, one has the guarantee that it is possible to modify the
      // |   mapping.  When swap space is not reserved one might get SIGSEGV
      // |   upon a write if no physical memory is available.
      //
      mmp = mmap(NULL, fileSize + 1, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_NORESERVE,
                 file.descriptor(), 0);
      if (mmp == MAP_FAILED) {
        STOP("Cannot memory-map the file: %s", strerror(errno));
      }
      if (verbose) DTPRINT("  File memory-mapped at address %p\n", mmp);

      // | A file is mapped in multiples of the page size. For a file that is
      // | not a multiple of the page size, the remaining memory is 0ed when
      // | mapped, and writes to that region are not written out to the file.
      // Thus, when `fileSize` is *not* a multiple of pageSize, then the
      // memory mapping will have some writable "scratch" space at the end,
      // filled with '\0' bytes. In this case we don't need to do anything
      // special.
      // However when `fileSize` *is* a multiple of pageSize, then attempt to
      // read/write *eof wil fail with a BUS error:
      // | Use of a mapped region can result in these signals:
      // | SIGBUS:
      // |   Attempted access to a portion of the buffer that does not
      // |   correspond to the file (for example, beyond the end of the file)
      // In order to circumvent this, we allocate a new memory-mapped region,
      // placed at the address `mmp + fileSize`. In theory, this should always
      // succeed because we over-allocated `mmp` by 1 byte; and even though
      // that 1 byte is not readable/writable, at least there is a guarantee
      // that it is not occupied by anyone else. Now, `mmap()` documentation
      // explicitly allows to declare mapping that overlap each other:
      // | MAP_ANONYMOUS:
      // |   The mapping is not backed by any file; its contents are
      // |   initialized to zero. The fd argument is ignored.
      // | MAP_FIXED
      // |   Don't interpret addr as a hint: place the mapping at exactly
      // |   that address.  `addr` must be a multiple of the page size. If
      // |   the memory region specified by addr and len overlaps pages of
      // |   any existing mapping(s), then the overlapped part of the existing
      // |   mapping(s) will be discarded.
      //
      if (fileSize % (unsigned long)pageSize == 0) {
        if (verbose) DTPRINT("  File size is a multiple of page size, need to allocate extra 1 page of memory\n");
        void *target = (void*)((char*)mmp + fileSize);
        xmmp = mmap(target, 1, PROT_WRITE|PROT_READ, MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED, -1, 0);
        if (xmmp == MAP_FAILED) STOP("Cannot allocate 1 byte at address %p", target);
        if (verbose) DTPRINT("  Extra memory allocated at %p\n", xmmp);
        ((char*)xmmp)[0] = '\0';
      }
    }
    sof = (const char*) mmp;
  } else {
    STOP("Neither `input` nor `filename` are given, nothing to read.");
  }
  eof = sof + fileSize;
  ASSERT(*eof == '\0');
  double tMap = wallclock();


  //*********************************************************************************************
  // [3] Check whether the file contains BOM (Byte Order Mark), and if yes
  //     strip it, modifying `sof`. Also, presence of BOM allows us to
  //     reliably detect the file's encoding.
  //     See: https://en.wikipedia.org/wiki/Byte_order_mark
  //     See: issues #1087 and #1465
  //*********************************************************************************************
  if (verbose) DTPRINT("[3] Detect and skip BOM\n");
  if (fileSize >= 3 && memcmp(sof, "\xEF\xBB\xBF", 3) == 0) {
    sof += 3;
    // ienc = CE_UTF8;
    if (args.verbose) DTPRINT("  UTF-8 byte order mark EF BB BF found at the start of the file and skipped.\n");
  }
  else if (fileSize >= 4 && memcmp(sof, "\x84\x31\x95\x33", 4) == 0) {
    sof += 4;
    // ienc = CE_GB18030;
    if (args.verbose) DTPRINT("  GB-18030 byte order mark 84 31 95 33 found at the start of the file and skipped.\n");
    DTWARN("GB-18030 encoding detected, however fread() is unable to decode it. Some character fields may be garbled.\n");
  }
  else if (fileSize >= 2 && sof[0] + sof[1] == '\xFE' + '\xFF') {  // either 0xFE 0xFF or 0xFF 0xFE
    STOP("File is encoded in UTF-16, this encoding is not supported by fread(). Please recode the file to UTF-8.");
  }


  //*********************************************************************************************
  // [4] Auto detect end-of-line character(s)
  //
  //     This section initializes variables `eol`, `eol2` and `eolLen`. If
  //     `eolLen` is 1, then we set both `eol` and `eol2` to the same
  //     character, otherwise `eolLen` is 2 and we set `eol` and `eol2` to the
  //     first and the second line-separator characters respectively.
  //*********************************************************************************************
  if (verbose) DTPRINT("[4] Detect end-of-line character(s)\n");

  // Scan the file until the first newline character is found. By the end of
  // this loop `ch` will be pointing to such a character, or to eof if there
  // are no newlines in the file.
  ch = sof;
  while (ch<eof && *ch!='\n' && *ch!='\r') {
    char c = *ch++;
    // Skip quoted fields, because they may contain different types of
    // newlines than in the rest of the file.
    if (c == quote) {
      const char *ch0 = ch;
      int nn = 0;
      while (ch < eof && *ch != quote && nn < 10) {
        nn += (*ch == '\n') || (*ch == '\r');
        ch++;
      }
      if (*ch == quote) {
        // Skip over the closing quote
        ch++;
      } else {
        // If we skipped till the end of the file within a quoted field, or
        // if there were too many lines, then maybe it's not really a quoted
        // field at all -- jump back to the beginning of the field and re-scan
        // as if it wasn't quoted.
        ch = ch0;
      }
    }
  }

  // No newlines were found
  if (ch==eof) {
    if (verbose) {
      DTPRINT("  Input ends before any \\r or \\n observed. It will be "
              "treated as a single row and copied to temporary buffer.\n");
    }
    eol = eol2 = '\n';
    eolLen = 1;
    // Make copy of the input, so that it can have a proper eol character at the end.
    size_t sz = (size_t)(eof - sof + eolLen);
    lineCopy = (char*) malloc(sz);
    if (!lineCopy) STOP("Unable to allocate %zd bytes for temporary copy of the input", sz);
    memcpy(lineCopy, sof, sz - 1);
    lineCopy[sz - 1] = '\n';
    sof = lineCopy;
    eof = lineCopy + sz;
  }
  // Otherwise `ch` is pointing to the first newline character encountered
  else {
    eol = eol2 = *ch;
    eolLen = 1;
    if (eol=='\r') {
      if (ch+1<eof && *(ch+1)=='\n') {
        if (verbose) DTPRINT("  Detected eol as \\r\\n (CRLF) in that order, the Windows standard.\n");
        eol2='\n'; eolLen=2;
      } else {
        if (ch+1<eof && *(ch+1)=='\r')
          STOP("Line ending is \\r\\r\\n. R's download.file() appears to add the extra \\r in text mode on Windows. Please download again in binary mode (mode='wb') which might be faster too. Alternatively, pass the URL directly to fread and it will download the file in binary mode for you.");
          // NB: on Windows, download.file from file: seems to condense \r\r too. So
        if (verbose) DTPRINT("Detected eol as \\r only (no \\n or \\r afterwards). An old Mac 9 standard, discontinued in 2002 according to Wikipedia.\n");
      }
    } else {
      if (ch+1<eof && *(ch+1)=='\r') {
        DTWARN("Detected eol as \\n\\r, a highly unusual line ending. According to Wikipedia the Acorn BBC used this. If it is intended that the first column on the next row is a character column where the first character of the field value is \\r (why?) then the first column should start with a quote (i.e. 'protected'). Proceeding with attempt to read the file.\n");
        eol2='\r'; eolLen=2;
      } else if (verbose) DTPRINT("  Detected eol as \\n only (no \\r afterwards), the UNIX and Mac standard.\n");
    }
  }


  //*********************************************************************************************
  // [5] Temporarily hide the last line if it doesn't end with a newline.
  //
  //     Ensure file ends with eol so we don't need to check 'ch < eof &&
  //     *ch...' everywhere in deep loops just because the very last field in
  //     the file might finish abrubtly with no final \n.
  //
  //     If we do see that the file has no trailing newline, then we create
  //     the "hidden line" context by copying the last line of the file into a
  //     separate buffer (with newline characters appended at the end). We
  //     will need to be careful to switch to this context when appropriate.
  //
  //     After this section, it is guaranteed that `eof[-eolLen] == eol` and
  //     `eoh` is either NULL or `eoh[-eolLen] == eol`. On the other hand, all
  //     subsequent sections will have to take into account the fact that the
  //     input may have been split into 2 parts.
  //*********************************************************************************************
  if (verbose) DTPRINT("[5] Check for missing newline at the end of input\n");

  // "Hidden line" context: start-of-hidden section and end-of-hidden section.
  const char *soh = NULL;
  const char *eoh = NULL;

  bool trailing_newline_added = false;
  if (!(eof[-eolLen] == eol && eof[-1] == eol2)) {
    const char *oldeof = eof;
    while (eof[-eolLen] != eol || eof[-1] != eol2) eof--;
    size_t sz = (size_t)(oldeof - eof + eolLen);
    size_t sz0 = sz - (size_t)eolLen;
    lineCopy = (char*) malloc(sz);
    if (!lineCopy) STOP("Unable to allocate %zd bytes for a temporary buffer", sz);
    memcpy(lineCopy, eof, sz0);
    lineCopy[sz - 1] = eol2;
    lineCopy[sz0] = eol;
    soh = lineCopy;
    eoh = lineCopy + sz;
    if (verbose)
      DTPRINT("  Last character in the file is not a newline, so EOF is "
              "temporarily moved %zd bytes backwards\n", sz0);
    ASSERT(eoh[-eolLen] == eol && eoh[-1] == eol2);
    ASSERT(*eof == *soh && oldeof[-1] == eoh[-eolLen-1]);
    trailing_newline_added = true;
  }
  ASSERT(eof[-eolLen] == eol && eof[-1] == eol2);


  //*********************************************************************************************
  // [6] Position to line `skipNrow+1` or to line containing `skipString`.
  //
  //     This section moves the `sof` pointer passing over the lines that the
  //     user requested to skip.
  //
  //     We also compute the `line` variable, which tracks the current line
  //     number within the file (similar to __LINE__ macro). Note that `line`
  //     variable is different from the logical row number: it doesn't attempt
  //     to skip newlines within quoted fields. Thus this line is similar to
  //     what text editors report, or bash commands like "wc -l", "head -n"
  //     or "tail -n".
  //*********************************************************************************************
  if (verbose) DTPRINT("[6] Skipping initial rows if needed\n");

  int line = 1;

  if (args.skipString) {
    // TODO: unsafe! there might be no \0 at the end of the file
    ch = strstr(sof, args.skipString);
    if (!ch) {
      STOP("skip='%s' not found in input (it is case sensitive and literal; "
           "i.e., no patterns, wildcards or regexps)", args.skipString);
    }
    // Move to beginning of line. We ignore complications arising from
    // possibility to end up inside a quoted field. Presumably, if the user
    // supplied explicit option `skipString` he knows what he is doing.
    if (soh && ch >= eof) {
      // The `skipString` was found within the last "hidden" line of the file.
      // This means the entire first section of the file can be skipped, and
      // only the "hidden" section remains. Thus we "unhide" it replacing the
      // "main" section.
      advance_sof_to(soh, &sof, &eof, &soh, &eoh);
      if (verbose) {
        DTPRINT("  Found skip='%s' on the last line of the input. Skipping all "
                "lines but the last", args.skipString);
      }
    } else {
      // Scan backwards to find the beginning of the current line
      while (ch > sof && ch[-1] != eol2) ch--;
      const char *start = ch;
      ch = sof;
      while (ch < start) {
        line += (*ch++ == eol && (eolLen == 1 || *ch++ == eol2));
      }
      if (verbose) {
        DTPRINT("  Found skip='%s' on line %d. The file will be scanned from "
                "that line onwards.\n", args.skipString, line);
      }
      advance_sof_to(start, &sof, &eof, &soh, &eoh);
    }
  } else

  // Skip the first `skipNrow` lines of input.
  if (args.skipNrow) {
    ch = sof; end = eof;
    while ((ch < end || (soh && (end != eoh) && (ch = soh) && (end = eoh)))
           && line <= args.skipNrow)
    {
      line += (*ch++ == eol && (eolLen == 1 || *ch++ == eol2));
    }
    if (line > args.skipNrow) {
      advance_sof_to(ch, &sof, &eof, &soh, &eoh);
      if (verbose) DTPRINT("  Skipped %d line(s) of input.\n", line);
    } else {
      STOP("skip=%d but the input has only %d line(s)\n", args.skipNrow, line-1);
    }
  }

  // Additionally, skip any blank lines at the start
  const char *lineStart = sof;
  ch = sof; end = eof;
  while ((ch < end || (soh && (end != eoh) && (ch = soh) && (end = eoh)))
         && isspace(*ch))  // isspace() matches ' ', \t, \n and \r
  {
    if (*ch++ == eol && (eolLen == 1 || *ch++ == eol2)) {
      lineStart = ch;
      line++;
    }
  }
  if (ch >= end) {
    if (args.skipNrow || args.skipString)
      STOP("All input has been skipped: the remainder of the file has nothing but whitespace.\n");
    else
      STOP("Input is empty or contains only Whitespace.\n");
  }
  if (verbose) {
    if (lineStart != sof) {
      DTPRINT("  Moved forward to first non-blank line (%d)\n", line);
    }
    DTPRINT("  Positioned on line %d starting: \"%s\"\n",
            line, strlim(lineStart, 30, eof));
  }
  advance_sof_to(lineStart, &sof, &eof, &soh, &eoh);


  //*********************************************************************************************
  // [7] Auto detect separator, quoting rule, first line and ncol, simply,
  //     using jump 0 only.
  //
  //     Always sample as if nrows= wasn't supplied. That's probably *why*
  //     user is setting nrow=0 to get the column names and types, without
  //     actually reading the data yet. Most likely to check consistency
  //     across a set of files.
  //*********************************************************************************************
  if (verbose) DTPRINT("[7] Detect separator, quoting rule, and ncolumns\n");

  int nseps;
  char seps[]=",|;\t ";  // default seps in order of preference. See ?fread.
  // using seps[] not *seps for writeability (http://stackoverflow.com/a/164258/403310)

  if (args.sep == '\0') {  // default is '\0' meaning 'auto'
    if (verbose) DTPRINT("  Detecting sep ...\n");
    nseps = (int) strlen(seps);
  } else {
    seps[0] = args.sep;
    seps[1] = '\0';
    nseps = 1;
    if (verbose) DTPRINT("  Using supplied sep '%s'\n", args.sep=='\t' ? "\\t" : seps);
  }

  int topNumLines=0;        // the most number of lines with the same number of fields, so far
  int topNumFields=1;       // how many fields that was, to resolve ties
  char topSep=eol;          // which sep that was, by default \n to mean single-column input (1 field)
  int topQuoteRule=0;       // which quote rule that was
  int topNmax=1;            // for that sep and quote rule, what was the max number of columns (just for fill=true)
                            //   (when fill=true, the max is usually the header row and is the longest but there are more
                            //    lines of fewer)
  const char *firstJumpEnd=NULL; // remember where the winning jumpline from jump 0 ends, to know its size excluding header

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
      // if (verbose) DTPRINT("  Trying sep='%c' with quoteRule %d ...\n", sep, quoteRule);
      for (int i=0; i<=JUMPLINES; i++) { numFields[i]=0; numLines[i]=0; } // clear VLAs
      int i=-1; // The slot we're counting the currently contiguous consistent ncol
      int thisLine=0, lastncol=-1;
      ch = sof; end = eof;
      while ((ch < end || (soh && (end != eoh) && (ch = soh) && (end = eoh)))
             && thisLine++ < JUMPLINES)
      {
        // Compute num columns and move `ch` to the start of next line
        int thisncol = countfields(&ch, &end, soh, eoh);
        if (thisncol < 0) {
          // invalid file with this sep and quote rule; abort
          numFields[0] = -1;
          break;
        }
        if (thisncol!=lastncol) { numFields[++i]=thisncol; lastncol=thisncol; } // new contiguous consistent ncol started
        numLines[i]++;
      }
      if (numFields[0]==-1) continue;
      if (firstJumpEnd==NULL) firstJumpEnd=ch;  // if this wins (doesn't get updated), it'll be single column input
      bool updated=false;
      int nmax=0;

      i = -1;
      while (numLines[++i]) {
        if (numFields[i] > nmax) nmax=numFields[i];  // for fill=true to know max number of columns
        //if (args.verbose) DTPRINT("numLines[i]=%d, topNumLines=%d, numFields[i]=%d, topNumFields=%d\n",
        //                           numLines[i], topNumLines, numFields[i], topNumFields);
        if (numFields[i]>1 &&
            ( numLines[i]>topNumLines ||   // most number of consistent ncol wins
             (numLines[i]==topNumLines && numFields[i]>topNumFields && sep!=' '))) {  // ties resolved by numFields
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
        DTPRINT(sep<' '? "  sep=%#02x" : "  sep='%c'", sep);
        DTPRINT("  with %d lines of %d fields using quote rule %d\n", topNumLines, topNumFields, topQuoteRule);
      }
    }
  }
  if (!firstJumpEnd) STOP("Internal error: no sep won");
  // the size in bytes of the first JUMPLINES from the start (jump point 0)
  size_t jump0size = (sof <= firstJumpEnd && firstJumpEnd <= eof)
                      ? (size_t)(firstJumpEnd - sof)
                      : (size_t)(eof - sof) + (size_t)(firstJumpEnd - soh);
  ASSERT(jump0size >= 0 && jump0size <= fileSize + (size_t)eolLen);
  quoteRule = topQuoteRule;
  sep = topSep;
  whiteChar = (sep==' ' ? '\t' : (sep=='\t' ? ' ' : 0));

  // Find the first line with the consistent number of fields.  There might
  // be irregular header lines above it.
  int ncol;
  // Save the `sof` pointer before moving it. We might need to come back to it
  // later when reporting an error in next section.
  const char *headerPtr = sof;
  if (fill) {
    // start input from first populated line; do not alter sof.
    ncol = topNmax;
  } else {
    ncol = topNumFields;
    int thisLine = -1;
    ch = sof; end = eof;
    while ((ch < eof || (soh && (end != eoh) && (end=eoh) && (ch=soh)))
           && ++thisLine < JUMPLINES)
    {
      const char *ch2 = ch;   // lineStart
      int cols = countfields(&ch, &end, soh, eoh);  // advances ch to next line
      if (cols == ncol) {
        advance_sof_to(ch2, &sof, &eof, &soh, &eoh);
        line += thisLine;
        break;
      }
    }
  }
  // For standard regular separated files, we're now on the first byte of the file.

  ASSERT(ncol >= 1 && line >= 1);
  ch = sof;
  end = eof;
  int tt = countfields(&ch, &end, soh, eoh);
  if (verbose) {
    DTPRINT("  Detected %d columns on line %d. This line is either column "
            "names or first data row. Line starts as: \"%s\"\n",
            tt, line, strlim(sof, 30, eof));
    DTPRINT("  Quote rule picked = %d\n", quoteRule);
    if (fill) DTPRINT("  fill=true and the most number of columns found is %d\n", ncol);
  }
  ASSERT(fill || tt == ncol);


  //*********************************************************************************************
  // [8] Detect and assign column names (if present)
  //
  //     This section also moves the `sof` pointer to point at the first row
  //     of data ("removing" the column names).
  //*********************************************************************************************
  if (verbose) DTPRINT("[8] Determine column names\n");
  // throw-away storage for processors to write to in this preamble.
  // Saves deep 'if (target)' inside processors.
  double trash_val; // double so that this storage is aligned. char trash[8] would not be aligned.
  void *trash = (void*)&trash_val;

  const char *colNamesAnchor = sof;
  colNames = (lenOff*) calloc((size_t)ncol, sizeof(lenOff));
  if (!colNames) STOP("Unable to allocate %d*%d bytes for column name pointers: %s", ncol, sizeof(lenOff), strerror(errno));
  bool allchar=true;
  ch = sof; // move back to start of line since countfields() moved to next
  end = eof;
  if (sep==' ') while (*ch==' ') ch++;
  ch--;  // so we can ++ at the beginning inside loop.
  for (int field=0; field<tt; field++) {
    const char *ch0 = ++ch;
    // DTPRINT("Field %d <<%s>>\n", field, strlim(ch, 20, eof));
    skip_white(&ch);
    if (allchar && !on_sep(&ch) && !StrtoD(&ch, (double *)trash)) allchar=false;  // don't stop early as we want to check all columns to eol here
    // considered looking for one isalpha present but we want 1E9 to be considered a value not a column name
    // StrtoD does not consume quoted fields according to the quote rule, so need to reparse using Field()
    ch = ch0;  // rewind to the start of this field
    int res = Field(&ch, (lenOff *)trash);
    ASSERT(res != 1);
    while (res == 2) {
      if (ch == end) {
        if (eoh && end != eoh) {
          ch = soh;
          end = eoh;
        } else ASSERT(0);
      }
      res = parse_string_continue(&ch, (lenOff *)trash);
    }

  }
  if (*ch!=eol)
    STOP("Read %d expected fields in the header row (fill=%d) but finished on \"%s\"", tt, fill, strlim(ch,30,eof));
  // already checked above that tt==ncol unless fill=TRUE
  // when fill=TRUE and column names shorter (test 1635.2), leave calloc initialized lenOff.len==0
  if (verbose && args.header!=NA_BOOL8) DTPRINT("  'header' changed by user from 'auto' to %s\n", args.header?"true":"false");
  if (args.header==false || (args.header==NA_BOOL8 && !allchar)) {
    if (verbose && args.header==NA_BOOL8) DTPRINT("  Some fields on line %d are not type character. Treating as a data row and using default column names.\n", line);
    // colNames was calloc'd so nothing to do; all len=off=0 already
    ch = sof;  // back to start of first row. Treat as first data row, no column names present.
    end = eof;
    // now check previous line which is being discarded and give helpful msg to user ...
    if (ch>headerPtr && args.skipNrow==0) {
      ch -= (eolLen+1);
      if (ch<headerPtr) ch=headerPtr;  // for when headerPtr[0]=='\n'
      while (ch>headerPtr && *ch!=eol2) ch--;
      if (ch>headerPtr) ch++;
      const char *prevStart = ch;
      int tmp = countfields(&ch, &end, soh, eoh);
      if (tmp==ncol) STOP("Internal error: row before first data row has the same number of fields but we're not using it.");
      if (tmp>1) DTWARN("Starting data input on line %d \"%s\" with %d fields and discarding "
                        "line %d \"%s\" before it because it has a different number of fields (%d).",
                        line, strlim(sof, 30, eof), ncol, line-1, strlim(prevStart, 30, eof), tmp);
    }
    if (ch!=sof) STOP("Internal error. ch!=sof after prevBlank check");
  } else {
    if (verbose && args.header==NA_BOOL8) {
      DTPRINT("  All the fields on line %d are character fields. Treating as the column names.\n", line);
    }
    ch = sof; end = eof;
    line++;
    if (sep==' ') while (*ch==' ') ch++;
    ch--;
    for (int i=0; i<ncol; i++) {
      const char *start = ++ch;
      int ret = Field(&ch, colNames + i);
      ASSERT(ret != 1);
      while (ret == 2) {
        line++;
        if (ch == eof) {
          ASSERT(eoh);
          ch = soh; end = eoh;
        }
        ret = parse_string_continue(&ch, colNames + i);
      }
      colNames[i].off += (size_t)(start-colNamesAnchor);
      if (*ch==eol) break;   // already checked number of fields previously above
    }
    if (*ch != eol) STOP("Internal error: reading colnames did not end on eol");
    advance_sof_to(ch + eolLen, &sof, &eof, &soh, &eoh);
  }
  int row1Line = line;
  double tLayout = wallclock();


  //*********************************************************************************************
  // [9] Make best guess at column types using 100 rows at 100 points,
  //     including the very first, middle and very last row.
  //     At the same time, calc mean and sd of row lengths in sample for very
  //     good nrow estimate.
  //*********************************************************************************************
  if (verbose) DTPRINT("[9] Detect column types\n");
  type = (int8_t *)malloc((size_t)ncol * sizeof(int8_t));
  size = (int8_t *)malloc((size_t)ncol * sizeof(int8_t));
  if (!type || !size) STOP("Failed to allocate %d x 2 bytes for type/size: %s", ncol, strerror(errno));

  for (int j = 0; j < ncol; j++) {
    // initialize with the first (lowest) type, 1==CT_BOOL8 at the time of writing. If we add CT_BOOL1 or CT_BOOL2 in
    /// future, using 1 here means this line won't need to be changed. CT_DROP is 0 and 1 is the first type.
    type[j] = 1;
    size[j] = typeSize[type[j]];
  }

  // how many places in the file to jump to and test types there (the very end is added as 11th or 101th)
  // not too many though so as not to slow down wide files; e.g. 10,000 columns.  But for such large files (50GB) it is
  // worth spending a few extra seconds sampling 10,000 rows to decrease a chance of costly reread even further.
  int nJumps = 0;
  size_t sz = (size_t)(eof - sof + (eoh ? eoh - soh : 0));
  if (jump0size>0) {
    if (jump0size*100*2 < sz) nJumps=100;  // 100 jumps * 100 lines = 10,000 line sample
    else if (jump0size*10*2 < sz) nJumps=10;
    // *2 to get a good spacing. We don't want overlaps resulting in double counting.
    // nJumps==1 means the whole (small) file will be sampled with one thread
  }
  nJumps++; // the extra sample at the very end (up to eof) is sampled and format checked but not jumped to when reading
  if (verbose) {
    DTPRINT("  Number of sampling jump points = %d because ", nJumps);
    if (jump0size==0) DTPRINT("jump0size==0\n");
    else DTPRINT("(%zd bytes from row 1 to eof) / (2 * %zd jump0size) == %zd\n",
                 sz, jump0size, sz/(2*jump0size));
  }

  size_t sampleLines=0;
  double sumLen=0.0, sumLenSq=0.0;
  int minLen=INT32_MAX, maxLen=-1;   // int_max so the first if(thisLen<minLen) is always true; similarly for max
  const char *lastRowEnd = sof;
  for (int j=0; j<nJumps; j++) {
    ch = (j == 0) ? sof :
         (j == nJumps-1) ? eof - (size_t)(0.5*jump0size) :
                           sof + (size_t)j*(sz/(size_t)(nJumps-1));
    end = eof;
    if (j>0 && !nextGoodLine(&ch, ncol, end))
      STOP("Could not find first good line start after jump point %d when sampling.", j);
    bool bumped = 0;  // did this jump find any different types; to reduce verbose output to relevant lines
    int jline = 0;  // line from this jump point
    while((ch < end || (soh && (end != eoh) && (end=eoh) && (ch=soh))) &&
          (jline<JUMPLINES || j==nJumps-1))
    {  // nJumps==1 implies sample all of input to eof; last jump to eof too
      const char *jlineStart = ch;
      if (sep==' ') while (*ch==' ') ch++;  // multiple sep=' ' at the jlineStart does not mean sep(!)
      // detect blank lines
      skip_white(&ch);
      if (*ch==eol) {
        if (!skipEmptyLines && !fill) break;
        jlineStart = ch;  // to avoid 'Line finished early' below and get to the sampleLines++ block at the end of this while
      }
      jline++;
      int field=0;
      const char *fieldStart = ch;  // Needed outside loop for error messages below
      while (*ch!=eol && field<ncol) {
        fieldStart=ch;
        int res;
        while (type[field]<=CT_STRING && (res = fun[type[field]](&ch, trash))) {
          int neols = 0;
          while (res == 2 && neols++ < 100) {
            if (ch == end) {
              if (eoh && end != eoh) { ch = soh; end = eoh; }
              else { res = 1; break; }
            }
            res = parse_string_continue(&ch, (lenOff*)trash);
          }
          if (res == 0) break;
          ch = fieldStart;
          if (type[field] < CT_STRING) {
            type[field]++;
            bumped = true;
          } else {
            // the field could not be read with this quote rule, try again with next one
            // Trying the next rule will only be successful if the number of fields is consistent with it
            ASSERT(quoteRule < 3);
            if (verbose) {
              DTPRINT("Bumping quote rule from %d to %d due to field %d on line %d of sampling jump %d starting \"%s\"\n",
                       quoteRule, quoteRule+1, field+1, jline, j, strlim(fieldStart, 200, end));
            }
            quoteRule++;
            bumped=true;
            ch = jlineStart;  // Try whole line again, in case it's a hangover from previous field
            field=0;
            continue;
          }
        }
        // DTPRINT("%d  (ch = %p)\n", type[field], ch);
        if (*ch == eol && ch[eolLen-1] == eol2) {
          break;
        } else {
          // skip over the field separator
          ASSERT(*ch==sep);
          ch++;
          field++;
        }
      }
      if (field<ncol-1 && !fill) {
        if (ch<end && *ch!=eol) {
          STOP("Internal error: line has finished early but not on an eol or eof (fill=false). Please report as bug.");
        } else if (ch>jlineStart) {
          STOP("Line %d has too few fields when detecting types. Use fill=TRUE to pad with NA. "
               "Expecting %d fields but found %d: \"%s\"", jline, ncol, field+1, strlim(jlineStart,200,end));
        }
      }
      ASSERT(ch < end);
      if (*ch!=eol || field>=ncol) {   // the || >=ncol is for when a comma ends the line with eol straight after
        if (field!=ncol) STOP("Internal error: Line has too many fields but field(%d)!=ncol(%d)", field, ncol);
        STOP("Line %d from sampling jump %d starting \"%s\" has more than the expected %d fields. "
             "Separator %d occurs at position %d which is character %d of the last field: \"%s\". "
             "Consider setting 'comment.char=' if there is a trailing comment to be ignored.",
            jline, j, strlim(jlineStart,10,end), ncol, ncol, (int)(ch-jlineStart), (int)(ch-fieldStart),
            strlim(fieldStart,200,end));
      }
      // if very last field was quoted, check if it was completed with an ending quote ok.
      // not necessarily a problem (especially if we detected no quoting), but we test it and nice to have
      // a warning regardless of quoting rule just in case file has been inadvertently truncated.
      // The warning is only issued if the file didn't have the newline on the last line.
      // This warning is early at type skipping around stage before reading starts, so user can cancel early
      if (type[ncol-1]==CT_STRING && *fieldStart==quote && *(ch-1)!=quote && trailing_newline_added) {
        if (quoteRule<2) STOP("Internal error: Last field of last line should select quote rule 2");
        DTWARN("Last field of last line starts with a quote but is not finished with a quote before end of file: \"%s\"",
                strlim(fieldStart, 200, end));
      }
      ch += eolLen;
      // Two reasons:  1) to get the end of the very last good row before whitespace or footer before eof
      //               2) to check sample jumps don't overlap, otherwise double count and bad estimate
      lastRowEnd = ch;
      //DTPRINT("\n");
      int thisLineLen = (int)(ch-jlineStart);  // ch is now on start of next line so this includes eolLen already
      sampleLines++;
      sumLen += thisLineLen;
      sumLenSq += thisLineLen*thisLineLen;
      if (thisLineLen<minLen) minLen=thisLineLen;
      if (thisLineLen>maxLen) maxLen=thisLineLen;
    }
    if (verbose && (bumped || j==0 || j==nJumps-1)) {
      DTPRINT("  Type codes (jump %03d)    : ",j); printTypes(ncol);
      DTPRINT("  Quote rule %d\n", quoteRule);
    }
  }
  while ((ch < end || (soh && (end != eoh) && (end=eoh) && (ch=soh))) && isspace(*ch)) ch++;
  if (ch < end) {
    DTWARN("Found the last consistent line but text exists afterwards (discarded): \"%s\"", strlim(ch,200,end));
  }
  retreat_eof_to(lastRowEnd, &sof, &eof, &soh, &eoh);

  size_t estnrow=1, allocnrow=1;
  size_t bytesRead = (size_t)(eof - sof) + (size_t)(eoh? eoh - soh : 0);
  size_t bytesToRead = bytesRead;
  double meanLineLen=0;
  if (sampleLines == 0) {
    if (verbose) DTPRINT("  sampleLines=0: only column names are present\n");
  } else {
    meanLineLen = (double)sumLen/sampleLines;
    estnrow = CEIL(bytesRead/meanLineLen);  // only used for progress meter and verbose line below
    double sd = sqrt( (sumLenSq - (sumLen*sumLen)/sampleLines)/(sampleLines-1) );
    allocnrow = clamp_szt((size_t)(bytesRead / fmax(meanLineLen - 2*sd, minLen)),
                          (size_t)(1.1*estnrow), 2*estnrow);
    // sd can be very close to 0.0 sometimes, so apply a +10% minimum
    // blank lines have length 1 so for fill=true apply a +100% maximum. It'll be grown if needed.
    if (verbose) {
      DTPRINT("  =====\n");
      DTPRINT("  Sampled %zd rows (handled \\n inside quoted fields) at %d jump point(s)\n", sampleLines, nJumps);
      DTPRINT("  Bytes from first data row on line %d to the end of last row: %zd\n", row1Line, bytesRead);
      DTPRINT("  Line length: mean=%.2f sd=%.2f min=%d max=%d\n", meanLineLen, sd, minLen, maxLen);
      DTPRINT("  Estimated number of rows: %zd / %.2f = %zd\n", bytesRead, meanLineLen, estnrow);
      DTPRINT("  Initial alloc = %zd rows (%zd + %d%%) using bytes/max(mean-2*sd,min) clamped between [1.1*estn, 2.0*estn]\n",
               allocnrow, estnrow, (int)(100.0*allocnrow/estnrow-100.0));
    }
    if (nJumps==1) {
      estnrow = allocnrow = sampleLines + (soh != NULL);
      if (verbose) DTPRINT("  All rows were sampled since file is small so we know nrow=%zd exactly\n", estnrow);
    } else {
      if (sampleLines > allocnrow) STOP("Internal error: sampleLines(%zd) > allocnrow(%zd)", sampleLines, allocnrow);
    }
    if (nrowLimit < allocnrow) {
      if (verbose) DTPRINT("  Alloc limited to lower nrows=%zd passed in.\n", nrowLimit);
      bytesToRead = (size_t) (bytesRead * (1.0 * nrowLimit / allocnrow));
      estnrow = allocnrow = nrowLimit;
    }
    if (verbose) DTPRINT("  =====\n");
  }


  //*********************************************************************************************
  // [10] Apply colClasses, select, drop and integer64
  //*********************************************************************************************
  if (verbose) DTPRINT("[10] Apply user overrides on column types\n");
  ch = sof;
  oldType = (int8_t *)malloc((size_t)ncol * sizeof(int8_t));
  if (!oldType) STOP("Unable to allocate %d bytes to check user overrides of column types", ncol);
  memcpy(oldType, type, (size_t)ncol) ;
  if (!userOverride(type, colNames, colNamesAnchor, ncol)) { // colNames must not be changed but type[] can be
    if (verbose) DTPRINT("  Cancelled by user: userOverride() returned false.");
    freadCleanup();
    return 1;
  }
  int ndrop=0, nUserBumped=0;
  size_t rowSize1 = 0;
  size_t rowSize4 = 0;
  size_t rowSize8 = 0;
  int nStringCols = 0;
  int nNonStringCols = 0;
  for (int j=0; j<ncol; j++) {
    size[j] = typeSize[type[j]];
    rowSize1 += (size[j] & 1);  // only works if all sizes are powers of 2
    rowSize4 += (size[j] & 4);
    rowSize8 += (size[j] & 8);
    if (type[j]==CT_DROP) { ndrop++; continue; }
    if (type[j]<oldType[j]) {
      STOP("Attempt to override column %d \"%.*s\" of inherent type '%s' down to '%s' which will lose accuracy. " \
           "If this was intended, please coerce to the lower type afterwards. Only overrides to a higher type are permitted.",
           j+1, colNames[j].len, colNamesAnchor+colNames[j].off, typeName[oldType[j]], typeName[type[j]]);
    }
    nUserBumped += type[j]>oldType[j];
    if (type[j] == CT_STRING) nStringCols++; else nNonStringCols++;
  }
  if (verbose) {
    DTPRINT("  After %d type and %d drop user overrides : ", nUserBumped, ndrop);
    printTypes(ncol); DTPRINT("\n");
  }
  double tColType = wallclock();


  //*********************************************************************************************
  // [11] Allocate the result columns
  //*********************************************************************************************
  if (verbose) DTPRINT("[11] Allocate memory for the datatable\n");
  if (verbose) {
    DTPRINT("  Allocating %d column slots (%d - %d dropped) with %zd rows\n",
            ncol-ndrop, ncol, ndrop, allocnrow);
  }
  size_t DTbytes = allocateDT(type, size, ncol, ndrop, allocnrow);
  if (DTbytes == 0) {
    // Failed to allocate: return
    freadCleanup();
    return 0;
  }
  double tAlloc = wallclock();

  // Read ahead and drop behind each point as they move through (assuming it's on a per thread basis).
  // Considered it but when processing string columns the buffers point to offsets in the mmp'd pages
  // which are revisited when writing the finished buffer to DT. So, it isn't sequential.
  // if (fnam!=NULL) {
  //   #ifdef MADV_SEQUENTIAL  // not on Windows. PrefetchVirtualMemory from Windows 8+ ?
  //   int ret = madvise((void *)mmp, (size_t)fileSize, MADV_SEQUENTIAL);
  //   #endif
  // }


  //*********************************************************************************************
  // [12] Read the data
  //*********************************************************************************************
  if (verbose) DTPRINT("[12] Read the data\n");
  ch = sof;   // back to start of first data row
  int hasPrinted=0;  // the percentage last printed so it prints every 2% without many calls to wallclock()
  bool stopTeam=false, firstTime=true;  // bool for MT-safey (cannot ever read half written bool value)
  int nTypeBump=0, nTypeBumpCols=0;
  double tRead=0, tReread=0, tTot=0;  // overall timings outside the parallel region
  double thNextGoodLine=0, thRead=0, thPush=0;  // reductions of timings within the parallel region
  char *typeBumpMsg=NULL;  size_t typeBumpMsgSize=0;
  #define stopErrSize 1000
  char stopErr[stopErrSize+1]="";  // must be compile time size: the message is generated and we can't free before STOP
  size_t DTi = 0;   // the current row number in DT that we are writing to
  const char *prevJumpEnd;  // the position after the last line the last thread processed (for checking)
  int buffGrown=0;
  size_t chunkBytes = umax((size_t)(1000*meanLineLen), 1ULL/*MB*/ *1024*1024);
  // chunkBytes is the distance between each jump point; it decides the number of jumps
  // We may want each chunk to write to its own page of the final column, hence 1000*maxLen
  // For the 44GB file with 12875 columns, the max line len is 108,497. We may want each chunk to write to its
  // own page (4k) of the final column, hence 1000 rows of the smallest type (4 byte int) is just
  // under 4096 to leave space for R's header + malloc's header.
  if (nJumps/*from sampling*/>1) {
    // ensure data size is split into same sized chunks (no remainder in last chunk) and a multiple of nth
    // when nth==1 we still split by chunk for consistency (testing) and code sanity
    nJumps = (int)(bytesToRead/chunkBytes);  // (int) rounds down
    if (nJumps==0) nJumps=1;
    else if (nJumps>nth) nJumps = nth*(1+(nJumps-1)/nth);
    chunkBytes = bytesToRead / (size_t)nJumps;
  } else {
    nJumps = 1;
  }
  if (verbose) {
    DTPRINT("  njumps=%d and chunkBytes=%zd\n", nJumps, chunkBytes);
  }
  size_t initialBuffRows = allocnrow / (size_t)nJumps;
  if (initialBuffRows < 10) initialBuffRows = 10;
  if (initialBuffRows > INT32_MAX) STOP("Buffer size %lld is too large\n", initialBuffRows);
  nth = imin(nJumps, nth);

  //-- Start parallel ----------------
  read:  // we'll return here to reread any columns with out-of-sample type exceptions
  prevJumpEnd = sof;
  #pragma omp parallel num_threads(nth)
  {
    int me = omp_get_thread_num();
    #pragma omp master
    nth = omp_get_num_threads();
    const char *thisJumpStart=NULL;  // The first good start-of-line after the jump point
    size_t myDTi = 0;  // which row in the final DT result I should start writing my chunk to
    size_t myNrow = 0; // the number of rows in my chunk

    // Allocate thread-private row-major myBuffs
    // Do not reuse &trash for myBuff0 as that might create write conflicts
    // between threads, causing slowdown of the process.
    size_t myBuffRows = initialBuffRows;  // Upon realloc, myBuffRows will increase to grown capacity
    void *myBuff8 = malloc(rowSize8 * myBuffRows);
    void *myBuff4 = malloc(rowSize4 * myBuffRows);
    void *myBuff1 = malloc(rowSize1 * myBuffRows);
    void *myBuff0 = malloc(8);  // for CT_DROP columns
    if ((rowSize8 && !myBuff8) ||
        (rowSize4 && !myBuff4) ||
        (rowSize1 && !myBuff1) || !myBuff0) stopTeam = true;

    ThreadLocalFreadParsingContext ctx = {
      .anchor = NULL,
      .buff8 = myBuff8,
      .buff4 = myBuff4,
      .buff1 = myBuff1,
      .rowSize8 = rowSize8,
      .rowSize4 = rowSize4,
      .rowSize1 = rowSize1,
      .DTi = 0,
      .nRows = allocnrow,
      .threadn = me,
      .quoteRule = quoteRule,
      .stopTeam = &stopTeam,
      #ifndef DTPY
      .nStringCols = nStringCols,
      .nNonStringCols = nNonStringCols
      #endif
    };
    prepareThreadContext(&ctx);

    #pragma omp for ordered schedule(dynamic) reduction(+:thNextGoodLine,thRead,thPush)
    for (int jump=0; jump<nJumps; jump++) {
      double tt0 = 0, tt1 = 0;
      if (verbose) { tt1 = tt0 = wallclock(); }

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
        if (verbose) { tt1 = wallclock(); thPush += tt1 - tt0; tt0 = tt1; }

        if (me==0 && (hasPrinted || (args.showProgress && jump/nth==4  &&
                                    ((double)nJumps/(nth*3)-1.0)*(wallclock()-tAlloc)>1.0 ))) {
          // Important for thread safety inside progess() that this is called not just from critical but that
          // it's the master thread too, hence me==0.
          // Jump 0 might not be assigned to thread 0; jump/nth==4 to wait for 4 waves to complete then decide once.
          int p = (int)(100.0*jump/nJumps);
          if (p>=hasPrinted) {
            // ETA TODO. Ok to call wallclock() now.
            progress(p, /*eta*/0);
            hasPrinted = p+1;  // update every 1%
          }
        }
        myNrow = 0;
      }
      if (jump>=nJumps || stopTeam) continue;  // nothing left to do. This jump was the dummy extra one.

      const char *tch = sof + (size_t)jump * chunkBytes;
      const char *nextJump = jump<nJumps-1 ? tch+chunkBytes+eolLen : eof;
      // +eolLen is for when nextJump happens to fall exactly on a line start (or on eol2 on Windows). The
      // next thread will start one line later because nextGoodLine() starts by finding next eol
      // Easier to imagine eolLen==1 and tch<=nextJump in the while() below
      if (jump>0 && !nextGoodLine(&tch, ncol, nextJump)) {
        stopTeam=true;
        DTPRINT("No good line could be found from jump point %d\n",jump); // TODO: change to stopErr
        continue;
      }
      thisJumpStart=tch;
      if (verbose) { tt1 = wallclock(); thNextGoodLine += tt1 - tt0; tt0 = tt1; }

      void *myBuff1Pos = myBuff1, *myBuff4Pos = myBuff4, *myBuff8Pos = myBuff8;
      void **allBuffPos[9];
      allBuffPos[0] = &myBuff0;
      allBuffPos[1] = &myBuff1Pos;
      allBuffPos[4] = &myBuff4Pos;
      allBuffPos[8] = &myBuff8Pos;

      const char *fake_anchor = thisJumpStart;
      while (tch<nextJump && myNrow < nrowLimit - myDTi) {
        if (myNrow == myBuffRows) {
          // buffer full due to unusually short lines in this chunk vs the sample; e.g. #2070
          myBuffRows *= 1.5;
          #pragma omp atomic
          buffGrown++;
          long diff8 = (char*)myBuff8Pos - (char*)myBuff8;
          long diff4 = (char*)myBuff4Pos - (char*)myBuff4;
          long diff1 = (char*)myBuff1Pos - (char*)myBuff1;
          ctx.buff8 = myBuff8 = realloc(myBuff8, rowSize8 * myBuffRows);
          ctx.buff4 = myBuff4 = realloc(myBuff4, rowSize4 * myBuffRows);
          ctx.buff1 = myBuff1 = realloc(myBuff1, rowSize1 * myBuffRows);
          if ((rowSize8 && !myBuff8) ||
              (rowSize4 && !myBuff4) ||
              (rowSize1 && !myBuff1)) {
            stopTeam = true;
            break;
          }
          // restore myBuffXPos in case myBuffX was moved by realloc
          myBuff8Pos = (void*)((char*)myBuff8 + diff8);
          myBuff4Pos = (void*)((char*)myBuff4 + diff4);
          myBuff1Pos = (void*)((char*)myBuff1 + diff1);
        }
        const char *tlineStart = tch;  // for error message
        if (sep==' ') while (*tch==' ') tch++;  // multiple sep=' ' at the tlineStart does not mean sep(!)
        skip_white(&tch);  // solely for blank lines otherwise could leave to field processors which handle leading white
        if (*tch==eol) {
          if (skipEmptyLines) { tch+=eolLen; continue; }
          else if (!fill) {
            #pragma omp critical
            if (!stopTeam) {
              stopTeam = true;
              snprintf(stopErr, stopErrSize,
                "Row %zd is empty. It is outside the sample rows. "
                "Set fill=true to treat it as an NA row, or blank.lines.skip=true to skip it",
                myDTi + myNrow);
                // TODO - include a few (numbered) lines before and after in the message.
            }
            break;
          }
        }

        int j = 0;
        bool at_line_end = false; // set to true if the loop ends at a line end
        while (j < ncol) {
          // DTPRINT("Field %d: '%.10s' as type %d  (tch=%p)\n", j+1, tch, type[j], tch);
          const char *fieldStart = tch;
          int8_t joldType = type[j];   // fetch shared type once. Cannot read half-written byte.
          int8_t thisType = joldType;  // to know if it was bumped in (rare) out-of-sample type exceptions
          int8_t absType = (int8_t) abs(thisType);

          // always write to buffPos even when CT_DROP. It'll just overwrite on next non-CT_DROP
          while (absType < NUMTYPE) {
            // normally returns success=1, and myBuffPos is assigned inside *fun.
            void *target = thisType > 0? *(allBuffPos[size[j]]) : myBuff0;
            int ret = fun[absType](&tch, target);
            if (ret == 0) break;
            while (ret == 2) {
              if (tch == eof) {
                if (eoh) { tch = soh; nextJump = eoh; }
                else break;
              }
              ret = parse_string_continue(&tch, (lenOff*)target);
            }
            if (ret == 0) break;
            // guess is insufficient out-of-sample, type is changed to negative sign and then bumped. Continue to
            // check that the new type is sufficient for the rest of the column to be sure a single re-read will work.
            absType++;
            thisType = -absType;
            tch = fieldStart;
          }

          if (joldType == CT_STRING) {
            ((lenOff*) myBuff8Pos)->off += (int32_t)(fieldStart - fake_anchor);
          } else if (thisType != joldType) {  // rare out-of-sample type exception
            #pragma omp critical
            {
              joldType = type[j];  // fetch shared value again in case another thread bumped it while I was waiting.
              // Can't PRINT because we're likely not master. So accumulate message and print afterwards.
              if (thisType < joldType) {   // thisType<0 (type-exception)
                char temp[1001];
                int len = snprintf(temp, 1000,
                  "Column %d (\"%.*s\") bumped from '%s' to '%s' due to \"%.*s\" on row %zd\n",
                  j+1, colNames[j].len, colNamesAnchor + colNames[j].off,
                  typeName[(int)abs(joldType)], typeName[(int)abs(thisType)],
                  (int)(tch-fieldStart), fieldStart, myDTi+myNrow);
                typeBumpMsg = (char*) realloc(typeBumpMsg, typeBumpMsgSize + (size_t)len + 1);
                strcpy(typeBumpMsg+typeBumpMsgSize, temp);
                typeBumpMsgSize += (size_t)len;
                nTypeBump++;
                if (joldType>0) nTypeBumpCols++;
                type[j] = thisType;
              } // else other thread bumped to a (negative) higher or equal type, so do nothing
            }
          }
          *((char**) allBuffPos[size[j]]) += size[j];
          j++;
          if (*tch==eol) {
            tch += eolLen;
            if (tch == eof && soh) {
              fake_anchor += soh - tch;
              tch = soh;
              nextJump = eoh;
            }
            at_line_end = true;
            break;
          }
          tch++;
        }

        if (j < ncol)  {
          // not enough columns observed
          if (!fill) {
            #pragma omp critical
            if (!stopTeam) {
              stopTeam = true;
              snprintf(stopErr, stopErrSize,
                "Expecting %d cols but row %zd contains only %d cols (sep='%c'). " \
                "Consider fill=true. \"%s\"",
                ncol, myDTi, j, sep, strlim(tlineStart, 500, nextJump));
            }
            break;
          }
          while (j<ncol) {
            switch (type[j]) {
            case CT_BOOL8:
              *(int8_t*)myBuff1Pos = NA_BOOL8;
              break;
            case CT_INT32_BARE:
            case CT_INT32_FULL:
              *(int32_t*)myBuff4Pos = NA_INT32;
              break;
            case CT_INT64:
              *(int64_t*)myBuff8Pos = NA_INT64;
              break;
            case CT_FLOAT64:
              *(double*)myBuff8Pos = NA_FLOAT64;
              break;
            case CT_STRING:
              ((lenOff*)myBuff8Pos)->len = blank_is_a_NAstring ? INT8_MIN : 0;
              ((lenOff*)myBuff8Pos)->off = 0;
              break;
            default:
              break;
            }
            *((char**) allBuffPos[size[j]]) += size[j];
            j++;
          }
        }
        if (!at_line_end) {
          #pragma omp critical
          if (!stopTeam) {
            // DTPRINT("tch=%p (=lineStart+%d), *tch=%02x\n", tch, tch-tlineStart, *tch);
            stopTeam = true;
            snprintf(stopErr, stopErrSize,
              "Too many fields on out-of-sample row %zd. Read all %d expected columns but more are present. \"%s\"",
              myDTi, ncol, strlim(tlineStart, 500, nextJump));
          }
          break;
        }
        myNrow++;
      }
      if (verbose) { tt1 = wallclock(); thRead += tt1 - tt0; tt0 = tt1; }
      ctx.anchor = thisJumpStart;
      ctx.nRows = myNrow;
      postprocessBuffer(&ctx);

      #pragma omp ordered
      {
        // stopTeam could be true if a previous thread already stopped while I was waiting my turn
        if (!stopTeam && prevJumpEnd != thisJumpStart) {
          snprintf(stopErr, stopErrSize,
            "Jump %d did not finish counting rows exactly where jump %d found its first good line start: "
            "prevEnd(%p)\"%s\" != thisStart(prevEnd%+d)\"%s\"",
            jump-1, jump, (const void*)prevJumpEnd, strlim(prevJumpEnd,50,nextJump),
            (int)(thisJumpStart-prevJumpEnd), strlim(thisJumpStart,50,nextJump));
          stopTeam=true;
        }
        myDTi = DTi;  // fetch shared DTi (where to write my results to the answer). The previous thread just told me.
        ctx.DTi = myDTi;
        if (myDTi >= nrowLimit) {
          // nrowLimit was supplied and a previous thread reached that limit while I was counting my rows
          stopTeam=true;
          myNrow = 0;
        } else {
          myNrow = umin(myNrow, nrowLimit - myDTi); // for the last jump that reaches nrowLimit
        }
        // tell next thread 2 things :
        prevJumpEnd = tch; // i) the \n I finished on so it can check (above) it started exactly on that \n good line start
        DTi += myNrow;     // ii) which row in the final result it should start writing to. As soon as I know myNrow.
        ctx.nRows = myNrow;
        orderBuffer(&ctx);
      }
      // END ORDERED.
      // Next thread can now start its ordered section and write its results to the final DT at the same time as me.
      // Ordered has to be last in some OpenMP implementations currently. Logically though, pushBuffer happens now.
    }
    // Push out all buffers one last time.
    if (myNrow) {
      pushBuffer(&ctx);
    }
    // Done reading the file: each thread should now clean up its own buffers.
    free(myBuff8); myBuff8 = NULL;
    free(myBuff4); myBuff4 = NULL;
    free(myBuff1); myBuff1 = NULL;
    free(myBuff0); myBuff0 = NULL;
    freeThreadContext(&ctx);
  }
  //-- end parallel ------------------


  //*********************************************************************************************
  // [13] Finalize the datatable
  //*********************************************************************************************
  if (hasPrinted && verbose) DTPRINT("\n");
  if (verbose) DTPRINT("[13] Finalizing the datatable\n");
  if (firstTime) {
    tReread = tRead = wallclock();
    tTot = tRead-t0;
    if (hasPrinted || verbose) {
      DTPRINT("\rRead %zd rows x %d columns from %s file in ", DTi, ncol-ndrop, filesize_to_str(fileSize));
      DTPRINT("%02d:%06.3f wall clock time\n", (int)tTot/60, fmod(tTot,60.0));
      // since parallel, clock() cycles is parallel too: so wall clock will have to do
    }
    // not-bumped columns are assigned type -CT_STRING in the rerun, so we have to count types now
    if (verbose) {
      DTPRINT("Thread buffers were grown %d times (if all %d threads each grew once, this figure would be %d)\n",
               buffGrown, nth, nth);
      int typeCounts[NUMTYPE];
      for (int i=0; i<NUMTYPE; i++) typeCounts[i] = 0;
      for (int i=0; i<ncol; i++) typeCounts[ (int)abs(type[i]) ]++;
      DTPRINT("Final type counts\n");
      for (int i=0; i<NUMTYPE; i++) DTPRINT("%10d : %-9s\n", typeCounts[i], typeName[i]);
    }
    if (nTypeBump) {
      if (hasPrinted || verbose) DTPRINT("Rereading %d columns due to out-of-sample type exceptions.\n", nTypeBumpCols);
      if (verbose) DTPRINT("%s", typeBumpMsg);
      // TODO - construct and output the copy and pastable colClasses argument so user can avoid the reread time in future.
      free(typeBumpMsg);
    }
  } else {
    tReread = wallclock();
    tTot = tReread-t0;
    if (hasPrinted || verbose) {
      DTPRINT("\rReread %zd rows x %d columns in ", DTi, nTypeBumpCols);
      DTPRINT("%02d:%06.3f\n", (int)(tReread-tRead)/60, fmod(tReread-tRead,60.0));
    }
  }
  if (stopTeam && stopErr[0]!='\0') STOP(stopErr); // else nrowLimit applied and stopped early normally
  if (DTi > allocnrow) {
    if (nrowLimit > allocnrow) STOP("Internal error: DTi(%zd) > allocnrow(%zd) but nrows=%zd (not limited)",
                                    DTi, allocnrow, nrowLimit);
    // for the last jump that fills nrow limit, then ansi is +=buffi which is >allocnrow and correct
  } else if (DTi == allocnrow) {
    if (verbose) DTPRINT("Read %zd rows. Exactly what was estimated and allocated up front\n", DTi);
  } else {
    allocnrow = DTi;
  }
  setFinalNrow(DTi);

  // However if some of the columns could not be read due to out-of-sample
  // type exceptions, we'll need to re-read the input file.
  if (firstTime && nTypeBump) {
    rowSize1 = rowSize4 = rowSize8 = 0;
    nStringCols = 0;
    nNonStringCols = 0;
    for (int j=0, resj=-1; j<ncol; j++) {
      if (type[j] == CT_DROP) continue;
      resj++;
      if (type[j]<0) {
        // column was bumped due to out-of-sample type exception
        // reallocColType(resj, newType);
        type[j] = -type[j];
        size[j] = typeSize[type[j]];
        rowSize1 += (size[j] & 1);
        rowSize4 += (size[j] & 4);
        rowSize8 += (size[j] & 8);
        if (type[j] == CT_STRING) nStringCols++; else nNonStringCols++;
      } else if (type[j]>=1) {
        // we'll skip over non-bumped columns in the rerun, whilst still incrementing resi (hence not CT_DROP)
        // not -type[i] either because that would reprocess the contents of not-bumped columns wastefully
        type[j] = -CT_STRING;
        size[j] = 0;
      }
    }
    allocateDT(type, size, ncol, ncol - nStringCols - nNonStringCols, DTi);
    // reread from the beginning
    DTi = 0;
    prevJumpEnd = ch = sof;
    firstTime = false;
    goto read;
  }


  //*********************************************************************************************
  // [14] Epilogue
  //*********************************************************************************************
  if (verbose) {
    DTPRINT("=============================\n");
    if (tTot<0.000001) tTot=0.000001;  // to avoid nan% output in some trivially small tests where tot==0.000s
    DTPRINT("%8.3fs (%3.0f%%) Memory map %.3fGB file\n", tMap-t0, 100.0*(tMap-t0)/tTot, 1.0*fileSize/(1024*1024*1024));
    DTPRINT("%8.3fs (%3.0f%%) sep=", tLayout-tMap, 100.0*(tLayout-tMap)/tTot);
    DTPRINT(sep=='\t' ? "'\\t'" : (sep=='\n' ? "'\\n'" : "'%c'"), sep);
    DTPRINT(" ncol=%d and header detection\n", ncol);
    DTPRINT("%8.3fs (%3.0f%%) Column type detection using %zd sample rows\n",
            tColType-tLayout, 100.0*(tColType-tLayout)/tTot, sampleLines);
    DTPRINT("%8.3fs (%3.0f%%) Allocation of %zd rows x %d cols (%.3fGB)\n",
            tAlloc-tColType, 100.0*(tAlloc-tColType)/tTot, allocnrow, ncol, DTbytes/(1024.0*1024*1024));
    thNextGoodLine/=nth; thRead/=nth; thPush/=nth;
    double thWaiting = tRead-tAlloc-thNextGoodLine-thRead-thPush;
    DTPRINT("%8.3fs (%3.0f%%) Reading %d chunks of %.3fMB (%d rows) using %d threads\n",
            tRead-tAlloc, 100.0*(tRead-tAlloc)/tTot, nJumps, (double)chunkBytes/(1024*1024), (int)(chunkBytes/meanLineLen), nth);
    DTPRINT("   = %8.3fs (%3.0f%%) Finding first non-embedded \\n after each jump\n", thNextGoodLine, 100.0*thNextGoodLine/tTot);
    DTPRINT("   + %8.3fs (%3.0f%%) Parse to row-major thread buffers\n", thRead, 100.0*thRead/tTot);
    DTPRINT("   + %8.3fs (%3.0f%%) Transpose\n", thPush, 100.0*thPush/tTot);
    DTPRINT("   + %8.3fs (%3.0f%%) Waiting\n", thWaiting, 100.0*thWaiting/tTot);
    DTPRINT("%8.3fs (%3.0f%%) Rereading %d columns due to out-of-sample type exceptions\n",
            tReread-tRead, 100.0*(tReread-tRead)/tTot, nTypeBumpCols);
    DTPRINT("%8.3fs        Total\n", tTot);
  }
  freadCleanup();
  return 1;
}
