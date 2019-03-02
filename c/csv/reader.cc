//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "csv/reader.h"
#include "csv/reader_arff.h"
#include "csv/reader_fread.h"
#include <stdlib.h>   // strtod
#include <strings.h>  // strcasecmp
#include <cerrno>     // errno
#include <cstring>    // std::memcmp
#include "datatable.h"
#include "encodings.h"
#include "options.h"
#include "utils/exceptions.h"
#include "utils/parallel.h"
#include "python/list.h"
#include "python/string.h"


//------------------------------------------------------------------------------
// GenericReader initialization
//------------------------------------------------------------------------------

GenericReader::GenericReader(const py::robj& pyrdr)
{
  sof = nullptr;
  eof = nullptr;
  line = 0;
  cr_is_newline = 0;
  printout_anonymize = config::fread_anonymize;
  printout_escape_unicode = false;
  freader  = pyrdr;
  src_arg  = pyrdr.get_attr("src");
  file_arg = pyrdr.get_attr("file");
  text_arg = pyrdr.get_attr("text");
  fileno   = pyrdr.get_attr("fileno").to_int32();
  logger   = pyrdr.get_attr("logger");

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
  init_overridecolumntypes();
}

// Copy-constructor will copy only the essential parts
GenericReader::GenericReader(const GenericReader& g)
  : freader(g.freader)  // for progress function / override columns
{
  // Input parameters
  nthreads         = g.nthreads;
  verbose          = g.verbose;
  sep              = g.sep;
  dec              = g.dec;
  quote            = g.quote;
  max_nrows        = g.max_nrows;
  skip_to_line     = 0;  // this parameter was already applied
  skip_to_string   = nullptr;
  na_strings       = g.na_strings;
  header           = g.header;
  strip_whitespace = g.strip_whitespace;
  skip_blank_lines = g.skip_blank_lines;
  report_progress  = g.report_progress;
  fill             = g.fill;
  blank_is_na      = g.blank_is_na;
  number_is_na     = g.number_is_na;
  override_column_types   = g.override_column_types;
  printout_anonymize      = g.printout_anonymize;
  printout_escape_unicode = g.printout_escape_unicode;
  t_open_input = g.t_open_input;
  // Runtime parameters
  input_mbuf = g.input_mbuf;
  sof     = g.sof;
  eof     = g.eof;
  line    = g.line;
  logger  = g.logger;   // for verbose messages / warnings
}

GenericReader::~GenericReader() {}


void GenericReader::init_verbose() {
  int8_t v = freader.get_attr("verbose").to_bool();
  verbose = (v > 0);
}

void GenericReader::init_nthreads() {
  #ifdef DTNOOPENMP
    nthreads = 1;
    trace("Using 1 thread because datatable was built without OMP support");
  #else
    int32_t nth = freader.get_attr("nthreads").to_int32();
    if (ISNA<int32_t>(nth)) {
      nthreads = config::nthreads;
      trace("Using default %d thread%s", nthreads, (nthreads==1? "" : "s"));
    } else {
      nthreads = config::normalize_nthreads(nth);
      int maxth = config::normalize_nthreads(0);
      trace("Using %d thread%s (requested=%d, max.available=%d)",
            nthreads, (nthreads==1? "" : "s"), nth, maxth);
    }
  #endif
}

void GenericReader::init_fill() {
  int8_t v = freader.get_attr("fill").to_bool();
  fill = (v > 0);
  if (fill) trace("fill=True (incomplete lines will be padded with NAs)");
}

void GenericReader::init_maxnrows() {
  int64_t n = freader.get_attr("max_nrows").to_int64();
  if (n < 0) {
    max_nrows = std::numeric_limits<size_t>::max();
  } else {
    max_nrows = static_cast<size_t>(n);
    trace("max_nrows = %lld", static_cast<long long>(n));
  }
}

void GenericReader::init_skiptoline() {
  int64_t n = freader.get_attr("skip_to_line").to_int64();
  skip_to_line = (n < 0)? 0 : static_cast<size_t>(n);
  if (n > 1) trace("skip_to_line = %zu", n);
}

void GenericReader::init_sep() {
  CString cstr = freader.get_attr("sep").to_cstring();
  size_t size = static_cast<size_t>(cstr.size);
  const char* ch = cstr.ch;
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
  CString cstr = freader.get_attr("dec").to_cstring();
  size_t size = static_cast<size_t>(cstr.size);
  const char* ch = cstr.ch;
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
  CString cstr = freader.get_attr("quotechar").to_cstring();
  size_t size = static_cast<size_t>(cstr.size);
  const char* ch = cstr.ch;
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
  report_progress = freader.get_attr("show_progress").to_bool();
  if (report_progress) trace("show_progress = True");
}

