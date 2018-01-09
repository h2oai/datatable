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
#include "csv/reader.h"
#include "csv/reader_arff.h"
#include "csv/reader_fread.h"
#include <stdint.h>
#include <stdlib.h>   // strtod
#include <strings.h>  // strcasecmp
#include <cerrno>     // errno
#include <cstring>    // std::memcmp
#include "utils/exceptions.h"
#include "utils/omp.h"



//------------------------------------------------------------------------------
// GenericReader initialization
//------------------------------------------------------------------------------

GenericReader::GenericReader(const PyObj& pyrdr) {
  mbuf = nullptr;
  offset = 0;
  offend = 0;
  line = 0;
  freader = pyrdr;
  src_arg = pyrdr.attr("src");
  file_arg = pyrdr.attr("file");
  text_arg = pyrdr.attr("text");
  fileno = pyrdr.attr("fileno").as_int32();
  logger = pyrdr.attr("logger");

  init_verbose();
  init_nthreads();
  init_fill();
  init_maxnrows();
  init_skiptoline();
  init_sep();
  init_dec();
  init_quote();
  init_showprogress();
  init_header();
  init_nastrings();
  init_skipstring();
  init_stripwhite();
  init_skipblanklines();

  warnings_to_errors = 0;
}

GenericReader::~GenericReader() {
  if (mbuf) mbuf->release();
}


void GenericReader::init_verbose() {
  int8_t v = freader.attr("verbose").as_bool();
  verbose = (v > 0);
}

void GenericReader::init_nthreads() {
  int32_t nth = freader.attr("nthreads").as_int32();
  nthreads = nth;
  int32_t maxth = omp_get_max_threads();
  if (nthreads > maxth) nthreads = maxth;
  if (nthreads <= 0) nthreads += maxth;
  if (nthreads <= 0) nthreads = 1;
  trace("Using %d threads (requested=%d, max.available=%d)",
        nthreads, nth, maxth);
}

void GenericReader::init_fill() {
  int8_t v = freader.attr("fill").as_bool();
  fill = (v > 0);
  if (fill) trace("fill=True (incomplete lines will be padded with NAs)");
}

void GenericReader::init_maxnrows() {
  int64_t n = freader.attr("max_nrows").as_int64();
  max_nrows = (n < 0)? LONG_MAX : n;
  if (n >= 0) trace("max_nrows=%lld", static_cast<long long>(n));
  if (n >= 0) trace("max_nrows=%lld", static_cast<long long>(n));
}

void GenericReader::init_skiptoline() {
  int64_t n = freader.attr("skip_to_line").as_int64();
  skip_to_line = (n < 0)? 0 : n;
  if (n > 1) trace("skip_to_line = %lld", static_cast<long long>(n));
}

void GenericReader::init_sep() {
  size_t size = 0;
  const char* ch = freader.attr("sep").as_cstring(&size);
  if (ch == nullptr) {
    sep = '\xFF';
    trace("sep = <auto-detect>");
  } else if (size == 0 || *ch == '\n' || *ch == '\r') {
    sep = '\n';
    trace("sep = <single-column mode>");
  } else if (size > 1) {
    throw ValueError() << "Multi-character sep is not allowed: '" << ch << "'";
  } else {
    if (*ch=='"' || *ch=='\'' || *ch=='`' || ('0' <= *ch && *ch <= '9') ||
        ('a' <= *ch && *ch <= 'z') || ('A' <= *ch && *ch <= 'Z')) {
      throw ValueError() << "sep = '" << ch << "' is not allowed";
    }
    sep = *ch;
  }
}

void GenericReader::init_dec() {
  size_t size = 0;
  const char* ch = freader.attr("dec").as_cstring(&size);
  if (ch == nullptr || size == 0) {  // None | ""
    // TODO: switch to auto-detect mode
    dec = '.';
  } else if (size > 1) {
    throw ValueError() << "Multi-character decimal separator is not allowed: '"
                       << ch << "'";
  } else if (*ch == '.' || *ch == ',') {
    dec = *ch;
    trace("Decimal separator = '%c'", dec);
  } else {
    throw ValueError() << "dec = '" << ch << "' is not allowed";
  }
}

