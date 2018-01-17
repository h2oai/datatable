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
#include <Python.h>
#include <string.h>    // memcpy
#include <sys/mman.h>  // mmap
#include <exception>
#include "csv/reader.h"
#include "csv/reader_parsers.h"
#include "csv/fread.h"
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
// Initialization
//------------------------------------------------------------------------------

FreadReader::FreadReader(GenericReader& greader) : g(greader) {
  targetdir = nullptr;
  strbufs = nullptr;
  sizes = nullptr;
  eof = nullptr;
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
  free(sizes);
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
// Parsing steps
//------------------------------------------------------------------------------

/**
 * Parse a single line of input starting from location `ctx.ch` as strings,
 * and interpret them as column names. At the end of this function the parsing
 * location `ctx.ch` will be moved to the beginning of the next line.
 *
 * The column names will be stored in `columns[i].name` fields. If the number
 * of column names on the input line is greater than the number of `columns`,
 * then the `columns` array will be extended to accomodate extra columns. If
 * the number of column names is less than the number of allocated columns,
 * then the missing columns will retain their default empty names.
 *
 * This function assumes that the `quoteRule` and `quote` were already detected
 * correctly, so that `parse_string()` can parse each field without error. If
 * not, a `RuntimeError` will be thrown.
 */
void FreadReader::parse_column_names(FieldParseContext& ctx) {
  const char*& ch = ctx.ch;

  // Skip whitespace at the beginning of a line.
  if (stripWhite && (*ch == ' ' || (*ch == '\t' && sep != '\t'))) {
    while (*ch == ' ' || *ch == '\t') ch++;
  }

  uint8_t echar = quoteRule == 0? static_cast<uint8_t>(quote) :
                  quoteRule == 1? '\\' : 0xFF;

  size_t ncols = columns.size();
  size_t ncols_found;
  for (size_t i = 0; ; ++i) {
    // Parse string field, but do not advance `ctx.target`: on the next
    // iteration we will write into the same place.
    parse_string(ctx);
    const char* start = ctx.anchor + ctx.target->str32.offset;
    size_t length = static_cast<size_t>(ctx.target->str32.length);

    if (i >= ncols) {
      columns.push_back(GReaderOutputColumn());
    }
    if (length > 0) {
      const uint8_t* usrc = reinterpret_cast<const uint8_t*>(start);
      int res = check_escaped_string(usrc, length, echar);
      if (res == 0) {
        columns[i].name = std::string(start, length);
      } else {
        char* newsrc = new char[length * 4];
        uint8_t* unewsrc = reinterpret_cast<uint8_t*>(newsrc);
        int newlen;
        if (res == 1) {
          newlen = decode_escaped_csv_string(usrc, length, unewsrc, echar);
        } else {
          newlen = decode_win1252(usrc, length, unewsrc);
          newlen = decode_escaped_csv_string(unewsrc, newlen, unewsrc, echar);
        }
        assert(newlen > 0);
        columns[i].name = std::string(newsrc, newlen);
        delete[] newsrc;
      }
    }
    // Skip the separator, handling special case of sep=' ' (multiple spaces are
    // treated as a single separator, and spaces at the beginning/end of line
    // are ignored).
    if (ch < eof && sep == ' ' && *ch == ' ') {
      while (ch < eof && *ch == ' ') ch++;
      if (ch == eof || ctx.skip_eol()) {
        ncols_found = i + 1;
        break;
      }
    } else if (ch < eof && *ch == sep && sep != '\n') {
      ch++;
    } else if (ch == eof || ctx.skip_eol()) {
      ncols_found = i + 1;
      break;
    } else {
      throw RuntimeError() << "Internal error: cannot parse column names";
    }
  }

  if (sep == ' ' && ncols_found == ncols - 1) {
    for (size_t j = ncols - 1; j > 0; j--){
      columns[j].name.swap(columns[j-1].name);
    }
    columns[0].name = "index";
  }
}



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



void FreadReader::userOverride()
{
  int ncols = columns.size();
  PyObject* colNamesList = PyList_New(ncols);
  PyObject* colTypesList = PyList_New(ncols);
  for (int i = 0; i < ncols; i++) {
    const char* src = columns[i].name.data();
    size_t len = columns[i].name.size();
    PyObject* pycol = len > 0? PyUnicode_FromStringAndSize(src, len)
                             : PyUnicode_FromFormat("V%d", i);
    PyObject* pytype = PyLong_FromLong(columns[i].type);
    PyList_SET_ITEM(colNamesList, i, pycol);
    PyList_SET_ITEM(colTypesList, i, pytype);
  }

  g.pyreader().invoke("_override_columns", "(OO)", colNamesList, colTypesList);

  for (int i = 0; i < ncols; i++) {
    PyObject* t = PyList_GET_ITEM(colTypesList, i);
    columns[i].type = (int8_t) PyLong_AsUnsignedLongMask(t);
  }
  pyfree(colTypesList);
  pyfree(colNamesList);
}


/**
 * Allocate memory for the DataTable that is being constructed.
 */
size_t FreadReader::allocateDT()
{
  Column** ccolumns = NULL;
  nstrcols = 0;
  int ncols = (int) columns.size();

  // First we need to estimate the size of the dataset that needs to be
  // created. However this needs to be done on first run only.
  // Also in this block we compute: `nstrcols` (will be used later in
  // `prepareLocalParseContext` and `postprocessBuffer`), as well as allocating
  // the `Column**` array.
  if (!dt) {
    size_t alloc_size = 0;
    int i, j;
    for (i = j = 0; i < ncols; i++) {
      if (columns[i].type == CT_DROP) continue;
      nstrcols += (columns[i].type == CT_STRING);
      SType stype = colType_to_stype[columns[i].type];
      alloc_size += stype_info[stype].elemsize * allocnrow;
      if (columns[i].type == CT_STRING) alloc_size += 5 * allocnrow;
      j++;
    }
    dtcalloc_g(ccolumns, Column*, j + 1);
    dtcalloc_g(strbufs, StrBuf*, j);
    ccolumns[j] = NULL;

    // Call the Python upstream to determine the strategy where the
    // DataTable should be created.
    targetdir = g.pyreader().invoke("_get_destination", "(n)", alloc_size)
                 .as_ccstring();
  } else {
    ccolumns = dt->columns;
    for (int i = 0; i < ncols; i++) {
      if (columns[i].typeBumped) continue;
      nstrcols += (columns[i].type == CT_STRING);
    }
  }

  // Compute number of digits in `ncols` (needed for creating file names).
  if (targetdir) {
    ndigits = 0;
    for (int nc = ncols; nc; nc /= 10) ndigits++;
  }

  // Create individual columns
  for (int i = 0, j = 0; i < ncols; i++) {
    int8_t type = columns[i].type;
    if (type == CT_DROP) continue;
    if (!columns[i].typeBumped) {
      SType stype = colType_to_stype[type];
      ccolumns[j] = realloc_column(ccolumns[j], stype, allocnrow, j);
      if (ccolumns[j] == NULL) goto fail;
    }
    j++;
  }

  if (!dt) {
    dt.reset(new DataTable(ccolumns));
  }
  return 1;

  fail:
  if (ccolumns) {
    Column **col = ccolumns;
    while (*col++) delete (*col);
    dtfree(ccolumns);
  }
  throw RuntimeError() << "Unable to allocate DataTable";
}



void FreadReader::setFinalNrow(size_t nrows) {
  int i, j;
  int ncols = columns.size();
  for (i = j = 0; i < ncols; i++) {
    int type = columns[i].type;
    if (type == CT_DROP) continue;
    Column* col = dt->columns[j];
    if (columns[i].typeBumped) {
      // do nothing
    } else if (type == CT_STRING) {
      StrBuf* sb = strbufs[j];
      assert(sb->numuses == 0);
      sb->mbuf->resize(sb->ptr);
      sb->mbuf = nullptr; // MemoryBuffer is also pointed to by the column
      col->mbuf->resize(sizeof(int32_t) * (nrows + 1));
      col->nrows = static_cast<int64_t>(nrows);
    } else {
      Column *c = realloc_column(col, colType_to_stype[type], nrows, j);
      if (c == nullptr) throw Error() << "Could not reallocate column";
    }
    j++;
  }
  dt->nrows = (int64_t) nrows;
}



void FreadReader::progress(double percent/*[0,100]*/) {
  g.pyreader().invoke("_progress", "(d)", percent);
}



//------------------------------------------------------------------------------
// FreadLocalParseContext
//------------------------------------------------------------------------------

FreadLocalParseContext::FreadLocalParseContext(
    size_t bcols, size_t brows, FreadReader& f
  ) : LocalParseContext(bcols, brows), ncols(f.columns.size()),
      ostrbufs(f.strbufs), sizes(f.sizes), dt(f.dt), columns(f.columns)
{
  anchor = nullptr;
  strbufs = nullptr;
  quote = f.quote;
  quoteRule = f.quoteRule;
  nstrcols = f.nstrcols;
  strbufs = new StrBuf[nstrcols]();
  for (int i = 0, j = 0, k = 0, off8 = 0; i < ncols; i++) {
    if (columns[i].type == CT_DROP) continue;
    if (columns[i].type == CT_STRING && !columns[i].typeBumped) {
      assert(k < nstrcols);
      strbufs[k].mbuf = new MemoryMemBuf(4096);
      strbufs[k].ptr = 0;
      strbufs[k].idx8 = off8;
      strbufs[k].idxdt = j;
      k++;
    }
    off8 += (f.sizes[i] > 0);
    j++;
  }
}


FreadLocalParseContext::~FreadLocalParseContext() {
  if (strbufs) {
    for (int k = 0; k < nstrcols; k++) {
      if (strbufs[k].mbuf)
        strbufs[k].mbuf->release();
    }
    free(strbufs);
  }
}


void FreadLocalParseContext::push_buffers() {}
const char* FreadLocalParseContext::read_chunk(const char* start, const char* end) {}


void FreadLocalParseContext::postprocess() {
  const uint8_t* zanchor = reinterpret_cast<const uint8_t*>(anchor);
  uint8_t echar = quoteRule == 0? static_cast<uint8_t>(quote) :
                  quoteRule == 1? '\\' : 0xFF;
  for (int k = 0; k < nstrcols; ++k) {
    MemoryBuffer* strdest = strbufs[k].mbuf;
    field64* lo = tbuf + strbufs[k].idx8;
    int32_t off = 1;
    size_t bufsize = strbufs[k].mbuf->size();
    for (size_t n = 0; n < used_nrows; n++) {
      int32_t len = lo->str32.length;
      if (len > 0) {
        size_t zlen = static_cast<size_t>(len);
        size_t zoff = static_cast<size_t>(off);
        if (bufsize < zlen * 3 + zoff) {
          bufsize = bufsize * 2 + zlen * 3;
          strdest->resize(bufsize);
        }
        const uint8_t* src = zanchor + lo->str32.offset;
        uint8_t* dest = static_cast<uint8_t*>(strdest->at(off - 1));
        int res = check_escaped_string(src, zlen, echar);
        if (res == 0) {
          memcpy(dest, src, zlen);
          off += zlen;
          lo->str32.offset = off;
        } else if (res == 1) {
          int newlen = decode_escaped_csv_string(src, len, dest, echar);
          off += static_cast<size_t>(newlen);
          lo->str32.offset = off;
        } else {
          int newlen = decode_win1252(src, len, dest);
          assert(newlen > 0);
          newlen = decode_escaped_csv_string(dest, newlen, dest, echar);
          off += static_cast<size_t>(newlen);
          lo->str32.offset = off;
        }
      } else if (len == 0) {
        lo->str32.offset = off;
      } else {
        assert(lo->str32.isna());
        lo->str32.offset = -off;
      }
      lo += tbuf_ncols;
    }
    strbufs[k].ptr = static_cast<size_t>(off - 1);
  }
}


void FreadLocalParseContext::orderBuffer() {
  for (int k = 0; k < nstrcols; ++k) {
    int j = strbufs[k].idxdt;
    size_t j8 = static_cast<size_t>(strbufs[k].idx8);
    StrBuf* sb = ostrbufs[j];
    // Compute `sz` (the size of the string content in the buffer) from the
    // offset of the last element. Typically this would be the same as
    // `strbufs[k].ptr`, however in rare cases when `used_nrows` have changed
    // from the time the buffer was post-processed, this may be different.
    int32_t lastOffset = tbuf[j8 + tbuf_ncols * (used_nrows - 1)].str32.offset;
    size_t sz = static_cast<size_t>(abs(lastOffset) - 1);
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
    strbufs[k].ptr = ptr;
    sb->ptr = ptr + sz;
  }
}



void FreadLocalParseContext::pushBuffer() {
  int i = 0;    // index within the `types` and `sizes`
  int j = 0;    // index within `dt->columns`, `tbuf` and `strbufs`
  int k = 0;
  int off = 0;
  for (; i < ncols; i++) {
    if (columns[i].type == CT_DROP) continue;
    Column* col = dt->columns[j];

    if (columns[i].typeBumped) {
      // do nothing
    } else if (columns[i].type == CT_STRING) {
      StrBuf* sb = ostrbufs[j];
      size_t ptr = strbufs[k].ptr;
      field64* lo = tbuf + strbufs[k].idx8;
      int32_t lastOffset = lo[(used_nrows - 1) * tbuf_ncols].str32.offset;
      size_t sz = static_cast<size_t>(abs(lastOffset)) - 1;

      int done = 0;
      while (!done) {
        int old;
        #pragma omp atomic capture
        old = sb->numuses++;
        if (old >= 0) {
          memcpy(sb->mbuf->at(ptr), strbufs[k].mbuf->get(), sz);
          done = 1;
        }
        #pragma omp atomic update
        sb->numuses--;
      }

      int32_t* dest = ((int32_t*) col->data()) + row0 + 1;
      int32_t iptr = (int32_t) ptr;
      for (size_t n = 0; n < used_nrows; n++) {
        int32_t soff = lo->str32.offset;
        *dest++ = (soff < 0)? soff - iptr : soff + iptr;
        lo += tbuf_ncols;
      }
      k++;

    } else {
      int8_t elemsize = sizes[i];
      const field64* src = tbuf + off;
      if (elemsize == 8) {
        uint64_t* dest = static_cast<uint64_t*>(col->data()) + row0;
        for (size_t r = 0; r < used_nrows; r++) {
          *dest = src->uint64;
          src += tbuf_ncols;
          dest++;
        }
      } else
      if (elemsize == 4) {
        uint32_t* dest = static_cast<uint32_t*>(col->data()) + row0;
        for (size_t r = 0; r < used_nrows; r++) {
          *dest = src->uint32;
          src += tbuf_ncols;
          dest++;
        }
      } else
      if (elemsize == 1) {
        uint8_t* dest = static_cast<uint8_t*>(col->data()) + row0;
        for (size_t r = 0; r < used_nrows; r++) {
          *dest = src->uint8;
          src += tbuf_ncols;
          dest++;
        }
      }
    }
    j++;
    off += (sizes[i] > 0);
  }
}



//==============================================================================
// FieldParseContext
//==============================================================================
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
    // Field() leaves *ch resting on sep, eol or eof
    if (*ch == sep) {
      if (sep == ' ') {
        while (*ch == ' ') ch++;
        if (ch == eof || skip_eol()) break;
        ncol++;
        continue;
      } else if (sep != '\n') {
        ch++;
        ncol++;
        continue;
      }
    }
    if (ch == eof || skip_eol()) {
      break;
    }
    ch = ch0;
    return -1;  // -1 means this line not valid for this sep and quote rule
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