void GenericReader::init_header() {
  header = freader.get_attr("header").to_bool();
  if (header >= 0) trace("header = %s", header? "True" : "False");
}

void GenericReader::init_nastrings() {
  // TODO: `na_strings` should be properly destroyed in the end
  na_strings = freader.get_attr("na_strings").to_cstringlist();
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
  skipstring_arg = freader.get_attr("skip_to_string");
  skip_to_string = skipstring_arg.to_cstring().ch;
  if (skip_to_string && skip_to_string[0]=='\0') skip_to_string = nullptr;
  if (skip_to_string && skip_to_line) {
    throw ValueError() << "Parameters `skip_to_line` and `skip_to_string` "
                       << "cannot be provided simultaneously";
  }
  if (skip_to_string) trace("skip_to_string = \"%s\"", skip_to_string);
}

void GenericReader::init_stripwhite() {
  strip_whitespace = freader.get_attr("strip_whitespace").to_bool();
  trace("strip_whitespace = %s", strip_whitespace? "True" : "False");
}

void GenericReader::init_skipblanklines() {
  skip_blank_lines = freader.get_attr("skip_blank_lines").to_bool();
  trace("skip_blank_lines = %s", skip_blank_lines? "True" : "False");
}

void GenericReader::init_overridecolumntypes() {
  override_column_types = !freader.get_attr("_columns").is_none();
}



//------------------------------------------------------------------------------
// Main read_all() function
//------------------------------------------------------------------------------

std::unique_ptr<DataTable> GenericReader::read_all()
{
  open_input();
  detect_and_skip_bom();
  skip_to_line_number();
  skip_to_line_with_string();
  skip_initial_whitespace();
  skip_trailing_whitespace();

  std::unique_ptr<DataTable> dt(nullptr);
  if (!dt) dt = read_empty_input();
  if (!dt) detect_improper_files();
  if (!dt) dt = FreadReader(*this).read_all();
  // if (!dt) dt = ArffReader(*this).read_all();
  if (!dt) {
    throw RuntimeError() << "Unable to read input " << src_arg.to_string();
  }
  return dt;
}



//------------------------------------------------------------------------------

size_t GenericReader::datasize() const {
  return static_cast<size_t>(eof - sof);
}

bool GenericReader::extra_byte_accessible() const {
  const char* ptr = static_cast<const char*>(input_mbuf.rptr());
  return (eof < ptr + input_mbuf.size());
}


__attribute__((format(printf, 2, 3)))
void GenericReader::trace(const char* format, ...) const {
  if (!verbose) return;
  va_list args;
  va_start(args, format);
  _message("debug", format, args);
  va_end(args);
}

__attribute__((format(printf, 2, 3)))
void GenericReader::warn(const char* format, ...) const {
  va_list args;
  va_start(args, format);
  _message("warning", format, args);
  va_end(args);
}

void GenericReader::_message(
  const char* method, const char* format, va_list args) const
{
  static char shared_buffer[2001];
  char* msg;
  if (strcmp(format, "%s") == 0) {
    msg = va_arg(args, char*);
  } else {
    msg = shared_buffer;
    IGNORE_WARNING(-Wformat-nonliteral)
    vsnprintf(msg, 2000, format, args);
    RESTORE_WARNINGS()
  }

  if (omp_get_thread_num() == 0) {
    try {
      Py_ssize_t len = static_cast<Py_ssize_t>(strlen(msg));
      PyObject* pymsg = PyUnicode_Decode(msg, len, "utf-8",
                                         "backslashreplace");  // new ref
      if (!pymsg) throw PyError();
      logger.invoke(method, "(O)", pymsg);
      Py_XDECREF(pymsg);
    } catch (const std::exception&) {
      // ignore any exceptions
    }
  } else {
    if (strcmp(method, "debug") == 0) {
      delayed_message += msg;
    } else {
      // delayed_warning not implemented yet
    }
  }
}

void GenericReader::progress(double progress, int statuscode) {
  xassert(omp_get_thread_num() == 0);
  freader.invoke("_progress", "(di)", progress, statuscode);
}