void GenericReader::init_quote() {
  size_t size = 0;
  const char* ch = freader.attr("quotechar").as_cstring(&size);
  if (ch == nullptr) {
    // TODO: switch to auto-detect mode
    quote = '"';
  } else if (size == 0) {
    quote = '\0';
  } else if (size > 1) {
    throw ValueError() << "Multi-character quote is not allowed: '"
                       << ch << "'";
  } else if (*ch == '"' || *ch == '\'' || *ch == '`') {
    quote = *ch;
    trace("Quote char = (%c)", quote);
  } else {
    throw ValueError() << "quotechar = (" << ch << ") is not allowed";
  }
}

void GenericReader::init_showprogress() {
  show_progress = freader.attr("show_progress").as_bool();
  if (show_progress) trace("show_progress = True");
}

void GenericReader::init_header() {
  header = freader.attr("header").as_bool();
  if (header >= 0) trace("header = %s", header? "True" : "False");
}

void GenericReader::init_nastrings() {
  na_strings = freader.attr("na_strings").as_cstringlist();
  blank_is_na = false;
  number_is_na = false;
  const char* const* ptr = na_strings;
  while (*ptr) {
    if (**ptr == '\0') {
      blank_is_na = true;
    } else {
      const char* ch = *ptr;
      size_t len = strlen(ch);
      if (*ch <= ' ' || ch[len-1] <= ' ') {
        throw ValueError() << "NA string \"" << ch << "\" has whitespace or "
                           << "control characters at the beginning or end";
      }
      if (strcasecmp(ch, "true") == 0 || strcasecmp(ch, "false") == 0) {
        throw ValueError() << "NA string \"" << ch << "\" looks like a boolean "
                           << "literal, this is not supported";
      }
      char* endptr;
      errno = 0;
      double f = strtod(ch, &endptr);
      if (errno == 0 && static_cast<size_t>(endptr - ch) == len) {
        (void) f;  // TODO: collect these values into an array?
        number_is_na = true;
      }
    }
    ptr++;
  }
  if (verbose) {
    if (*na_strings == nullptr) {
      trace("No na_strings provided");
    } else {
      std::string out = "na_strings = [";
      ptr = na_strings;
      while (*ptr++) {
        out += '"';
        out += ptr[-1];
        out += '"';
        if (*ptr) out += ", ";
      }
      out += ']';
      trace("%s", out.c_str());
      if (number_is_na) trace("  + some na strings look like numbers");
      if (blank_is_na)  trace("  + empty string is considered an NA");
    }
  }
}

void GenericReader::init_skipstring() {
  skipstring_arg = freader.attr("skip_to_string");
  skip_string = skipstring_arg.as_cstring();
  if (skip_string && skip_string[0]=='\0') skip_string = nullptr;
  if (skip_string && skip_to_line) {
    throw ValueError() << "Parameters `skip_to_line` and `skip_to_string` "
                       << "cannot be provided simultaneously";
  }
  if (skip_string) trace("skip_to_string = \"%s\"", skip_string);
}

void GenericReader::init_stripwhite() {
  strip_white = freader.attr("strip_white").as_bool();
  trace("strip_whitespace = %s", strip_white? "True" : "False");
}

void GenericReader::init_skipblanklines() {
  skip_blank_lines = freader.attr("skip_blank_lines").as_bool();
  trace("skip_blank_lines = %s", skip_blank_lines? "True" : "False");
}



//------------------------------------------------------------------------------
// Main read() function
//------------------------------------------------------------------------------

DataTablePtr GenericReader::read()
{
  open_input();
  detect_and_skip_bom();
  skip_to_line_number();
  skip_to_line_with_string();
  skip_initial_whitespace();
  skip_trailing_whitespace();

  DataTablePtr dt(nullptr);
  if (!dt) dt = read_empty_input();
  if (!dt) detect_improper_files();
  if (!dt) dt = FreadReader(*this).read();
  // if (!dt) dt = ArffReader(*this).read();
  if (!dt) throw RuntimeError() << "Unable to read input "
                                << src_arg.as_cstring();
  return dt;  // copy-elision
}



//------------------------------------------------------------------------------

