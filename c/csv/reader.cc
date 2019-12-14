//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include <stdlib.h>             // strtod
#include <cerrno>               // errno
#include <cstring>              // std::memcmp
#include "csv/reader.h"
#include "csv/reader_arff.h"
#include "csv/reader_fread.h"
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "python/_all.h"
#include "python/string.h"
#include "utils/exceptions.h"
#include "utils/misc.h"         // wallclock
#include "utils/macros.h"
#include "datatable.h"
#include "encodings.h"
#include "options.h"


//------------------------------------------------------------------------------
// options
//------------------------------------------------------------------------------

static bool log_anonymize = false;
static bool log_escape_unicode = false;

void GenericReader::init_options() {
  dt::register_option(
    "fread.anonymize",
    []{ return py::obool(log_anonymize); },
    [](const py::Arg& value){ log_anonymize = value.to_bool_strict(); },
    "[DEPRECATED] same as fread.log.anonymize");

  dt::register_option(
    "fread.log.anonymize",
    []{ return py::obool(log_anonymize); },
    [](const py::Arg& value){ log_anonymize = value.to_bool_strict(); },
    "If True, any snippets of data being read that are printed in the\n"
    "log will be first anonymized by converting all non-0 digits to 1,\n"
    "all lowercase letters to a, all uppercase letters to A, and all\n"
    "unicode characters to U.\n"
    "This option is useful in production systems when reading sensitive\n"
    "data that must not accidentally leak into log files or be printed\n"
    "with the error messages.");

  dt::register_option(
    "fread.log.escape_unicode",
    []{ return py::obool(log_escape_unicode); },
    [](const py::Arg& value){ log_escape_unicode = value.to_bool_strict(); },
    "If True, all unicode characters in the verbose log will be written\n"
    "in hexadecimal notation. Use this option if your terminal cannot\n"
    "print unicode, or if the output gets somehow corrupted because of\n"
    "the unicode characters.");
}




//------------------------------------------------------------------------------
// GenericReader initialization
//------------------------------------------------------------------------------

GenericReader::GenericReader()
{
  na_strings = nullptr;
  sof = nullptr;
  eof = nullptr;
  line = 0;
  cr_is_newline = 0;
}


GenericReader::GenericReader(const py::robj& pyrdr)
{
  job = std::make_shared<dt::progress::work>(WORK_PREPARE + WORK_READ);
  sof = nullptr;
  eof = nullptr;
  line = 0;
  cr_is_newline = 0;
  src_arg  = pyrdr.get_attr("_src");
  file_arg = pyrdr.get_attr("_file");
  text_arg = pyrdr.get_attr("_text");
  fileno   = pyrdr.get_attr("_fileno").to_int32();

  init_verbose(   py::Arg(pyrdr.get_attr("_verbose"), "Parameter `verbose`"));
  init_logger(    py::Arg(pyrdr.get_attr("_logger"), "Parameter `logger`"));
  init_nthreads(  py::Arg(pyrdr.get_attr("_nthreads"), "Parameter `nthreads`"));
  init_fill(      py::Arg(pyrdr.get_attr("_fill"), "Parameter `fill`"));
  init_maxnrows(  py::Arg(pyrdr.get_attr("_maxnrows"), "Parameter `max_nrows`"));
  init_skiptoline(py::Arg(pyrdr.get_attr("_skip_to_line"), "Parameter `skip_to_line`"));
  init_sep(       py::Arg(pyrdr.get_attr("_sep"), "Parameter `sep`"));
  init_dec(       py::Arg(pyrdr.get_attr("_dec"), "Parameter `dec`"));
  init_quote(     py::Arg(pyrdr.get_attr("_quotechar"), "Parameter `quotechar`"));
  init_header(    py::Arg(pyrdr.get_attr("_header"), "Parameter `header`"));
  init_nastrings( py::Arg(pyrdr.get_attr("_nastrings"), "Parameter `na_strings`"));
  init_skipstring(py::Arg(pyrdr.get_attr("_skip_to_string"), "Parameter `skip_to_string`"));
  init_stripwhite(py::Arg(pyrdr.get_attr("_strip_whitespace"), "Parameter `strip_whitespace`"));
  init_skipblanks(py::Arg(pyrdr.get_attr("_skip_blank_lines"), "Parameter `skip_blank_lines`"));
  init_columns(   py::Arg(pyrdr.get_attr("_columns"), "Parameter `columns`"));
}