void GenericReader::emit_delayed_messages() {
  xassert(omp_get_thread_num() == 0);
  if (delayed_message.size()) {
    trace("%s", delayed_message.c_str());
    delayed_message.clear();
  }
  // delayed_warning not implemented
}


static void print_byte(uint8_t c, char*& out) {
  *out++ = '\\';
  if (c == '\n') *out++ = 'n';
  else if (c == '\r') *out++ = 'r';
  else if (c == '\t') *out++ = 't';
  else {
    int d0 = int(c & 0xF);
    int d1 = int(c >> 4);
    *out++ = 'x';
    *out++ = static_cast<char>((d1 < 10 ? '0' : 'A' - 10) + d1);
    *out++ = static_cast<char>((d0 < 10 ? '0' : 'A' - 10) + d0);
  }
}

/**
 * Extract an input line starting from `ch` and until an end of line, or until
 * an end of file, or until `limit` characters have been printed, whichever
 * comes first. This function returns the string copied into an internal static
 * buffer. It can be called more than once, provided that the total size of all
 * requested strings does not exceed `BUFSIZE`.
 *
 * The function will attempt to escape all non-printable characters. It can
 * optionally escape all unicode characters too; or anonymize all text content.
 */
const char* GenericReader::repr_source(const char* ch, size_t limit) const {
  return repr_binary(ch, eof, limit);
}


const char* GenericReader::repr_binary(
  const char* ch, const char* endch, size_t limit) const
{
  static constexpr size_t BUFSIZE = 1002;
  static char buf[BUFSIZE + 10];
  static size_t pos = 0;

  if (pos + limit + 1 > BUFSIZE) pos = 0;
  char* ptr = buf + pos;
  char* out = ptr;
  char* end = ptr + limit;
  bool stopped_at_newline = false;
  while (out < end) {
    stopped_at_newline = true;
    if (ch == endch) break;
    uint8_t c = static_cast<uint8_t>(*ch++);

    // Stop at a newline
    if (c == '\n') break;
    if (c == '\r') {
      if (cr_is_newline) break;
      if (ch < endch     && ch[0] == '\n') break;  // \r\n
      if (ch + 1 < endch && ch[0] == '\r' && ch[1] == '\n') break;  // \r\r\n
    }
    stopped_at_newline = false;

    // Control characters
    if (c < 0x20) {
      print_byte(c, out);
    }

    // Normal ASCII characters
    else if (c < 0x80) {
      *out++ = (printout_anonymize && c >= '1' && c <= '9')? '1' :
               (printout_anonymize && c >= 'a' && c <= 'z')? 'a' :
               (printout_anonymize && c >= 'A' && c <= 'Z')? 'A' :
               static_cast<char>(c);
    }

    // Unicode (UTF-8) characters
    else if (c < 0xF8) {
      auto usrc = reinterpret_cast<const uint8_t*>(ch - 1);
      size_t cp_bytes = (c < 0xE0)? 2 : (c < 0xF0)? 3 : 4;
      bool cp_valid = (ch + cp_bytes - 2 < endch) &&
                      is_valid_utf8(usrc, cp_bytes);
      if (!cp_valid || printout_escape_unicode) {
        print_byte(c, out);
      } else if (printout_anonymize) {
        *out++ = 'U';
        ch += cp_bytes - 1;
      } else {
        // Copy the unicode character
        *out++ = static_cast<char>(c);
        *out++ = *ch++;
        if (cp_bytes >= 3) *out++ = *ch++;
        if (cp_bytes == 4) *out++ = *ch++;
      }
    }

    // Invalid bytes
    else {
      print_byte(c, out);
    }
  }
  if (out > end) out = end;
  if (!stopped_at_newline) {
    out[-1] = '.';
    out[-2] = '.';
    out[-3] = '.';
  }
  *out++ = '\0';
  pos += static_cast<size_t>(out - ptr);
  return ptr;
}



//------------------------------------------------------------------------------
// Input handling
//------------------------------------------------------------------------------

