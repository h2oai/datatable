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
#include "options.h"
#include "utils/exceptions.h"
#include "utils/omp.h"
#include "python/long.h"
#include "python/string.h"


//------------------------------------------------------------------------------
// GenericReader initialization
//------------------------------------------------------------------------------

GenericReader::GenericReader(const PyObj& pyrdr) {
  input_mbuf = nullptr;
  sof = nullptr;
  eof = nullptr;
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
  init_overridecolumntypes();
}

// Copy-constructor will copy only the essential parts
GenericReader::GenericReader(const GenericReader& g) {
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
  override_column_types = g.override_column_types;
  // Runtime parameters
  input_mbuf    = g.input_mbuf? g.input_mbuf->shallowcopy() : nullptr;
  sof     = g.sof;
  eof     = g.eof;
  line    = g.line;
  logger  = g.logger;   // for verbose messages / warnings
  freader = g.freader;  // for progress function / override columns
}

GenericReader::~GenericReader() {
  if (input_mbuf) input_mbuf->release();
}


void GenericReader::init_verbose() {
  int8_t v = freader.attr("verbose").as_bool();
  verbose = (v > 0);
}

void GenericReader::init_nthreads() {
  int32_t nth = freader.attr("nthreads").as_int32();
  if (ISNA<int32_t>(nth)) {
    nthreads = config::nthreads;
    trace("Using default %d threads", nthreads);
  } else {
    nthreads = nth;
    int32_t maxth = omp_get_max_threads();
    if (nthreads > maxth) nthreads = maxth;
    if (nthreads <= 0) nthreads += maxth;
    if (nthreads <= 0) nthreads = 1;
    trace("Using %d threads (requested=%d, max.available=%d)",
          nthreads, nth, maxth);
  }
}

void GenericReader::init_fill() {
  int8_t v = freader.attr("fill").as_bool();
  fill = (v > 0);
  if (fill) trace("fill=True (incomplete lines will be padded with NAs)");
}

void GenericReader::init_maxnrows() {
  int64_t n = freader.attr("max_nrows").as_int64();
  if (n < 0) {
    max_nrows = std::numeric_limits<size_t>::max();
  } else {
    max_nrows = static_cast<size_t>(n);
    trace("max_nrows = %lld", static_cast<long long>(n));
  }
}

void GenericReader::init_skiptoline() {
  int64_t n = freader.attr("skip_to_line").as_int64();
  skip_to_line = (n < 0)? 0 : static_cast<size_t>(n);
  if (n > 1) trace("skip_to_line = %zu", n);
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
  report_progress = freader.attr("show_progress").as_bool();
  if (report_progress) trace("show_progress = True");
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
  skip_to_string = skipstring_arg.as_cstring();
  if (skip_to_string && skip_to_string[0]=='\0') skip_to_string = nullptr;
  if (skip_to_string && skip_to_line) {
    throw ValueError() << "Parameters `skip_to_line` and `skip_to_string` "
                       << "cannot be provided simultaneously";
  }
  if (skip_to_string) trace("skip_to_string = \"%s\"", skip_to_string);
}

void GenericReader::init_stripwhite() {
  strip_whitespace = freader.attr("strip_whitespace").as_bool();
  trace("strip_whitespace = %s", strip_whitespace? "True" : "False");
}

void GenericReader::init_skipblanklines() {
  skip_blank_lines = freader.attr("skip_blank_lines").as_bool();
  trace("skip_blank_lines = %s", skip_blank_lines? "True" : "False");
}

