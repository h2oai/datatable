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
#include "csv/reader_fread.h"
#include "csv/reader.h"
#include "csv/fread.h"
#include <Python.h>
#include <string.h>    // memcpy
#include <sys/mman.h>  // mmap
#include <exception>
#include "memorybuf.h"
#include "datatable.h"
#include "column.h"
#include "py_datatable.h"
#include "py_encodings.h"
#include "py_utils.h"
#include "utils/assert.h"
#include "utils/file.h"
#include "utils/pyobj.h"
#include "utils.h"

static const SType colType_to_stype[NUMTYPE] = {
  ST_VOID,
  ST_BOOLEAN_I1,
  ST_BOOLEAN_I1,
  ST_BOOLEAN_I1,
  ST_BOOLEAN_I1,
  ST_INTEGER_I4,
  ST_INTEGER_I8,
  ST_REAL_F4,
  ST_REAL_F8,
  ST_REAL_F8,
  ST_REAL_F8,
  ST_STRING_I4_VCHAR,
};

// For temporary printing file names.
static char fname[1000];



//------------------------------------------------------------------------------

FreadReader::FreadReader(GenericReader& greader) : g(greader) {
  targetdir = nullptr;
  strbufs = nullptr;
  colNames = nullptr;
  lineCopy = nullptr;
  types = nullptr;
  sizes = nullptr;
  tmpTypes = nullptr;
  eof = nullptr;
  ncols = 0;
  nstrcols = 0;
  ndigits = 0;
  whiteChar = dec = sep = quote = '\0';
  quoteRule = -1;
  stripWhite = false;
  blank_is_a_NAstring = false;
  LFpresent = false;
}

FreadReader::~FreadReader() {
  free(strbufs);
  free(colNames);
  free(lineCopy);
  free(types);
  free(sizes);
  free(tmpTypes);
}


FieldParseContext FreadReader::makeFieldParseContext(
    const char*& ch, field64* target, const char* anchor
) {
  return {
    .ch = ch,
    .target = target,
    .anchor = anchor,
    .eof = eof,
    .NAstrings = g.na_strings,
    .whiteChar = whiteChar,
    .dec = dec,
    .sep = sep,
    .quote = quote,
    .quoteRule = quoteRule,
    .stripWhite = stripWhite,
    .blank_is_a_NAstring = blank_is_a_NAstring,
    .LFpresent = LFpresent,
  };
}


DataTablePtr FreadReader::read() {
  int retval = freadMain();
  if (!retval) throw PyError();
  return std::move(dt);
}



//------------------------------------------------------------------------------

Column* FreadReader::alloc_column(SType stype, size_t nrows, int j)
{
  // TODO(pasha): figure out how to use `WritableBuffer`s here
  Column *col = NULL;
  if (targetdir) {
    snprintf(fname, 1000, "%s/col%0*d", targetdir, ndigits, j);
    col = Column::new_mmap_column(stype, static_cast<int64_t>(nrows), fname);
  } else{
    col = Column::new_data_column(stype, static_cast<int64_t>(nrows));
  }
  if (col == NULL) return NULL;

  if (stype_info[stype].ltype == LT_STRING) {
    dtrealloc(strbufs[j], StrBuf, 1);
    StrBuf* sb = strbufs[j];
    // Pre-allocate enough memory to hold 5-char strings in the buffer. If
    // this is not enough, we will always be able to re-allocate during the
    // run time.
    size_t alloc_size = nrows * 5;
    sb->mbuf = static_cast<StringColumn<int32_t>*>(col)->strbuf;
    sb->mbuf->resize(alloc_size);
    sb->ptr = 0;
    sb->idx8 = -1;  // not needed for this structure
    sb->idxdt = j;
    sb->numuses = 0;
  }
  return col;
}


Column* FreadReader::realloc_column(Column *col, SType stype, size_t nrows, int j)
{
  if (col != NULL && stype != col->stype()) {
    delete col;
    return alloc_column(stype, nrows, j);
  }
  if (col == NULL) {
    return alloc_column(stype, nrows, j);
  }

  // Buffer in string's column has nrows + 1 elements
  size_t xnrows = nrows + (stype_info[stype].ltype == LT_STRING);
  size_t new_alloc_size = stype_info[stype].elemsize * xnrows;
  col->mbuf->resize(new_alloc_size);
  col->nrows = (int64_t) nrows;
  return col;
}