void GenericReader::open_input() {
  double t0 = wallclock();
  CString text;
  const char* filename = nullptr;
  size_t extra_byte = 0;
  if (fileno > 0) {
    const char* src = src_arg.to_cstring().ch;
    input_mbuf = MemoryRange::overmap(src, /* extra = */ 1, fileno);
    size_t sz = input_mbuf.size();
    if (sz > 0) {
      sz--;
      static_cast<char*>(input_mbuf.wptr())[sz] = '\0';
      extra_byte = 1;
    }
    trace("Using file %s opened at fd=%d; size = %zu", src, fileno, sz);

  } else if ((text = text_arg.to_cstring())) {
    size_t size = static_cast<size_t>(text.size);
    input_mbuf = MemoryRange::external(text.ch, size + 1);
    extra_byte = 1;
    input_is_string = true;

  } else if ((filename = file_arg.to_cstring().ch)) {
    input_mbuf = MemoryRange::overmap(filename, /* extra = */ 1);
    size_t sz = input_mbuf.size();
    if (sz > 0) {
      sz--;
      static_cast<char*>(input_mbuf.xptr())[sz] = '\0';
      extra_byte = 1;
    }
    trace("File \"%s\" opened, size: %zu", filename, sz);

  } else {
    throw RuntimeError() << "No input given to the GenericReader";
  }
  line = 1;
  sof = static_cast<char*>(input_mbuf.wptr());
  eof = sof + input_mbuf.size() - extra_byte;
  if (eof) xassert(*eof == '\0');

  if (verbose) {
    trace("==== file sample ====");
    const char* ch = sof;
    bool newline = true;
    for (int i = 0; i < 5 && ch < eof; i++) {
      if (newline) trace("%s", repr_source(ch, 100));
      else         trace("...%s", repr_source(ch, 97));
      const char* start = ch;
      const char* end = std::min(eof, ch + 10000);
      while (ch < end) {
        char c = *ch++;
        // simplified newline sequence. TODO: replace with `skip_eol()`
        if (c == '\n' || c == '\r') {
          if ((*ch == '\r' || *ch == '\n') && *ch != c) ch++;
          break;
        }
      }
      if (ch == end && ch < eof) {
        ch = start + 100;
        newline = false;
      } else {
        newline = true;
      }
    }
    trace("=====================");
  }
  t_open_input = wallclock() - t0;
}


/**
 * Check whether the input contains BOM (Byte Order Mark), and if so skip it
 * modifying `sof`. If BOM indicates UTF-16 file, then recode the file into
 * UTF-8 (we cannot read UTF-16 directly).
 *
 * See: https://en.wikipedia.org/wiki/Byte_order_mark
 */
