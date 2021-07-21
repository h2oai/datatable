//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include <stdlib.h>             // strtod
#include <cerrno>               // errno
#include <cstring>              // std::memcmp
#include "csv/reader.h"
#include "csv/reader_arff.h"
#include "csv/reader_fread.h"
#include "datatable.h"
#include "encodings.h"
#include "frame/py_frame.h"
#include "options.h"
#include "parallel/api.h"
#include "python/_all.h"
#include "python/string.h"
#include "read/parsers/rt.h"
#include "stype.h"
#include "utils/exceptions.h"
#include "utils/misc.h"         // wallclock
#include "utils/macros.h"
namespace dt {
namespace read {

#define D() if (verbose) logger_.info()


//------------------------------------------------------------------------------
// options
//------------------------------------------------------------------------------

static const char * doc_options_fread_log_anonymize =
R"(

This option controls logs anonymization that is useful in production
systems, when reading sensitive data that must not accidentally leak
into log files or be printed with the error messages.

Parameters
----------
return: bool
    Current `anonymize` value. Initially, this option is set to `False`.

new_anonymize: bool
    New `anonymize` value. If `True`, any snippets of data being read
    that are printed in the log will be first anonymized by converting
    all non-zero digits to `1`, all lowercase letters to `a`,
    all uppercase letters to `A`, and all unicode characters to `U`.
    If `False`, no data anonymization will be performed.

)";

static const char * doc_options_fread_log_escape_unicode =
R"(

This option controls escaping of the unicode characters.

Use this option if your terminal cannot print unicode,
or if the output gets somehow corrupted because of the unicode characters.

Parameters
----------
return: bool
    Current `escape_unicode` value. Initially, this option is set to `False`.

new_escape_unicode: bool
    If `True`, all unicode characters in the verbose log will be written
    in hexadecimal notation. If `False`, no escaping of the unicode
    characters will be performed.

)";

static const char * doc_options_fread_parse_dates =
R"(
If True, fread will attempt to detect columns of date32 type. If False,
then columns with date values will be returned as strings.

This option is temporary and will be removed in the future.
)";

static const char * doc_options_fread_parse_times =
R"(
If True, fread will attempt to detect columns of time64 type. If False,
then columns with timestamps will be returned as strings.

This option is temporary and will be removed in the future.
)";

static bool log_anonymize = false;
static bool log_escape_unicode = false;
bool parse_dates = true;
bool parse_times = true;


static py::oobj get_anonymize() {
  return py::obool(log_anonymize);
}

static void set_anonymize(const py::Arg& arg) {
  log_anonymize = arg.to_bool_strict();
}


static py::oobj get_escape_unicode() {
  return py::obool(log_escape_unicode);
}

static void set_escape_unicode(const py::Arg& arg) {
  log_escape_unicode = arg.to_bool_strict();
}


void GenericReader::init_options() {
  dt::register_option(
    "fread.anonymize",
    []{ return py::obool(log_anonymize); },
    [](const py::Arg& value){ log_anonymize = value.to_bool_strict(); },
    "[DEPRECATED] same as fread.log.anonymize");

  dt::register_option(
    "fread.log.anonymize",
    get_anonymize,
    set_anonymize,
    doc_options_fread_log_anonymize
  );

  dt::register_option(
    "fread.log.escape_unicode",
    get_escape_unicode,
    set_escape_unicode,
    doc_options_fread_log_escape_unicode
  );

  dt::register_option(
    "fread.parse_dates",
    []{ return py::obool(parse_dates); },
    [](const py::Arg& value){ parse_dates = value.to_bool_strict(); },
    doc_options_fread_parse_dates
  );

  dt::register_option(
    "fread.parse_times",
    []{ return py::obool(parse_times); },
    [](const py::Arg& value){ parse_times = value.to_bool_strict(); },
    doc_options_fread_parse_times
  );
}




//------------------------------------------------------------------------------
// GenericReader initialization
//------------------------------------------------------------------------------