const char* GenericReader::dataptr() const {
  return static_cast<const char*>(mbuf->at(offset));
}

size_t GenericReader::datasize() const {
  return mbuf->size() - offset - offend;
}

bool GenericReader::extra_byte_accessible() const {
  return (offend > 0);
}


__attribute__((format(printf, 2, 3)))
void GenericReader::trace(const char* format, ...) const {
  if (!verbose) return;
  va_list args;
  va_start(args, format);
  char *msg;
  if (strcmp(format, "%s") == 0) {
    msg = va_arg(args, char*);
  } else {
    msg = (char*) alloca(2001);
    vsnprintf(msg, 2000, format, args);
  }
  va_end(args);

  try {
    Py_ssize_t len = static_cast<Py_ssize_t>(strlen(msg));
    PyObject* pymsg = PyUnicode_Decode(msg, len, "utf-8",
                                       "backslashreplace");  // new ref
    if (!pymsg) throw PyError();
    logger.invoke("debug", "(O)", pymsg);
    Py_XDECREF(pymsg);
  } catch (const std::exception&) {
    // ignore any exceptions
  }
}



//------------------------------------------------------------------------------

void GenericReader::open_input() {
  offset = 0;
  offend = 0;
  line = 1;
  if (fileno > 0) {
    const char* src = src_arg.as_cstring();
    mbuf = new OvermapMemBuf(src, 1, fileno);
    size_t sz = mbuf->size();
    if (sz > 0) {
      sz--;
      *(mbuf->getstr() + sz) = '\0';
    }
    trace("Using file %s opened at fd=%d; size = %zu", src, fileno, sz);
    return;
  }
  size_t size = 0;
  const char* text = text_arg.as_cstring(&size);
  if (text) {
    mbuf = new ExternalMemBuf(text, size + 1);
    return;
  }
  const char* filename = file_arg.as_cstring();
  if (filename) {
    mbuf = new OvermapMemBuf(filename, 1);
    size_t sz = mbuf->size();
    if (sz > 0) {
      sz--;
      *(mbuf->getstr() + sz) = '\0';
    }
    trace("File \"%s\" opened, size: %zu", filename, sz);
    return;
  }
  throw RuntimeError() << "No input given to the GenericReader";
}


/**
 * Check whether the input contains BOM (Byte Order Mark), and if so skip it
 * modifying `offset`. If BOM indicates UTF-16 file, then recode the file into
 * UTF-8 (we cannot read UTF-16 directly).
 *
 * See: https://en.wikipedia.org/wiki/Byte_order_mark
 */
void GenericReader::detect_and_skip_bom() {
  size_t sz = datasize();
  const char* ch = dataptr();
  if (!sz) return;
  if (sz >= 3 && ch[0]=='\xEF' && ch[1]=='\xBB' && ch[2]=='\xBF') {
    offset += 3;
    trace("UTF-8 byte order mark EF BB BF found at the start of the file "
          "and skipped");
  } else
  if (sz >= 2 && ch[0] + ch[1] == '\xFE' + '\xFF') {
    trace("UTF-16 byte order mark %s found at the start of the file and "
          "skipped", ch[0]=='\xFE'? "FE FF" : "FF FE");
    decode_utf16();
    detect_and_skip_bom();  // just in case BOM was not discarded
  }
}


/**
 * Skip all initial whitespace in the file (i.e. empty lines and spaces).
 * However if `strip_white` is false, then we want to remove empty lines only,
 * leaving the initial spaces on the last line.
 *
 * This function modifies `offset` so that it points to: (1) the first
 * non-whitespace character in the file, if strip_white is true; or (2) the
 * first character on the first line that contains any non-whitespace
 * characters, if strip_white is false.
 *
 * Example
 * -------
 * Suppose input is the following (_ shows spaces, ␤ is newline, and ⇥ is tab):
 *
 *     _ _ _ _ ␤ _ ⇥ _ H e l l o …
 *
 * If strip_white=true, then this function will move the offset to character H;
 * whereas if strip_white=false, this function will move the offset to the
 * first space after '␤'.
 */
