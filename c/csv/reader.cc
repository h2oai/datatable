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
#include <stdlib.h>
#include <cstring>   // std::memcmp
#include "utils/exceptions.h"
#include "utils/omp.h"



//------------------------------------------------------------------------------
// GenericReader initialization
//------------------------------------------------------------------------------

GenericReader::GenericReader(const PyObj& pyrdr) {
  mbuf = nullptr;
  offset = 0;
  offend = 0;
  freader = pyrdr;
  src_arg = pyrdr.attr("src");
  file_arg = pyrdr.attr("file");
  text_arg = pyrdr.attr("text");
  fileno = pyrdr.attr("fileno").as_int32();
  logger = pyrdr.attr("logger");

  set_verbose(pyrdr.attr("verbose").as_bool());
  set_nthreads(pyrdr.attr("nthreads").as_int32());
  set_fill(pyrdr.attr("fill").as_bool());
  sep = pyrdr.attr("sep").as_char();
  dec = pyrdr.attr("dec").as_char();
  quote = pyrdr.attr("quotechar").as_char();
  set_maxnrows(pyrdr.attr("max_nrows").as_int64());
  set_skiplines(pyrdr.attr("skip_lines").as_int64());
  header = pyrdr.attr("header").as_bool();
  strip_white = pyrdr.attr("strip_white").as_bool();
  skip_blank_lines = pyrdr.attr("skip_blank_lines").as_bool();
  show_progress = pyrdr.attr("show_progress").as_bool();
  skipstring_arg = pyrdr.attr("skip_to_string");
  skip_string = skipstring_arg.as_cstring();
  na_strings = pyrdr.attr("na_strings").as_cstringlist();
  warnings_to_errors = 0;
}

GenericReader::~GenericReader() {
  if (mbuf) mbuf->release();
}


void GenericReader::set_nthreads(int32_t nth) {
  nthreads = nth;
  int32_t maxth = omp_get_max_threads();
  if (nthreads > maxth) nthreads = maxth;
  if (nthreads <= 0) nthreads += maxth;
  if (nthreads <= 0) nthreads = 1;
  trace("Using %d threads (requested=%d, max.available=%d)",
        nthreads, nth, maxth);
}

void GenericReader::set_verbose(int8_t v) {
  verbose = (v > 0);
}

void GenericReader::set_fill(int8_t v) {
  fill = (v > 0);
  if (fill) trace("fill=True (incomplete lines will be filled with NAs)");
}

void GenericReader::set_maxnrows(int64_t n) {
  max_nrows = (n < 0)? LONG_MAX : n;
}

void GenericReader::set_skiplines(int64_t n) {
  skip_lines = (n < 0)? 0 : n;
}



//------------------------------------------------------------------------------
// Main read() function
//------------------------------------------------------------------------------