GenericReader::GenericReader()
  : preframe(this)
{
  source_name = nullptr;
  na_strings = nullptr;
  sof = nullptr;
  eof = nullptr;
  line = 0;
  cr_is_newline = 0;
  multisource_strategy = FreadMultiSourceStrategy::Warn;
  errors_strategy = IreadErrorHandlingStrategy::Error;
  memory_limit = size_t(-1);
}


// Copy-constructor will copy only the essential parts
GenericReader::GenericReader(const GenericReader& g)
  : preframe(this)
{
  // Input parameters
  nthreads         = g.nthreads;
  verbose          = g.verbose;
  sep              = g.sep;
  dec              = g.dec;
  quote            = g.quote;
  max_nrows        = g.max_nrows;
  multisource_strategy = g.multisource_strategy;
  errors_strategy  = g.errors_strategy;
  skip_to_line     = g.skip_to_line;
  na_strings       = g.na_strings;
  header           = g.header;
  strip_whitespace = g.strip_whitespace;
  skip_blank_lines = g.skip_blank_lines;
  skip_to_string   = g.skip_to_string;
  skip_to_line     = g.skip_to_line;
  fill             = g.fill;
  blank_is_na      = g.blank_is_na;
  number_is_na     = g.number_is_na;
  columns_arg      = g.columns_arg;
  t_open_input     = g.t_open_input;
  memory_limit     = g.memory_limit;
  encoding_        = g.encoding_;
  // Runtime parameters
  job     = g.job;
  input_mbuf = g.input_mbuf;
  sof     = g.sof;
  eof     = g.eof;
  line    = g.line;
  logger_ = g.logger_;
  source_name = g.source_name;
  tempfiles = g.tempfiles;
}

GenericReader::~GenericReader() {}


void GenericReader::init_nthreads(const py::Arg& arg) {
  int32_t DEFAULT = -(1 << 30);
  int32_t nth = arg.to<int32_t>(DEFAULT);
  int maxth = static_cast<int>(dt::num_threads_in_pool());
  if (nth == DEFAULT) {
    nthreads = maxth;
    D() << "Using default " << nthreads << " thread(s)";
  } else {
    nthreads = nth;
    if (nthreads > maxth) nthreads = maxth;
    if (nthreads <= 0) nthreads += maxth;
    if (nthreads <= 0) nthreads = 1;
    D() << "Using " << nthreads << " thread(s) "
          "(requested=" << nth << ", max.available=" << maxth << ")";
  }
}


void GenericReader::init_encoding(const py::Arg& arg) {
  if (arg.is_none_or_undefined()) return;
  encoding_ = arg.to_string();
  if (!PyCodec_KnownEncoding(encoding_.c_str())) {
    throw ValueError() << "Unknown encoding " << encoding_;
  }
  D() << "encoding = '" << encoding_ << "'";
}


void GenericReader::init_fill(const py::Arg& arg) {
  fill = arg.to<bool>(false);
  if (fill) {
    D() << "fill = True (incomplete lines will be padded with NAs)";
  }
}

void GenericReader::init_maxnrows(const py::Arg& arg) {
  int64_t n = arg.to<int64_t>(-1);
  if (n < 0) {
    max_nrows = std::numeric_limits<size_t>::max();
  } else {
    max_nrows = static_cast<size_t>(n);
    D() << "max_nrows = " << max_nrows;
  }
}

void GenericReader::init_skiptoline(const py::Arg& arg) {
  int64_t n = arg.to<int64_t>(-1);
  skip_to_line = (n < 0)? 0 : static_cast<size_t>(n);
  if (n > 1) {
    D() << "skip_to_line = " << skip_to_line;
  }
}

void GenericReader::init_sep(const py::Arg& arg) {
  if (arg.is_none_or_undefined()) {
    sep = '\xFF';
    return;
  }
  auto str = arg.to_string();
  size_t size = str.size();
  const char c = size? str[0] : '\n';
  if (c == '\n' || c == '\r') {
    sep = '\n';
    D() << "sep = <single-column mode>";
  } else if (size > 1) {
    throw NotImplError() << "Multi-character or unicode separators are not "
                            "supported: '" << str << "'";
  } else {
    if (c=='"' || c=='\'' || c=='`' || ('0' <= c && c <= '9') ||
        ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
        c=='.' || c=='-' || c=='+') {
      throw ValueError() << "Separator `'" << c << "'` is not allowed";
    }
    sep = c;
    D() << "sep = '" << sep << "'";
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
    D() << "dec = " << dec;
  } else {
    throw ValueError() << "Only dec='.' or ',' are allowed";
  }
}