void FreadReader::userOverride(int8_t *types_, const char *anchor, int ncols_)
{
  types = types_;
  PyObject *colNamesList = PyList_New(ncols_);
  PyObject *colTypesList = PyList_New(ncols_);
  uint8_t echar = quoteRule == 0? static_cast<uint8_t>(quote) :
                  quoteRule == 1? '\\' : 0xFF;
  for (int i = 0; i < ncols_; i++) {
    RelStr ocol = colNames[i];
    PyObject* pycol = NULL;
    if (ocol.length > 0) {
      const char* src = anchor + ocol.offset;
      const uint8_t* usrc = reinterpret_cast<const uint8_t*>(src);
      size_t zlen = static_cast<size_t>(ocol.length);
      int res = check_escaped_string(usrc, zlen, echar);
      if (res == 0) {
        pycol = PyUnicode_FromStringAndSize(src, ocol.length);
      } else {
        char* newsrc = new char[zlen * 4];
        uint8_t* unewsrc = reinterpret_cast<uint8_t*>(newsrc);
        int newlen;
        if (res == 1) {
          newlen = decode_escaped_csv_string(usrc, ocol.length, unewsrc, echar);
        } else {
          newlen = decode_win1252(usrc, ocol.length, unewsrc);
          newlen = decode_escaped_csv_string(unewsrc, newlen, unewsrc, echar);
        }
        assert(newlen > 0);
        pycol = PyUnicode_FromStringAndSize(newsrc, newlen);
        delete[] newsrc;
      }
    } else {
      pycol = PyUnicode_FromFormat("V%d", i);
    }
    PyObject *pytype = PyLong_FromLong(types[i]);
    PyList_SET_ITEM(colNamesList, i, pycol);
    PyList_SET_ITEM(colTypesList, i, pytype);
  }

  g.pyreader().invoke("_override_columns", "(OO)", colNamesList, colTypesList);

  for (int i = 0; i < ncols_; i++) {
    PyObject *t = PyList_GET_ITEM(colTypesList, i);
    types[i] = (int8_t) PyLong_AsUnsignedLongMask(t);
  }

  pyfree(colTypesList);
  pyfree(colNamesList);
}


/**
 * Allocate memory for the DataTable that is being constructed.
 */
size_t FreadReader::allocateDT(int ncols_, int ndrop_, size_t nrows)
{
  Column** columns = NULL;
  nstrcols = 0;

  // First we need to estimate the size of the dataset that needs to be
  // created. However this needs to be done on first run only.
  // Also in this block we compute: `nstrcols` (will be used later in
  // `prepareLocalParseContext` and `postprocessBuffer`), as well as allocating
  // the `Column**` array.
  if (ncols == 0) {
    // DTPRINT("Writing the DataTable into %s", targetdir);
    assert(!dt);
    ncols = ncols_;

    size_t alloc_size = 0;
    int i, j;
    for (i = j = 0; i < ncols; i++) {
      if (types[i] == CT_DROP) continue;
      nstrcols += (types[i] == CT_STRING);
      SType stype = colType_to_stype[types[i]];
      alloc_size += stype_info[stype].elemsize * nrows;
      if (types[i] == CT_STRING) alloc_size += 5 * nrows;
      j++;
    }
    assert(j == ncols_ - ndrop_);
    dtcalloc_g(columns, Column*, j + 1);
    dtcalloc_g(strbufs, StrBuf*, j);
    columns[j] = NULL;

    // Call the Python upstream to determine the strategy where the
    // DataTable should be created.
    targetdir = g.pyreader().invoke("_get_destination", "(n)", alloc_size)
                 .as_ccstring();
  } else {
    assert(dt && ncols == ncols_);
    columns = dt->columns;
    for (int i = 0; i < ncols; i++)
      nstrcols += (types[i] == CT_STRING);
  }

  // Compute number of digits in `ncols` (needed for creating file names).
  if (targetdir) {
    ndigits = 0;
    for (int nc = ncols; nc; nc /= 10) ndigits++;
  }

  // Create individual columns
  for (int i = 0, j = 0; i < ncols_; i++) {
    int8_t type = types[i];
    if (type == CT_DROP) continue;
    if (type > 0) {
      SType stype = colType_to_stype[type];
      columns[j] = realloc_column(columns[j], stype, nrows, j);
      if (columns[j] == NULL) goto fail;
    }
    j++;
  }

  if (!dt) {
    dt.reset(new DataTable(columns));
  }
  return 1;

  fail:
  if (columns) {
    Column **col = columns;
    while (*col++) delete (*col);
    dtfree(columns);
  }
  throw RuntimeError() << "Unable to allocate DataTable";
}