DataTablePtr GenericReader::read()
{
  open_input();
  detect_and_skip_bom();
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

template <typename TThreadContext> class ChunkedDataReader
{
  // the data is read from here:
  const char* inputptr;
  size_t inputsize;
  size_t inputline;
  // and saved into here:
  std::vector<OutputColumn> outcols;
  // via the intermediate buffers TThreadContext, that are instantiated within
  // the read_all() method.

  // Extra parameters:
  std::vector<ColumnSpec> colspec;
  size_t chunksize;
  size_t nchunks;
  int nthreads;
  bool contiguous_chunks;
  int : 24;

public:
  ChunkedDataReader();
  virtual ~ChunkedDataReader();
  virtual void compute_chunking_strategy();
  virtual void adjust_chunk_start(const char**);
  virtual void read_all();
  virtual void push_buffers(TThreadContext&);
  virtual const char* read_chunk(const char* start, const char* end,
                                 TThreadContext&);

  void set_input(const char* ptr, size_t size, size_t line);
};




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

template <typename TThreadContext>
void ChunkedDataReader<TThreadContext>::read_all()
{
  chunksize = inputsize / nchunks;
  const char* prev_chunkend = inputptr;
  bool stopTeam = false;

  #pragma omp parallel num_threads(nthreads)
  {
    int ithread = omp_get_thread_num();
    TThreadContext ctx(ithread);
    ctx.prepare_strbufs(colspec);

    #pragma omp for ordered schedule(dynamic)
    for (int ichunk = 0; ichunk < nchunks; ++ichunk) {
      if (stopTeam) continue;
      push_buffers(ctx);
      const char* chunkstart = inputptr + ichunk * chunksize;
      const char* chunkend = chunkstart + chunksize;
      if (ichunk == nchunks - 1) chunkend = inputptr + inputsize;
      if (ichunk > 0) adjust_chunk_start(&chunkstart);

      const char* end = read_chunk(chunkstart, chunkend, ctx);
      // ctx.preorder();

      #pragma omp ordered
      {
        if (chunkstart != prev_chunkend && contiguous_chunks) {
          printf("Previous chunk did not finish at the same place: %p vs %p...\n", prev_chunkend, chunkstart);
          // Re-read the chunk
          ctx.discard();
          end = read_chunk(prev_chunkend, chunkend, ctx);
        }
        prev_chunkend = end;
        ctx.order();
      }
    }
    ctx.push_buffers();
  }
}


template <typename TThreadContext>
void ChunkedDataReader<TThreadContext>::push_buffers(TThreadContext& ctx)
{
  if (ctx.used_nrows == 0) return;
  size_t rowsize8 = ctx.rowsize / 8;
  for (size_t i = 0, j = 0, k = 0; i < colspec.size(); ++i) {
    auto coltype = colspec[i].type;
    if (coltype == ColumnSpec::Type::Drop) {
      continue;
    }
    if (coltype == ColumnSpec::Type::String) {
      StrBuf2& sb = ctx.strbufs[k];
      outcols[j].strdata->write_at(sb.writepos, sb.usedsize, sb.strdata);
      sb.usedsize = 0;

      int32_t* dest = static_cast<int32_t*>(outcols[j].data);
      RelStr* src = static_cast<RelStr*>(ctx.wbuf);
      int32_t offset = abs(dest[-1]);
      for (int64_t row = 0; row < ctx.used_nrows; ++row) {
        int32_t o = src->offset;
        dest[row] = o >= 0? o + offset : o - offset;
        src += rowsize8;
      }
      k++;
    } else {
      // ... 3 cases, depending on colsize
    }
    j++;
  }
  ctx.used_nrows = 0;
}


// Default implementation merely moves the pointer to the beginning of the
// next line.
template <typename TThreadContext>
void ChunkedDataReader<TThreadContext>::adjust_chunk_start(const char** pch) {
  const char* ch = *pch;
  while (*ch) {
    if (*ch == '\r' || *ch == '\n') {
      ch += 1 + (ch[0] + ch[1] == '\r' + '\n');
      break;
    }
    ch++;
  }
  *pch = ch;
}


template <typename TThreadContext>
void ChunkedDataReader<TThreadContext>::set_input(
    const char* input, size_t size, size_t line
) {
  inputptr = input;
  inputsize = size;
  inputline = line;
}


//------------------------------------------------------------------------------
// ThreadContext
//------------------------------------------------------------------------------

ThreadContext::ThreadContext(int ith, size_t nrows, size_t ncols) {
  ithread = ith;
  rowsize = 8 * ncols;
  wbuf_nrows = nrows;
  wbuf = malloc(rowsize * wbuf_nrows);
  used_nrows = 0;
}


ThreadContext::~ThreadContext() {
  assert(used_nrows == 0);
  if (wbuf) free(wbuf);
}


void ThreadContext::prepare_strbufs(const std::vector<ColumnSpec>& columns) {
  size_t ncols = columns.size();
  for (size_t i = 0; i < ncols; ++i) {
    if (columns[i].type == ColumnSpec::Type::String) {
      strbufs.push_back(StrBuf2(static_cast<int64_t>(i)));
    }
  }
}


void* ThreadContext::next_row() {
  if (used_nrows == wbuf_nrows) {
    wbuf_nrows += (wbuf_nrows + 1) / 2;
    wbuf = realloc(wbuf, wbuf_nrows * rowsize);
    if (!wbuf) {
      throw RuntimeError() << "Unable to allocate " << wbuf_nrows * rowsize
                           << " bytes for the temporary buffers";
    }
  }
  return static_cast<void*>(static_cast<char*>(wbuf) + (used_nrows++) * rowsize);
}


void ThreadContext::discard() {
  used_nrows = 0;
}