void GenericReader::init_quote(const py::Arg& arg) {
  if (arg.is_none_or_undefined()) {
    quote = '"';
    return;
  }
  auto str = arg.to_string();
  if (str.size() == 0) {
    quote = '\0';
  } else if (str.size() > 1) {
    throw ValueError() << "Multi-character quote is not allowed: '"
                       << str << "'";
  } else if (str[0] == '"' || str[0] == '\'' || str[0] == '`') {
    quote = str[0];
    if (quote == '\'') { D() << "quote = \"'\""; }
    else               { D() << "quote = '" << quote << "'"; }
  } else {
    throw ValueError() << "quotechar = (" << escape_backticks(str)
                       << ") is not allowed";
  }
}

void GenericReader::init_header(const py::Arg& arg) {
  if (arg.is_none_or_undefined()) {
    header = dt::GETNA<int8_t>();
  } else {
    header = arg.to_bool_strict();
    D() << "header = " << (header? "True" : "False");
  }
}

void GenericReader::init_multisource(const py::Arg& arg) {
  auto str = arg.to<std::string>("");
  if (str == "")            multisource_strategy = FreadMultiSourceStrategy::Warn;
  else if (str == "warn")   multisource_strategy = FreadMultiSourceStrategy::Warn;
  else if (str == "error")  multisource_strategy = FreadMultiSourceStrategy::Error;
  else if (str == "ignore") multisource_strategy = FreadMultiSourceStrategy::Ignore;
  else {
    throw ValueError() << arg.name() << " got invalid value " << str;
  }
}

void GenericReader::init_errors(const py::Arg& arg) {
  auto str = arg.to<std::string>("");
  if (str == "")            errors_strategy = IreadErrorHandlingStrategy::Warn;
  else if (str == "warn")   errors_strategy = IreadErrorHandlingStrategy::Warn;
  else if (str == "raise")  errors_strategy = IreadErrorHandlingStrategy::Error;
  else if (str == "ignore") errors_strategy = IreadErrorHandlingStrategy::Ignore;
  else if (str == "store")  errors_strategy = IreadErrorHandlingStrategy::Store;
  else {
    throw ValueError() << arg.name() << " got invalid value " << str;
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
      D() << "No na_strings provided";
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
      D() << out;
      if (number_is_na) D() << "  + some na strings look like numbers";
      if (blank_is_na)  D() << "  + empty string is considered an NA";
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
    D() << "skip_to_string = \"" << skip_to_string << "\"";
  }
}

void GenericReader::init_stripwhite(const py::Arg& arg) {
  strip_whitespace = arg.to<bool>(true);
  D() << "strip_whitespace = " << (strip_whitespace? "True" : "False");
}

void GenericReader::init_skipblanks(const py::Arg& arg) {
  skip_blank_lines = arg.to<bool>(false);
  D() << "skip_blank_lines = " << (skip_blank_lines? "True" : "False");
}

void GenericReader::init_tempdir(const py::Arg& arg_tempdir) {
  auto clsTempFiles = py::oobj::import("datatable.utils.fread", "TempFiles");
  auto tempdir = arg_tempdir.to_oobj_or_none();
  tempfiles = clsTempFiles.call({tempdir, logger_.get_pylogger()});
}

void GenericReader::init_columns(const py::Arg& arg) {
  if (arg.is_defined()) {
    columns_arg = arg.to_oobj();
  }
}

void GenericReader::init_logger(
        const py::Arg& arg_logger, const py::Arg& arg_verbose)
{
  verbose = arg_verbose.to<bool>(false);
  if (arg_logger.is_none_or_undefined()) {
    if (verbose) {
      logger_.enable();
    }
  } else {
    logger_.use_pylogger(arg_logger.to_oobj());
    verbose = true;
  }
}