void FreadReader::setFinalNrow(size_t nrows) {
  int i, j;
  for (i = j = 0; i < ncols; i++) {
    int type = types[i];
    if (type == CT_DROP) continue;
    Column *col = dt->columns[j];
    if (type == CT_STRING) {
      StrBuf* sb = strbufs[j];
      assert(sb->numuses == 0);
      sb->mbuf->resize(sb->ptr);
      sb->mbuf = nullptr; // MemoryBuffer is also pointed to by the column
      col->mbuf->resize(sizeof(int32_t) * (nrows + 1));
      col->nrows = static_cast<int64_t>(nrows);
    } else if (type > 0) {
      Column *c = realloc_column(col, colType_to_stype[type], nrows, j);
      if (c == nullptr) throw Error() << "Could not reallocate column";
    }
    j++;
  }
  dt->nrows = (int64_t) nrows;
}


void FreadReader::prepareLocalParseContext(FreadLocalParseContext *ctx)
{
  try {
    ctx->nstrcols = nstrcols;
    ctx->strbufs = new StrBuf[nstrcols]();
    for (int i = 0, j = 0, k = 0, off8 = 0; i < (int)ncols; i++) {
      if (types[i] == CT_DROP) continue;
      if (types[i] == CT_STRING) {
        assert(k < nstrcols);
        ctx->strbufs[k].mbuf = new MemoryMemBuf(4096);
        ctx->strbufs[k].ptr = 0;
        ctx->strbufs[k].idx8 = off8;
        ctx->strbufs[k].idxdt = j;
        k++;
      }
      off8 += (sizes[i] > 0);
      j++;
    }
    return;

  } catch (std::exception&) {
    printf("prepareLocalParseContext() failed\n");
    for (int k = 0; k < nstrcols; k++) {
      if (ctx->strbufs[k].mbuf)
        ctx->strbufs[k].mbuf->release();
    }
    dtfree(ctx->strbufs);
    *(ctx->stopTeam) = 1;
  }
}


void FreadReader::postprocessBuffer(FreadLocalParseContext* ctx)
{
  try {
    StrBuf* ctx_strbufs = ctx->strbufs;
    const uint8_t *anchor = (const uint8_t*) ctx->anchor;
    size_t nrows = ctx->used_nrows;
    RelStr* __restrict__ const lenoffs = (RelStr *__restrict__) ctx->obuf;
    int colCount = (int) ctx->obuf_ncols;
    uint8_t echar = ctx->quoteRule == 0? static_cast<uint8_t>(ctx->quote) :
                    ctx->quoteRule == 1? '\\' : 0xFF;

    for (int k = 0; k < nstrcols; k++) {
      assert(ctx_strbufs != NULL);
      RelStr *__restrict__ lo = lenoffs + ctx_strbufs[k].idx8;
      MemoryBuffer* strdest = ctx_strbufs[k].mbuf;
      int32_t off = 1;
      size_t bufsize = ctx_strbufs[k].mbuf->size();
      for (size_t n = 0; n < nrows; n++) {
        int32_t len = lo->length;
        if (len > 0) {
          size_t zlen = (size_t) len;
          if (bufsize < zlen * 3 + (size_t) off) {
            bufsize = bufsize * 2 + zlen * 3;
            strdest->resize(bufsize);
          }
          const uint8_t* src = anchor + lo->offset;
          uint8_t* dest = static_cast<uint8_t*>(strdest->at(off - 1));
          int res = check_escaped_string(src, zlen, echar);
          if (res == 0) {
            memcpy(dest, src, zlen);
            off += zlen;
            lo->offset = off;
          } else if (res == 1) {
            int newlen = decode_escaped_csv_string(src, len, dest, echar);
            off += (size_t) newlen;
            lo->offset = off;
          } else {
            int newlen = decode_win1252(src, len, dest);
            assert(newlen > 0);
            newlen = decode_escaped_csv_string(dest, newlen, dest, echar);
            off += (size_t) newlen;
            lo->offset = off;
          }
        } else if (len == 0) {
          lo->offset = off;
        } else {
          assert(len == NA_LENOFF);
          lo->offset = -off;
        }
        lo += colCount;
      }
      ctx_strbufs[k].ptr = (size_t) (off - 1);
    }
    return;
  } catch (std::exception&) {
    printf("postprocessBuffer() failed\n");
    *(ctx->stopTeam) = 1;
  }
}