// Copy-constructor will copy only the essential parts
GenericReader::GenericReader(const GenericReader& g)
{
  // Input parameters
  nthreads         = g.nthreads;
  verbose          = g.verbose;
  sep              = g.sep;
  dec              = g.dec;
  quote            = g.quote;
  max_nrows        = g.max_nrows;
  skip_to_line     = 0;  // this parameter was already applied
  na_strings       = g.na_strings;
  header           = g.header;
  strip_whitespace = g.strip_whitespace;
  skip_blank_lines = g.skip_blank_lines;
  fill             = g.fill;
  blank_is_na      = g.blank_is_na;
  number_is_na     = g.number_is_na;
  columns_arg      = g.columns_arg;
  t_open_input     = g.t_open_input;
  // Runtime parameters
  job     = g.job;
  input_mbuf = g.input_mbuf;
  sof     = g.sof;
  eof     = g.eof;
  line    = g.line;
  logger  = g.logger;   // for verbose messages / warnings
}

GenericReader::~GenericReader() {}


void GenericReader::init_verbose(const py::Arg& arg) {
  verbose = arg.to<bool>(false);
}

void GenericReader::init_nthreads(const py::Arg& arg) {
  int32_t DEFAULT = -(1 << 30);
  int32_t nth = arg.to<int32_t>(DEFAULT);
  int maxth = static_cast<int>(dt::num_threads_in_pool());
  if (nth == DEFAULT) {
    nthreads = maxth;
    trace("Using default %d thread%s", nthreads, (nthreads==1? "" : "s"));
  } else {
    nthreads = nth;
    if (nthreads > maxth) nthreads = maxth;
    if (nthreads <= 0) nthreads += maxth;
    if (nthreads <= 0) nthreads = 1;
    trace("Using %d thread%s (requested=%d, max.available=%d)",
          nthreads, (nthreads==1? "" : "s"), nth, maxth);
  }
}

void GenericReader::init_fill(const py::Arg& arg) {
  fill = arg.to<bool>(false);
  if (fill) {
    trace("fill=True (incomplete lines will be padded with NAs)");
  }
}

void GenericReader::init_maxnrows(const py::Arg& arg) {
  int64_t n = arg.to<int64_t>(-1);
  if (n < 0) {
    max_nrows = std::numeric_limits<size_t>::max();
  } else {
    max_nrows = static_cast<size_t>(n);
    trace("max_nrows = %lld", static_cast<long long>(n));
  }
}

void GenericReader::init_skiptoline(const py::Arg& arg) {
  int64_t n = arg.to<int64_t>(-1);
  skip_to_line = (n < 0)? 0 : static_cast<size_t>(n);
  if (n > 1) trace("skip_to_line = %zu", n);
}

void GenericReader::init_sep(const py::Arg& arg) {
  if (arg.is_none_or_undefined()) {
    sep = '\xFF';
    trace("sep = <auto-detect>");
    return;
  }
  auto str = arg.to_string();
  size_t size = str.size();
  const char c = size? str[0] : '\n';
  if (c == '\n' || c == '\r') {
    sep = '\n';
    trace("sep = <single-column mode>");
  } else if (size > 1) {
    throw NotImplError() << "Multi-character or unicode separators are not "
                            "supported: '" << str << "'";
  } else {
    if (c=='"' || c=='\'' || c=='`' || ('0' <= c && c <= '9') ||
        ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')) {
      throw ValueError() << "Separator `" << c << "` is not allowed";
    }
    sep = c;
  }
}