void GenericReader::init_memorylimit(const py::Arg& arg) {
  constexpr size_t UNLIMITED = size_t(-1);
  memory_limit = arg.to<size_t>(UNLIMITED);
  if (memory_limit != UNLIMITED) {
    D() << "memory_limit = " << memory_limit << " bytes";
  }
}




//------------------------------------------------------------------------------
// Main read_all() function
//------------------------------------------------------------------------------

py::oobj GenericReader::read_buffer(const Buffer& buf, size_t extra_byte)
{
  {
    auto _ = logger_.section("[1] Prepare for reading");
    job = std::make_shared<dt::progress::work>(WORK_PREPARE + WORK_READ);
    open_buffer(buf, extra_byte);
    process_encoding();
    log_file_sample();
  }
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

  if (!output_) {
    throw IOError() << "Unable to read input " << *source_name;
  }

  job->done();
  return output_;
}


void GenericReader::log_file_sample() {
  if (!verbose) return;
  d() << "==== file sample ====";
  const char* ch = sof;
  bool newline = true;
  for (int i = 0; i < 5 && ch < eof; i++) {
    if (newline) d() << repr_source(ch, 100);
    else         d() << "..." << repr_source(ch, 97);
    const char* start = ch;
    const char* end = std::min(eof, ch + 10000);
    while (ch < end) {
      char c = *ch++;
      // simplified newline sequence. TODO: replace with `skip_eol()`
      if (c == '\n' || c == '\r') {
        if (ch < end && (*ch == '\r' || *ch == '\n') && *ch != c) ch++;
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
  d() << "=====================";
}


//------------------------------------------------------------------------------

py::oobj GenericReader::get_tempfiles() const {
  return tempfiles;
}

log::Message GenericReader::d() const {
  xassert(verbose);
  return logger_.info();
}


size_t GenericReader::datasize() const {
  return static_cast<size_t>(eof - sof);
}

bool GenericReader::extra_byte_accessible() const {
  const char* ptr = static_cast<const char*>(input_mbuf.rptr());
  return (eof < ptr + input_mbuf.size());
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

void GenericReader::open_buffer(const Buffer& buf, size_t extra_byte) {
  input_mbuf = buf;
  line = 1;
  sof = static_cast<const char*>(input_mbuf.rptr());
  eof = sof + input_mbuf.size() - extra_byte;

  if (eof && extra_byte) {
    xassert(*eof == '\0');
  }
}


void GenericReader::process_encoding() {
  if (encoding_.empty()) return;
  if (verbose) {
    D() << "Decoding input from " << encoding_;
  }
  job->add_work_amount(WORK_DECODE_UTF16);
  job->set_message("Decoding " + encoding_);
  dt::progress::subtask subjob(*job, WORK_DECODE_UTF16);

  const char* enc_errors = (encoding_ == "base64"? "strict" : "replace");

  auto decoder = py::oobj::from_new_reference(
      PyCodec_IncrementalDecoder(encoding_.c_str(), enc_errors));
  auto decode = decoder.get_attr("decode");

  auto wb = std::make_unique<MemoryWritableBuffer>(input_mbuf.size() * 6/5);
  constexpr size_t CHUNK_SIZE = 1024 * 1024;
  for (const char* ch = sof; ch < eof; ch += CHUNK_SIZE) {
    size_t chunk_size = std::min(static_cast<size_t>(eof - ch), CHUNK_SIZE);
    // TODO: use obytes class
    auto original_bytes =
      py::oobj::from_new_reference(
        PyBytes_FromStringAndSize(ch, static_cast<Py_ssize_t>(chunk_size)));
    auto is_final = py::obool(ch + chunk_size == eof);
    auto decoded_string = decode.call({original_bytes, is_final});
    wb->write(decoded_string.to_cstring());
  }
  wb->finalize();
  open_buffer(wb->get_mbuf(), 0);
  subjob.done();
}


/**
 * Check whether the input contains BOM (Byte Order Mark), and if so skip it
 * modifying `sof`. If BOM indicates UTF-16 file, then recode the file into
 * UTF-8 (we cannot read UTF-16 directly).
 *
 * See: https://en.wikipedia.org/wiki/Byte_order_mark
 */
void GenericReader::detect_and_skip_bom() {
  if (!encoding_.empty()) return;
  size_t sz = datasize();
  const char* ch = sof;
  if (!sz) return;
  if (sz >= 3 && ch[0]=='\xEF' && ch[1]=='\xBB' && ch[2]=='\xBF') {
    sof += 3;
    D() << "UTF-8 byte order mark EF BB BF found at the start of the file "
           "and skipped";
  } else
  if (sz >= 2 && ch[0] + ch[1] == '\xFE' + '\xFF') {
    D() << "UTF-16 byte order mark " << (ch[0]=='\xFE'? "FE FF" : "FF FE")
        << " found at the start of the file and skipped";
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
 * Suppose input is the following (_ is a space):
 *
 *     _ _ _ _ \\n _ \\t _ H e l l o …
 *
 * If strip_whitespace=true, then this function will move the `sof` to
 * character H; whereas if strip_whitespace=false, this function will move the
 * `sof` to the first space after '␤'.
 */
void GenericReader::skip_initial_whitespace() {
  const char* ch = sof;
  if (!sof) return;
  while ((ch < eof) && (*ch <= ' ')  &&
         (*ch==' ' || *ch=='\n' || *ch=='\r' ||
         (*ch=='\t' && (ch+1 < eof) && *(ch+1) != '\t' && *(ch+1) <= ' '))) {
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
    D() << "Skipped " << doffset << " initial whitespace character(s)";
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
      D() << "Skipped " << d << " trailing whitespace characters";
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
    D() << "Skipped to line " << line << " in the file";
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
          D() << "Skipped to line " << line
              << " containing skip_to_string = \"" << skip_to_string << "\"";
        }
        return;
      } else {
        ch++;
      }
    }
    if (ch < eof && (*ch=='\n' || *ch=='\r')) {
      ch += 1 + ((ch + 1 < eof) && *ch + ch[1] == '\n' + '\r');
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
    D() << "Input is empty, returning a (0 x 0) DataTable";
    job->add_done_amount(WORK_READ);
    output_ = py::Frame::oframe(new DataTable());
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
    throw IOError() << *source_name << " is an HTML file. Please "
        << "open it in a browser and then save in a plain text format.";
  }
  // --- detect Feather ---
  if (sof + 8 < eof && std::memcmp(sof, "FEA1", 4) == 0
                    && std::memcmp(eof - 4, "FEA1", 4) == 0) {
    throw IOError() << *source_name << " is a feather file, it "
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
    output_ = py::Frame::oframe(dt);
    return true;
  }
  return false;
}


bool GenericReader::read_csv() {
  auto dt = FreadReader(*this).read_all();
  if (dt) {
    output_ = py::Frame::oframe(dt.release());
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
  input_mbuf = Buffer::unsafe(
                  static_cast<const void*>(buf),
                  static_cast<size_t>(ssize) + 1);
  sof = static_cast<char*>(input_mbuf.wptr());
  eof = sof + ssize + 1;
  subjob.done();
}



void GenericReader::report_columns_to_python() {
  size_t ncols = preframe.ncols();

  if (columns_arg) {
    py::olist colDescriptorList(ncols);
    size_t i = 0, j = 0;
    for (const auto& col : preframe) {
      colDescriptorList.set(i++, col.py_descriptor());
    }

    py::otuple newColumns =
      py::oobj::import("datatable.utils.fread", "_override_columns")
        .call({columns_arg, colDescriptorList}).to_otuple();

    py::olist newNamesList = newColumns[0].to_pylist();
    py::olist newTypesList = newColumns[1].to_pylist();
    if (newTypesList) {
      XAssert(newTypesList.size() == ncols);
      for (i = 0, j = 0; i < ncols; i++) {
        auto& coli = preframe.column(i);
        py::robj elem = newTypesList[i];
        coli.set_rtype(elem.to_int64());
        coli.outcol().set_stype(coli.get_stype());
        if (newNamesList && coli.get_rtype() != RT::RDrop) {
          XAssert(j < newNamesList.size());
          elem = newNamesList[j++];
          coli.set_name(elem.to_string());
        }
      }
    }
  }
}




}}  // namespace dt::read