void FreadReader::orderBuffer(FreadLocalParseContext *ctx)
{
  try {
    size_t colCount = ctx->obuf_ncols;
    StrBuf* ctx_strbufs = ctx->strbufs;
    for (int k = 0; k < nstrcols; ++k) {
      int j = ctx_strbufs[k].idxdt;
      size_t j8 = static_cast<size_t>(ctx_strbufs[k].idx8);
      StrBuf* sb = strbufs[j];
      // Compute `sz` (the size of the string content in the buffer) from the
      // offset of the last element. Typically this would be the same as
      // `ctx_strbufs[k].ptr`, however in rare cases when `used_nrows` have changed
      // from the time the buffer was post-processed, this may be different.
      RelStr lastElem = ctx->obuf[j8 + colCount * (ctx->used_nrows - 1)].str32;
      size_t sz = static_cast<size_t>(abs(lastElem.offset) - 1);
      size_t ptr = sb->ptr;
      MemoryBuffer* sb_mbuf = sb->mbuf;
      // If we need to write more than the size of the available buffer, the
      // buffer has to grow. Check documentation for `StrBuf.numuses` in
      // `py_fread.h`.
      while (ptr + sz > sb_mbuf->size()) {
        size_t newsize = (ptr + sz) * 2;
        int old = 0;
        // (1) wait until no other process is writing into the buffer
        while (sb->numuses > 0)
          /* wait until .numuses == 0 (all threads finished writing) */;
        // (2) make `numuses` negative, indicating that no other thread may
        // initiate a memcopy operation for now.
        #pragma omp atomic capture
        {
          old = sb->numuses;
          sb->numuses -= 1000000;
        }
        // (3) The only case when `old != 0` is if another thread started
        // memcopy operation in-between statements (1) and (2) above. In
        // that case we restore the previous value of `numuses` and repeat
        // the loop.
        // Otherwise (and it is the most common case) we reallocate the
        // buffer and only then restore the `numuses` variable.
        if (old == 0) {
          sb_mbuf->resize(newsize);
        }
        #pragma omp atomic update
        sb->numuses += 1000000;
      }
      ctx_strbufs[k].ptr = ptr;
      sb->ptr = ptr + sz;
    }
    return;
  } catch (std::exception&) {
    printf("orderBuffer() failed");
    *(ctx->stopTeam) = 1;
  }
}