void GenericReader::init_dec(const py::Arg& arg) {
  if (arg.is_none_or_undefined()) {
    // TODO: switch to auto-detect mode
    dec = '.';
    return;
  }
  auto str = arg.to_string();
  if (str.size() > 1) {
    throw ValueError() << "Multi-character decimal separator is not allowed: '"
                       << str << "'";
  }
  // If `str` is empty, then `c` will become '\0'
  const char c = str[0];
  if (c == '.' || c == ',') {
    dec = c;
    trace("Decimal separator = '%c'", dec);
  } else {
    throw ValueError() << "Only dec='.' or ',' are allowed";
  }
}

void GenericReader::init_quote(const py::Arg& arg) {
  auto str = arg.to<std::string>("\"");
  if (str.size() == 0) {
    quote = '\0';
  } else if (str.size() > 1) {
    throw ValueError() << "Multi-character quote is not allowed: '"
                       << str << "'";
  } else if (str[0] == '"' || str[0] == '\'' || str[0] == '`') {
    quote = str[0];
    trace("Quote char = (%c)", quote);
  } else {
    throw ValueError() << "quotechar = (" << str << ") is not allowed";
  }
}

void GenericReader::init_header(const py::Arg& arg) {
  if (arg.is_none_or_undefined()) {
    header = GETNA<int8_t>();
  } else {
    header = arg.to_bool_strict();
    trace("header = %s", header? "True" : "False");
  }
}