void GenericReader::detect_and_skip_bom() {
  size_t sz = datasize();
  const char* ch = sof;
  if (!sz) return;
  if (sz >= 3 && ch[0]=='\xEF' && ch[1]=='\xBB' && ch[2]=='\xBF') {
    sof += 3;
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
 * However if `strip_whitespace` is false, then we want to remove empty lines
 * only, leaving the initial spaces on the last line.
 *
 * This function modifies `sof` so that it points to: (1) the first
 * non-whitespace character in the file, if strip_whitespace is true; or (2) the
 * first character on the first line that contains any non-whitespace
 * characters, if strip_whitespace is false.
 *
 * Example
 * -------
 * Suppose input is the following (_ shows spaces, ␤ is newline, and ⇥ is tab):
 *
 *     _ _ _ _ ␤ _ ⇥ _ H e l l o …
 *
 * If strip_whitespace=true, then this function will move the `sof` to
 * character H; whereas if strip_whitespace=false, this function will move the
 * `sof` to the first space after '␤'.
 */
void GenericReader::skip_initial_whitespace() {
  const char* ch = sof;
  if (!sof) return;
  while ((ch < eof) && (*ch <= ' ') &&
         (*ch==' ' || *ch=='\n' || *ch=='\r' || *ch=='\t')) {
    ch++;
  }
  if (!strip_whitespace) {
    ch--;
    while (ch >= sof && (*ch==' ' || *ch=='\t')) ch--;
    ch++;
  }
  if (ch > sof) {
    size_t doffset = static_cast<size_t>(ch - sof);
    sof = ch;
    trace("Skipped %zu initial whitespace character(s)", doffset);
  }
}


void GenericReader::skip_trailing_whitespace() {
  const char* ch = eof - 1;
  if (!sof) return;
  // Skip characters \0 and Ctrl+Z
  while (ch >= sof && (*ch=='\0' || *ch=='\x1A')) {
    ch--;
  }
  if (ch < eof - 1) {
    size_t d = static_cast<size_t>(eof - 1 - ch);
    eof = ch + 1;
    if (d > 1) {
      trace("Skipped %zu trailing whitespace characters", d);
    }
  }
}


void GenericReader::skip_to_line_number() {
  if (skip_to_line <= line) return;
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
    sof = ch;
    trace("Skipped to line %zd in the file", line);
  }
}


void GenericReader::skip_to_line_with_string() {
  const char* const ss = skip_to_string;
  if (!ss) return;
  const char* ch = sof;
  const char* line_start = sof;
  while (ch < eof) {
    if (*ch == *ss) {
      int d = 1;
      while (ss[d] != '\0' && ch + d < eof && ch[d] == ss[d]) d++;
      if (ss[d] == '\0') {
        if (line_start > sof) {
          sof = line_start;
          trace("Skipped to line %zd containing skip_to_string = \"%s\"",
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
  throw ValueError() << "skip_to_string = \"" << skip_to_string << "\" was not found "
                     << "in the input";
}


std::unique_ptr<DataTable> GenericReader::read_empty_input() {
  size_t size = datasize();
  if (size == 0 || (size == 1 && *sof == '\0')) {
    trace("Input is empty, returning a (0 x 0) DataTable");
    return std::unique_ptr<DataTable>(new DataTable());
  }
  return nullptr;
}


/**
 * This method will attempt to check whether the file looks like it is one
 * of the unsupported formats (such as HTML). If so, an exception will be
 * thrown.
 */
void GenericReader::detect_improper_files() {
  const char* ch = sof;
  // --- detect HTML ---
  while (ch < eof && (*ch==' ' || *ch=='\t')) ch++;
  if (ch + 15 < eof && std::memcmp(ch, "<!DOCTYPE html>", 15) == 0) {
    throw RuntimeError() << src_arg.to_string() << " is an HTML file. Please "
        << "open it in a browser and then save in a plain text format.";
  }
  // --- detect Feather ---
  if (sof + 8 < eof && std::memcmp(sof, "FEA1", 4) == 0
                    && std::memcmp(eof - 4, "FEA1", 4) == 0) {
    throw RuntimeError() << src_arg.to_string() << " is a feather file, it "
        "cannot be read with fread.";
  }
}


void GenericReader::decode_utf16() {
  const char* ch = sof;
  size_t size = datasize();
  if (!size) return;

  Py_ssize_t ssize = static_cast<Py_ssize_t>(size);
  int byteorder = 0;
  PyObject* t = PyUnicode_DecodeUTF16(ch, ssize, "replace", &byteorder);
  tempstr = py::oobj::from_new_reference(t);

  // `buf` is a borrowed ref, belongs to PyObject* `t`
  const char* buf = PyUnicode_AsUTF8AndSize(t, &ssize);
  input_mbuf = MemoryRange::external(
                  const_cast<void*>(static_cast<const void*>(buf)),
                  static_cast<size_t>(ssize) + 1);
  sof = static_cast<char*>(input_mbuf.wptr());
  eof = sof + ssize + 1;
}



void GenericReader::report_columns_to_python() {
  size_t ncols = columns.size();

  if (override_column_types) {
    py::olist colDescriptorList(ncols);
    for (size_t i = 0; i < ncols; i++) {
      colDescriptorList.set(i, columns[i].py_descriptor());
    }

    py::olist newTypesList =
      freader.invoke("_override_columns0", "(O)",
                     std::move(colDescriptorList).release()).to_pylist();

    if (newTypesList) {
      for (size_t i = 0; i < ncols; i++) {
        py::robj elem = newTypesList[i];
        columns[i].set_rtype(elem.to_int64());
      }
    }
  } else {
    py::olist colNamesList(ncols);
    for (size_t i = 0; i < ncols; ++i) {
      colNamesList.set(i, py::ostring(columns[i].get_name()));
    }
    freader.invoke("_set_column_names", "(O)",
                   std::move(colNamesList).release());
  }
}



dtptr GenericReader::makeDatatable() {
  size_t ncols = columns.size();
  size_t nrows = columns.get_nrows();
  size_t ocols = columns.nColumnsInOutput();
  std::vector<Column*> ccols;
  ccols.reserve(ocols);
  for (size_t i = 0; i < ncols; ++i) {
    dt::read::Column& col = columns[i];
    if (!col.is_in_output()) continue;
    MemoryRange databuf = col.extract_databuf();
    MemoryRange strbuf = col.extract_strbuf();
    SType stype = col.get_stype();
    ccols.push_back((stype == SType::STR32 || stype == SType::STR64)
      ? new_string_column(nrows, std::move(databuf), std::move(strbuf))
      : Column::new_mbuf_column(stype, std::move(databuf))
    );
  }
  py::olist names = freader.get_attr("_colnames").to_pylist();
  return dtptr(new DataTable(std::move(ccols), names));
}