void FreadReader::pushBuffer(FreadLocalParseContext *ctx)
{
  StrBuf *__restrict__ ctx_strbufs = ctx->strbufs;
  const field64* __restrict__ obuf = ctx->obuf;
  int nrows = (int) ctx->used_nrows;
  size_t row0 = ctx->row0;

  int i = 0;    // index within the `types` and `sizes`
  int j = 0;    // index within `dt->columns`, `obuf` and `strbufs`
  int off = 0;
  int rowCount8 = (int) ctx->obuf_ncols;
  int rowCount4 = (int) ctx->obuf_ncols * 2;
  int rowCount1 = (int) ctx->obuf_ncols * 8;

  int k = 0;
  for (; i < ncols; i++) {
    if (types[i] == CT_DROP) continue;
    Column *col = dt->columns[j];

    if (types[i] == CT_STRING) {
      StrBuf *sb = strbufs[j];
      int idx8 = ctx_strbufs[k].idx8;
      size_t ptr = ctx_strbufs[k].ptr;
      const RelStr *__restrict__ lo = (const RelStr*)(obuf + idx8);
      size_t sz = (size_t) abs(lo[(nrows - 1)*rowCount8].offset) - 1;

      int done = 0;
      while (!done) {
        int old;
        #pragma omp atomic capture
        old = sb->numuses++;
        if (old >= 0) {
          memcpy(sb->mbuf->at(ptr), ctx_strbufs[k].mbuf->get(), sz);
          done = 1;
        }
        #pragma omp atomic update
        sb->numuses--;
      }

      int32_t* dest = ((int32_t*) col->data()) + row0 + 1;
      int32_t iptr = (int32_t) ptr;
      for (int n = 0; n < nrows; n++) {
        int32_t soff = lo->offset;
        *dest++ = (soff < 0)? soff - iptr : soff + iptr;
        lo += rowCount8;
      }
      k++;

    } else if (types[i] > 0) {
      int8_t elemsize = sizes[i];
      if (elemsize == 8) {
        const uint64_t* src = ((const uint64_t*) obuf) + off;
        uint64_t* dest = ((uint64_t*) col->data()) + row0;
        for (int r = 0; r < nrows; r++) {
          *dest = *src;
          src += rowCount8;
          dest++;
        }
      } else
      if (elemsize == 4) {
        const uint32_t* src = ((const uint32_t*) obuf) + off * 2;
        uint32_t* dest = ((uint32_t*) col->data()) + row0;
        for (int r = 0; r < nrows; r++) {
          *dest = *src;
          src += rowCount4;
          dest++;
        }
      } else
      if (elemsize == 1) {
        const uint8_t* src = ((const uint8_t*) obuf) + off * 8;
        uint8_t* dest = ((uint8_t*) col->data()) + row0;
        for (int r = 0; r < nrows; r++) {
          *dest = *src;
          src += rowCount1;
          dest++;
        }
      }
    }
    j++;
    off += (sizes[i] > 0);
  }
}


void FreadReader::progress(double percent/*[0,100]*/) {
  g.pyreader().invoke("_progress", "(d)", percent);
}



//------------------------------------------------------------------------------
// FreadLocalParseContext
//------------------------------------------------------------------------------

FreadLocalParseContext::FreadLocalParseContext(
    size_t brows, size_t bcols, FreadReader& f, bool* stopTeam_)
{
  anchor = nullptr;
  obuf = nullptr;
  obuf_nrows = brows;
  obuf_ncols = bcols;
  used_nrows = 0;
  row0 = 0;

  stopTeam = stopTeam_;
  quote = f.quote;
  quoteRule = f.quoteRule;
  obuf = (field64*) malloc((brows * bcols + 1) * 8);
  if (!obuf) *stopTeam = true;
}


FreadLocalParseContext::~FreadLocalParseContext() {
  free(obuf);
  if (strbufs) {
    for (int k = 0; k < nstrcols; k++) {
      if (strbufs[k].mbuf)
        strbufs[k].mbuf->release();
    }
    free(strbufs);
  }
}



//------------------------------------------------------------------------------
// FieldParseContext
//------------------------------------------------------------------------------
void parse_string(FieldParseContext&);