void GenericReader::init_nastrings(const py::Arg& arg) {
  na_strings_container = arg.to<strvec>({"NA"});
  size_t n = na_strings_container.size();
  na_strings_ptr = std::unique_ptr<const char*[]>(new const char*[n+1]);
  na_strings = na_strings_ptr.get();
  for (size_t i = 0; i < n; ++i) {
    na_strings[i] = na_strings_container[i].data();
  }
  na_strings[n] = nullptr;
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
      if (strcmp(ch, "true") == 0 || strcmp(ch, "True") == 0 ||
          strcmp(ch, "TRUE") == 0 || strcmp(ch, "false") == 0 ||
          strcmp(ch, "False") == 0 || strcmp(ch, "FALSE") == 0)
      {
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

void GenericReader::init_skipstring(const py::Arg& arg) {
  skip_to_string = arg.to<std::string>("");
  if (!skip_to_string.empty()) {
    if (skip_to_line) {
      throw ValueError() << "Parameters `skip_to_line` and `skip_to_string` "
                         << "cannot be provided simultaneously";
    }
    trace("skip_to_string = \"%s\"", skip_to_string.data());
  }
}

void GenericReader::init_stripwhite(const py::Arg& arg) {
  strip_whitespace = arg.to<bool>(true);
  trace("strip_whitespace = %s", strip_whitespace? "True" : "False");
}

void GenericReader::init_skipblanks(const py::Arg& arg) {
  skip_blank_lines = arg.to<bool>(true);
  trace("skip_blank_lines = %s", skip_blank_lines? "True" : "False");
}

void GenericReader::init_columns(const py::Arg& arg) {
  if (arg.is_defined()) {
    columns_arg = arg.to_oobj();
  }
}

void GenericReader::init_logger(const py::Arg& arg) {
  if (arg.is_none_or_undefined()) {
    if (verbose) {
      logger = py::oobj::import("datatable.fread", "_DefaultLogger").call();
    }
  } else {
    logger = arg.to_oobj();
    verbose = true;
  }
}




//------------------------------------------------------------------------------
// Main read_all() function
//------------------------------------------------------------------------------

py::oobj GenericReader::read_all()
{
  open_input();
  bool done = read_jay();

  if (!done) {
    detect_and_skip_bom();
    skip_to_line_number();
    skip_to_line_with_string();
    skip_initial_whitespace();
    skip_trailing_whitespace();
    job->add_done_amount(WORK_PREPARE);

    read_empty_input() ||
    detect_improper_files() ||
    read_csv();
  }

  if (outputs.empty()) {
    throw RuntimeError() << "Unable to read input " << src_arg.to_string();
  }

  job->done();
  return outputs[0];
}



//------------------------------------------------------------------------------

size_t GenericReader::datasize() const {
  return static_cast<size_t>(eof - sof);
}

bool GenericReader::extra_byte_accessible() const {
  const char* ptr = static_cast<const char*>(input_mbuf.rptr());
  return (eof < ptr + input_mbuf.size());
}


#if !DT_OS_WINDOWS
__attribute__((format(printf, 2, 3)))
#endif
void GenericReader::trace(const char* format, ...) const {
  if (!verbose) return;
  va_list args;
  va_start(args, format);
  _message("debug", format, args);
  va_end(args);
}

#if !DT_OS_WINDOWS
__attribute__((format(printf, 2, 3)))
#endif
void GenericReader::warn(const char* format, ...) const {
  va_list args;
  va_start(args, format);
  _message("warning", format, args);
  va_end(args);
}

static void _send_message_to_python(
    const char* method, const char* message, const py::oobj& logger)
{
  PyObject* pymsg = PyUnicode_FromString(message);
  if (pymsg) {
    PyObject* py_logger = logger.to_borrowed_ref();
    PyObject* res = PyObject_CallMethod(py_logger, method, "(O)", pymsg);
    Py_XDECREF(res);
  }
  PyErr_Clear();  // ignore any exceptions
  Py_XDECREF(pymsg);
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
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wformat-nonliteral"
    vsnprintf(msg, 2000, format, args);
    #pragma GCC diagnostic pop
  }

  if (dt::num_threads_in_team() == 0) {
    _send_message_to_python(method, msg, logger);
  } else {
    // Any other team-wide mutex would work too
    std::lock_guard<std::mutex> lock(dt::python_mutex());
    delayed_message += msg;
  }
}

void GenericReader::emit_delayed_messages() {
  std::lock_guard<std::mutex> lock(dt::python_mutex());
  if (delayed_message.size()) {
    _send_message_to_python("debug", delayed_message.c_str(), logger);
    delayed_message.clear();
  }
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
      *out++ = (log_anonymize && c >= '1' && c <= '9')? '1' :
               (log_anonymize && c >= 'a' && c <= 'z')? 'a' :
               (log_anonymize && c >= 'A' && c <= 'Z')? 'A' :
               static_cast<char>(c);
    }

    // Unicode (UTF-8) characters
    else if (c < 0xF8) {
      auto usrc = reinterpret_cast<const uint8_t*>(ch - 1);
      size_t cp_bytes = (c < 0xE0)? 2 : (c < 0xF0)? 3 : 4;
      bool cp_valid = (ch + cp_bytes - 2 < endch) &&
                      is_valid_utf8(usrc, cp_bytes);
      if (!cp_valid || log_escape_unicode) {
        print_byte(c, out);
      } else if (log_anonymize) {
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
  if (input_mbuf) return;
  double t0 = wallclock();
  CString text;
  const char* filename = nullptr;
  size_t extra_byte = 0;
  if (fileno > 0) {
    const char* src = src_arg.to_cstring().ch;
    input_mbuf = Buffer::overmap(src, /* extra = */ 1, fileno);
    size_t sz = input_mbuf.size();
    if (sz > 0) {
      sz--;
      static_cast<char*>(input_mbuf.wptr())[sz] = '\0';
      extra_byte = 1;
    }
    trace("Using file %s opened at fd=%d; size = %zu", src, fileno, sz);

  } else if ((text = text_arg.to_cstring())) {
    size_t size = static_cast<size_t>(text.size);
    input_mbuf = Buffer::external(text.ch, size + 1);
    extra_byte = 1;
    input_is_string = true;

  } else if ((filename = file_arg.to_cstring().ch)) {
    input_mbuf = Buffer::overmap(filename, /* extra = */ 1);
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
  if (skip_to_string.empty()) return;
  const char* const ss = skip_to_string.data();
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


bool GenericReader::read_empty_input() {
  size_t size = datasize();
  if (size == 0 || (size == 1 && *sof == '\0')) {
    trace("Input is empty, returning a (0 x 0) DataTable");
    job->add_done_amount(WORK_READ);
    outputs.push_back(py::Frame::oframe(new DataTable()));
    return true;
  }
  return false;
}


/**
 * This method will attempt to check whether the file looks like it is one
 * of the unsupported formats (such as HTML). If so, an exception will be
 * thrown.
 */
bool GenericReader::detect_improper_files() {
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
  return false;
}


bool GenericReader::read_jay() {
  if (eof - sof >= 24 &&
      sof[0] == 'J' && sof[1] == 'A' && sof[2] == 'Y' &&
      sof[3] >= '1' && sof[3] <= '9')
  {
    job->add_done_amount(WORK_PREPARE);
    // Buffer's size can be larger than the input
    input_mbuf.resize(datasize());
    DataTable* dt = open_jay_from_mbuf(input_mbuf);
    job->add_done_amount(WORK_READ);
    outputs.push_back(py::Frame::oframe(dt));
    return true;
  }
  return false;
}


bool GenericReader::read_csv() {
  auto dt = FreadReader(*this).read_all();
  if (dt) {
    outputs.push_back(py::Frame::oframe(dt.release()));
    return true;
  }
  return false;
}



void GenericReader::decode_utf16() {
  const char* ch = sof;
  size_t size = datasize();
  if (!size) return;
  job->add_work_amount(WORK_DECODE_UTF16);
  job->set_message("Decoding UTF-16");
  dt::progress::subtask subjob(*job, WORK_DECODE_UTF16);

  Py_ssize_t ssize = static_cast<Py_ssize_t>(size);
  int byteorder = 0;
  PyObject* t = PyUnicode_DecodeUTF16(ch, ssize, "replace", &byteorder);
  tempstr = py::oobj::from_new_reference(t);

  // `buf` is a borrowed ref, belongs to PyObject* `t`
  const char* buf = PyUnicode_AsUTF8AndSize(t, &ssize);
  input_mbuf = Buffer::external(
                  const_cast<void*>(static_cast<const void*>(buf)),
                  static_cast<size_t>(ssize) + 1);
  sof = static_cast<char*>(input_mbuf.wptr());
  eof = sof + ssize + 1;
  subjob.done();
}



void GenericReader::report_columns_to_python() {
  size_t ncols = columns.size();

  if (columns_arg) {
    py::olist colDescriptorList(ncols);
    for (size_t i = 0; i < ncols; i++) {
      colDescriptorList.set(i, columns[i].py_descriptor());
    }

    py::otuple newColumns =
      py::oobj::import("datatable.fread", "_override_columns")
        .call({columns_arg, colDescriptorList}).to_otuple();

    column_names = newColumns[0].to_pylist();
    py::olist newTypesList = newColumns[1].to_pylist();

    if (newTypesList) {
      for (size_t i = 0; i < ncols; i++) {
        py::robj elem = newTypesList[i];
        columns[i].set_rtype(elem.to_int64());
      }
    }
  }
}



dtptr GenericReader::makeDatatable() {
  size_t ncols = columns.size();
  size_t nrows = columns.get_nrows();
  size_t ocols = columns.nColumnsInOutput();
  std::vector<Column> ccols;
  ccols.reserve(ocols);
  for (size_t i = 0; i < ncols; ++i) {
    dt::read::Column& col = columns[i];
    if (!col.is_in_output()) continue;
    Buffer databuf = col.extract_databuf();
    Buffer strbuf = col.extract_strbuf();
    SType stype = col.get_stype();
    ccols.push_back((stype == SType::STR32 || stype == SType::STR64)
      ? Column::new_string_column(nrows, std::move(databuf), std::move(strbuf))
      : Column::new_mbuf_column(nrows, stype, std::move(databuf))
    );
  }
  if (column_names) {
    return dtptr(new DataTable(std::move(ccols), column_names));
  } else {
    return dtptr(new DataTable(std::move(ccols), columns.get_names()));
  }
}