void GenericReader::skip_initial_whitespace() {
  const char* sof = dataptr();         // start-of-file
  const char* eof = sof + datasize();  // end-of-file
  const char* ch = sof;
  if (!sof) return;
  while ((ch < eof) && (*ch <= ' ') &&
         (*ch==' ' || *ch=='\n' || *ch=='\r' || *ch=='\t')) {
    ch++;
  }
  if (!strip_white) {
    ch--;
    while (ch >= sof && (*ch==' ' || *ch=='\t')) ch--;
    ch++;
  }
  if (ch > sof) {
    size_t doffset = static_cast<size_t>(ch - sof);
    offset += doffset;
    trace("Skipped %zu initial whitespace character(s)", doffset);
  }
}


void GenericReader::skip_trailing_whitespace() {
  const char* sof = dataptr();
  const char* eof = sof + datasize();
  const char* ch = eof - 1;
  if (!sof) return;
  // Skip characters \0 and Ctrl+Z
  while (ch >= sof && (*ch=='\0' || *ch=='\x1A')) {
    ch--;
  }
  if (ch < eof - 1) {
    size_t d = static_cast<size_t>(eof - 1 - ch);
    offend += d;
    if (d > 1) {
      trace("Skipped %zu trailing whitespace characters", d);
    }
  }
}


void GenericReader::skip_to_line_number() {
  if (skip_to_line <= line) return;
  const char* sof = dataptr();
  const char* eof = sof + datasize();
  const char* ch = sof;
  while (ch < eof && line < skip_to_line) {
    char c = *ch;
    if (c=='\n' || c=='\r') {
      ch += 1 + (ch+1 < eof && c + ch[1] == '\n' + '\r');
      line++;
      if (line == skip_to_line) break;
    } else {
      ch++;
    }
  }
  if (ch > sof) {
    offset += static_cast<size_t>(ch - sof);
    trace("Skipped to line %zd in the file", line);
  }
}


void GenericReader::skip_to_line_with_string() {
  const char* const ss = skip_string;
  if (!ss) return;
  const char* sof = dataptr();
  const char* eof = sof + datasize();
  const char* ch = sof;
  const char* line_start = sof;
  while (ch < eof) {
    if (*ch == *ss) {
      int d = 1;
      while (ss[d] != '\0' && ch + d < eof && ch[d] == ss[d]) d++;
      if (ss[d] == '\0') {
        if (line_start > sof) {
          offset += static_cast<size_t>(line_start - sof);
          trace("Skipped to line %zd containing skip_string = \"%s\"",
                line, ss);
        }
        return;
      } else {
        ch++;
      }
    }
    if (*ch=='\n' || *ch=='\r') {
      ch += 1 + (ch+1 < eof && *ch + ch[1] == '\n' + '\r');
      line_start = ch;
      line++;
    } else {
      ch++;
    }
  }
  throw ValueError() << "skip_string = \"" << skip_string << "\" was not found "
                     << "in the input";
}


DataTablePtr GenericReader::read_empty_input() {
  size_t size = datasize();
  const char* sof = dataptr();
  if (size == 0 || (size == 1 && *sof == '\0')) {
    trace("Input is empty, returning a (0 x 0) DataTable");
    Column** columns = static_cast<Column**>(malloc(sizeof(Column*)));
    columns[0] = nullptr;
    return DataTablePtr(new DataTable(columns));
  }
  return nullptr;
}


/**
 * This method will attempt to check whether the file looks like it is one
 * of the unsupported formats (such as HTML). If so, an exception will be
 * thrown.
 */
void GenericReader::detect_improper_files() {
  const char* ch = dataptr();
  const char* eof = ch + datasize();
  while (ch < eof && (*ch==' ' || *ch=='\t')) ch++;
  if (std::memcmp(ch, "<!DOCTYPE html>", 15) == 0) {
    throw RuntimeError() << src_arg.as_cstring() << " is an HTML file. Please "
        << "open it in a browser and then save in a plain text format.";
  }
}