/**
 * skip_eol() is used to consume a "newline" token from the current parsing
 * location (`ch`). Specifically,
 *   (1) if there is a newline sequence at the current parsing position, this
 *       function advances the parsing position past the newline and returns
 *       true;
 *   (2) otherwise it returns false and the current parsing location remains
 *       unchanged.
 *
 * We recognize the following sequences as newlines (where "LF" is byte 0x0A
 * or '\\n', and "CR" is 0x0D or '\\r'):
 *     CR CR LF
 *     CR LF
 *     LF CR
 *     LF
 *     CR  (only if `LFpresent` is false)
 *
 * Here LF and CR-LF are the most commonly used line endings, while LF-CR and
 * CR are encountered much less frequently. The sequence CR-CR-LF is not
 * usually recognized as a single newline by most text editors. However we find
 * that occasionally a file with CR-LF endings gets recoded into CR-CR-LF line
 * endings by buggy software.
 *
 * In addition, CR (\\r) is treated specially: it is considered a newline only
 * when `LFpresent` is false. This is because it is common to find files created
 * by programs that don't account for '\\r's and fail to quote fields containing
 * these characters. If we were to treat these '\\r's as newlines, the data
 * would be parsed incorrectly. On the other hand, there are files where '\\r's
 * are used as valid newlines. In order to handle both of these cases, we
 * introduce parameter `LFpresent` which is set to true if there is any '\\n'
 * found in the file, in which case a standalone '\\r' will not be considered a
 * newline.
 */
bool FieldParseContext::skip_eol() {
  // we call eol() when we expect to be at a newline, so optimize as if we are
  // at the end of line.
  if (*ch == '\n') {       // '\n\r' or '\n'
    ch += 1 + (ch[1] == '\r');
    return true;
  }
  if (*ch == '\r') {
    if (ch[1] == '\n') {   // '\r\n'
      ch += 2;
      return true;
    }
    if (ch[1] == '\r' && ch[2] == '\n') {  // '\r\r\n'
      ch += 3;
      return true;
    }
    if (!LFpresent) {      // '\r'
      ch++;
      return true;
    }
  }
  return false;
}


/**
 * Return True iff `ch` is a valid field terminator character: either a field
 * separator or a newline.
 */
bool FieldParseContext::end_of_field() {
  // \r is 13, \n is 10, and \0 is 0. The second part is optimized based on the
  // fact that the characters in the ASCII range 0..13 are very rare, so a
  // single check `tch<=13` is almost equivalent to checking whether `tch` is one
  // of \r, \n, \0. We cast to unsigned first because `char` type is signed by
  // default, and therefore characters in the range 0x80-0xFF are negative.
  // We use eol() because that looks at LFpresent inside it w.r.t. \r
  char c = *ch;
  if (c == sep) return true;
  if (static_cast<uint8_t>(c) > 13) return false;
  if (c == '\n' || c == '\0') return true;
  if (c == '\r') {
    if (LFpresent) {
      const char* tch = ch + 1;
      while (*tch == '\r') tch++;
      if (*tch == '\n') return true;
    } else {
      return true;
    }
  }
  return false;
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
  if (skip_eol() || ch==eof) {
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
    if (skip_eol()) {
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


bool FieldParseContext::nextGoodLine(int ncol, bool fill, bool skip_blank_lines) {
  const char* ch0 = ch;
  // we may have landed inside quoted field containing embedded sep and/or embedded \n
  // find next \n and see if 5 good lines follow. If not try next \n, and so on, until we find the real \n
  // We don't know which line number this is, either, because we jumped straight to it. So return true/false for
  // the line number and error message to be worked out up there.
  int attempts = 0;
  while (ch < eof && attempts++<30) {
    while (*ch!='\0' && *ch!='\n' && *ch!='\r') ch++;
    if (*ch=='\0') return false;
    skip_eol();  // move to the first byte of the next line
    int i = 0;
    // countfields() below moves the parse location, so store it in `ch1` in
    // order to revert to the current parsing location later.
    const char* ch1 = ch;
    for (; i < 5; ++i) {
      int n = countfields();  // advances `ch` to the beginning of the next line
      if (n != ncol &&
          !(ncol == 1 && n == 0) &&
          !(skip_blank_lines && n == 0) &&
          !(fill && n < ncol)) break;
    }
    ch = ch1;
    // `i` is the count of consecutive consistent rows
    if (i == 5) break;
  }
  if (*ch!='\0' && attempts<30) return true;
  ch = ch0;
  return false;
}