void GenericReader::init_overridecolumntypes() {
  override_column_types = !freader.attr("_columns").is_none();
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

size_t GenericReader::datasize() const {
  return static_cast<size_t>(eof - sof);
}

bool GenericReader::extra_byte_accessible() const {
  return (eof < input_mbuf->getstr() + input_mbuf->size());
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
}

void GenericReader::progress(double progress, int statuscode) {
  freader.invoke("_progress", "(di)", progress, statuscode);
}



//------------------------------------------------------------------------------

void GenericReader::open_input() {
  size_t size = 0;
  const char* text = nullptr;
  const char* filename = nullptr;
  size_t extra_byte = 0;
  if (fileno > 0) {
    const char* src = src_arg.as_cstring();
    input_mbuf = new OvermapMemBuf(src, 1, fileno);
    size_t sz = input_mbuf->size();
    if (sz > 0) {
      sz--;
      *(input_mbuf->getstr() + sz) = '\0';
      extra_byte = 1;
    }
    trace("Using file %s opened at fd=%d; size = %zu", src, fileno, sz);

  } else if ((text = text_arg.as_cstring(&size))) {
    input_mbuf = new ExternalMemBuf(text, size + 1);
    extra_byte = 1;

  } else if ((filename = file_arg.as_cstring())) {
    input_mbuf = new OvermapMemBuf(filename, 1);
    size_t sz = input_mbuf->size();
    if (sz > 0) {
      sz--;
      *(input_mbuf->getstr() + sz) = '\0';
      extra_byte = 1;
    }
    trace("File \"%s\" opened, size: %zu", filename, sz);

  } else {
    throw RuntimeError() << "No input given to the GenericReader";
  }
  line = 1;
  sof = input_mbuf->getstr();
  eof = sof + input_mbuf->size() - extra_byte;
  if (eof) xassert(*eof == '\0');
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


DataTablePtr GenericReader::read_empty_input() {
  size_t size = datasize();
  if (size == 0 || (size == 1 && *sof == '\0')) {
    trace("Input is empty, returning a (0 x 0) DataTable");
    Column** cols = static_cast<Column**>(malloc(sizeof(Column*)));
    cols[0] = nullptr;
    return DataTablePtr(new DataTable(cols));
  }
  return nullptr;
}


/**
 * This method will attempt to check whether the file looks like it is one
 * of the unsupported formats (such as HTML). If so, an exception will be
 * thrown.
 */
void GenericReader::detect_improper_files() {
  const char* ch = sof;  // dataptr();
  while (ch < eof && (*ch==' ' || *ch=='\t')) ch++;
  if (std::memcmp(ch, "<!DOCTYPE html>", 15) == 0) {
    throw RuntimeError() << src_arg.as_cstring() << " is an HTML file. Please "
        << "open it in a browser and then save in a plain text format.";
  }
}


void GenericReader::decode_utf16() {
  const char* ch = sof;
  size_t size = datasize();
  if (!size) return;

  Py_ssize_t ssize = static_cast<Py_ssize_t>(size);
  int byteorder = 0;
  tempstr = PyObj(PyUnicode_DecodeUTF16(ch, ssize, "replace", &byteorder));
  PyObject* t = tempstr.as_pyobject();  // new ref
  // borrowed ref, belongs to PyObject `t`
  const char* buf = PyUnicode_AsUTF8AndSize(t, &ssize);
  input_mbuf->release();
  input_mbuf = new ExternalMemBuf(buf, static_cast<size_t>(ssize) + 1);
  sof = input_mbuf->getstr();
  eof = sof + ssize + 1;
  // the object `t` remains alive within `tempstr`
  Py_DECREF(t);
}



void GenericReader::report_columns_to_python() {
  size_t ncols = columns.size();

  if (override_column_types) {
    PyyList colDescriptorList(ncols);
    for (size_t i = 0; i < ncols; i++) {
      colDescriptorList[i] = columns[i].py_descriptor();
    }

    PyyList newTypesList =
      freader.invoke("_override_columns0", "(O)",
                     colDescriptorList.release());

    if (newTypesList) {
      for (size_t i = 0; i < ncols; i++) {
        PyObj elem = newTypesList[i];
        columns[i].rtype = static_cast<RT>(elem.as_int64());  // unsafe?
        // Temporary
        switch (columns[i].rtype) {
          case RDrop:    columns[i].type = PT::Str32; break;
          case RAuto:    break;
          case RBool:    columns[i].type = PT::Bool01; break;
          case RInt:     columns[i].type = PT::Int32; break;
          case RInt32:   columns[i].type = PT::Int32; break;
          case RInt64:   columns[i].type = PT::Int64; break;
          case RFloat:   columns[i].type = PT::Float32Hex; break;
          case RFloat32: columns[i].type = PT::Float32Hex; break;
          case RFloat64: columns[i].type = PT::Float64Plain; break;
          case RStr:     columns[i].type = PT::Str32; break;
          case RStr32:   columns[i].type = PT::Str32; break;
          case RStr64:   columns[i].type = PT::Str64; break;
        }
      }
    }

  } else {
    PyyList colNamesList(ncols);
    for (size_t i = 0; i < ncols; ++i) {
      colNamesList[i] = PyyString(columns[i].name);
    }
    freader.invoke("_set_column_names", "(O)", colNamesList.release());
  }
}



DataTablePtr GenericReader::makeDatatable() {
  Column** ccols = NULL;
  size_t ncols = columns.size();
  size_t ocols = columns.nColumnsInOutput();
  ccols = (Column**) malloc((ocols + 1) * sizeof(Column*));
  ccols[ocols] = NULL;
  for (size_t i = 0, j = 0; i < ncols; ++i) {
    GReaderColumn& col = columns[i];
    if (!col.presentInOutput) continue;
    SType stype = ParserLibrary::info(col.type).stype;
    MemoryBuffer* databuf = col.extract_databuf();
    MemoryBuffer* strbuf = col.extract_strbuf();
    ccols[j] = Column::new_mbuf_column(stype, databuf, strbuf);
    j++;
  }
  return DataTablePtr(new DataTable(ccols));
}