void GenericReader::decode_utf16() {
  const char* ch = dataptr();
  size_t size = datasize();
  if (!size) return;
  if (ch[size - 1] == '\0') size--;

  Py_ssize_t ssize = static_cast<Py_ssize_t>(size);
  int byteorder = 0;
  tempstr = PyObj::fromPyObjectNewRef(
    PyUnicode_DecodeUTF16(ch, ssize, "replace", &byteorder)
  );
  PyObject* t = tempstr.as_pyobject();  // new ref
  // borrowed ref, belongs to PyObject `t`
  char* buf = PyUnicode_AsUTF8AndSize(t, &ssize);
  mbuf->release();
  mbuf = new ExternalMemBuf(buf, static_cast<size_t>(ssize) + 1);
  offset = 0;
  offend = 0;
  // the object `t` remains alive within `tempstr`
  Py_DECREF(t);
}




//------------------------------------------------------------------------------
// StrBuf2
//------------------------------------------------------------------------------

StrBuf2::StrBuf2(int64_t i) {
  colidx = i;
  writepos = 0;
  usedsize = 0;
  allocsize = 1024;
  strdata = static_cast<char*>(malloc(allocsize));
  if (!strdata) {
    throw RuntimeError()
          << "Unable to allocate 1024 bytes for a temporary buffer";
  }
}

StrBuf2::~StrBuf2() {
  free(strdata);
}

void StrBuf2::resize(size_t newsize) {
  strdata = static_cast<char*>(realloc(strdata, newsize));
  allocsize = newsize;
  if (!strdata) {
    throw RuntimeError() << "Unable to allocate " << newsize
                         << " bytes for a temporary buffer";
  }
}



//------------------------------------------------------------------------------
// FieldParseContext
//------------------------------------------------------------------------------
void parse_string(FieldParseContext&);


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
bool FieldParseContext::eol(const char** pch) {
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
bool FieldParseContext::end_of_field(const char* tch) {
  // \r is 13, \n is 10, and \0 is 0. The second part is optimized based on the
  // fact that the characters in the ASCII range 0..13 are very rare, so a
  // single check `tch<=13` is almost equivalent to checking whether `tch` is one
  // of \r, \n, \0. We cast to unsigned first because `char` type is signed by
  // default, and therefore characters in the range 0x80-0xFF are negative.
  // We use eol() because that looks at LFpresent inside it w.r.t. \r
  return *tch==sep || ((uint8_t)*tch<=13 && (*tch=='\0' || eol(&tch)));
}


const char* FieldParseContext::end_NA_string(const char* fieldStart) {
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


void FieldParseContext::skip_white() {
  // skip space so long as sep isn't space and skip tab so long as sep isn't tab
  if (whiteChar == 0) {   // whiteChar==0 means skip both ' ' and '\t';  sep is neither ' ' nor '\t'.
    while (*ch == ' ' || *ch == '\t') ch++;
  } else {
    while (*ch == whiteChar) ch++;  // sep is ' ' or '\t' so just skip the other one.
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
int FieldParseContext::countfields()
{
  const char* ch0 = ch;
  if (sep==' ') while (*ch==' ') ch++;  // multiple sep==' ' at the start does not mean sep
  skip_white();
  if (eol(&ch) || ch==eof) {
    ch++;
    return 0;
  }
  int ncol = 1;
  while (ch < eof) {
    parse_string(*this);
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
      ch++;
      return ncol;
    }
    if (*ch!='\0') {
      ch = ch0;
      return -1;  // -1 means this line not valid for this sep and quote rule
    }
    break;
  }
  return ncol;
}


bool FieldParseContext::nextGoodLine(int ncol) {
  const char* ch0 = ch;
  // we may have landed inside quoted field containing embedded sep and/or embedded \n
  // find next \n and see if 5 good lines follow. If not try next \n, and so on, until we find the real \n
  // We don't know which line number this is, either, because we jumped straight to it. So return true/false for
  // the line number and error message to be worked out up there.
  int attempts = 0;
  while (ch < eof && attempts++<30) {
    while (*ch!='\0' && *ch!='\n' && *ch!='\r') ch++;
    if (*ch=='\0') return false;
    eol(&ch);  // move to last byte of the line ending sequence
    ch++;      // move to first byte of next line
    int i = 0;
    const char* ch1 = ch;
    while (i<5 && countfields()==ncol) i++;
    ch = ch1;
    if (i==5) break;
  }
  if (*ch!='\0' && attempts<30) return true;
  ch = ch0;
  return false;
}
